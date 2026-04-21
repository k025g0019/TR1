#include "Dx12Renderer.h"

#include <d3dcompiler.h>

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

//========================================
// Dx12Renderer 実装
//========================================
// DirectX12 の初期化、頂点アップロード、画面提示までを順番に担当します。
// このアプリでは 2D 頂点列を描くだけなので、パイプラインはかなり絞った構成です。

namespace {

//========================================
// ローカル補助関数
//========================================

/* 例外送出 */
[[noreturn]] void ThrowWithMessage(const std::string& message) {
    throw std::runtime_error(message);
}

/* HRESULT チェック */
void ThrowIfFailed(HRESULT hr, const char* context) {
    if (SUCCEEDED(hr)) {
        return;
    }

    std::ostringstream stream;
    stream << context << " failed. HRESULT=0x" << std::hex
           << static_cast<unsigned long>(hr);
    ThrowWithMessage(stream.str());
}

/* リソース遷移バリア生成 */
D3D12_RESOURCE_BARRIER MakeTransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    /* Present 用と RenderTarget 用の状態を毎フレーム切り替えるので、生成を共通化します。 */
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

/* ヒープ設定生成 */
D3D12_HEAP_PROPERTIES MakeHeapProperties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES properties = {};
    properties.Type = type;
    properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

/* バッファ記述子生成 */
D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 sizeInBytes) {
    /* このアプリの頂点バッファは 1 次元の生メモリ領域なので、BUFFER 設定で固定します。 */
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

/* シェーダーエラー追記 */
void AppendBlobMessage(std::string& message, ID3DBlob* blob) {
    if (blob == nullptr) {
        return;
    }
    message += " ";
    message.append(
        static_cast<const char*>(blob->GetBufferPointer()),
        blob->GetBufferSize());
}

}  // namespace

//========================================
// ライフサイクル
//========================================

Dx12Renderer::~Dx12Renderer() {
    /* Map 済みバッファ解放 */
    if (vertexBuffer_ && mappedVertexData_ != nullptr) {
        D3D12_RANGE readRange = {0, 0};
        vertexBuffer_->Unmap(0, &readRange);
    }

    /* フェンスイベント解放 */
    if (fenceEvent_ != nullptr) {
        CloseHandle(fenceEvent_);
    }
}

void Dx12Renderer::Initialize(HWND hwnd, UINT width, UINT height) {
    //========================================
    // 基本情報保持
    //========================================

    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    //========================================
    // 初期化工程
    //========================================

    CreateFactoryAndDevice();
    CreateCommandObjects();
    CreateSwapChain();
    CreateRenderTargetViews();
    CreatePipeline();
}

//========================================
// 頂点アップロード
//========================================

void Dx12Renderer::UploadVertices(const std::vector<Vertex>& vertices) {
    /* 固定長 upload heap を使っているので、想定上限を超えた時点で明示的に止めます。 */
    if (vertices.size() > kMaxVertices) {
        ThrowWithMessage("vertex count exceeds upload buffer capacity");
    }

    /* CPU で組んだ頂点列を、そのまま Map 済みメモリへ連続コピーします。 */
    if (!vertices.empty()) {
        std::memcpy(
            mappedVertexData_,
            vertices.data(),
            vertices.size() * sizeof(Vertex));
    }

    /* 今回描く頂点数だけ IA が読むよう、ビューのサイズをその都度合わせます。 */
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.StrideInBytes = sizeof(Vertex);
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
}

//========================================
// 1 フレーム描画
//========================================

void Dx12Renderer::Render(UINT vertexCount) {
    //========================================
    // コマンドリスト準備
    //========================================

    ThrowIfFailed(commandAllocator_->Reset(), "ID3D12CommandAllocator::Reset");
    ThrowIfFailed(
        commandList_->Reset(commandAllocator_.Get(), pipelineState_.Get()),
        "ID3D12GraphicsCommandList::Reset");

    //========================================
    // 描画範囲設定
    //========================================

    /* ウィンドウ全体へ描くので、ビューポートとシザーは画面サイズそのままを使います。 */
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width_);
    scissorRect.bottom = static_cast<LONG>(height_);
    commandList_->RSSetScissorRects(1, &scissorRect);

    //========================================
    // パイプライン設定
    //========================================

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);

    //========================================
    // 描画開始前の状態遷移
    //========================================

    D3D12_RESOURCE_BARRIER toRender = MakeTransitionBarrier(
        renderTargets_[frameIndex_].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRender);

    //========================================
    // 画面クリア
    //========================================

    /* 今回のバックバッファを取得し、まずは背景色で全面クリアします。 */
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentRenderTargetView();
    constexpr float clearColor[] = {0.03f, 0.05f, 0.09f, 1.0f};
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    //========================================
    // 頂点描画
    //========================================

    if (vertexCount > 0) {
        commandList_->DrawInstanced(vertexCount, 1, 0, 0);
    }

    //========================================
    // 表示直前の状態遷移
    //========================================

    D3D12_RESOURCE_BARRIER toPresent = MakeTransitionBarrier(
        renderTargets_[frameIndex_].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

    //========================================
    // 実行と表示
    //========================================

    ThrowIfFailed(commandList_->Close(), "ID3D12GraphicsCommandList::Close");

    ID3D12CommandList* commandLists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, commandLists);

    ThrowIfFailed(swapChain_->Present(1, 0), "IDXGISwapChain::Present");
    WaitForGpu();
}

//========================================
// Device 初期化
//========================================

void Dx12Renderer::CreateFactoryAndDevice() {
    /* DXGI ファクトリ生成 */
    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2");

    //========================================
    // ハードウェアアダプタ探索
    //========================================

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
         factory_->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);

        /* ソフトウェアアダプタ除外 */
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            adapter.Reset();
            continue;
        }

        /* 最初に成功した Device を採用 */
        if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device_)))) {
            break;
        }
        adapter.Reset();
    }

    //========================================
    // WARP フォールバック
    //========================================

    if (!device_) {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), "EnumWarpAdapter");
        ThrowIfFailed(
            D3D12CreateDevice(
                warpAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device_)),
            "D3D12CreateDevice");
    }
}

//========================================
// コマンド系生成
//========================================

void Dx12Renderer::CreateCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    /* コマンドキュー */
    ThrowIfFailed(
        device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)),
        "ID3D12Device::CreateCommandQueue");

    /* コマンドアロケータ */
    ThrowIfFailed(
        device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&commandAllocator_)),
        "ID3D12Device::CreateCommandAllocator");

    /* コマンドリスト */
    ThrowIfFailed(
        device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocator_.Get(),
            nullptr,
            IID_PPV_ARGS(&commandList_)),
        "ID3D12Device::CreateCommandList");

    /* 初回利用まで閉じておく */
    ThrowIfFailed(commandList_->Close(), "ID3D12GraphicsCommandList::Close");
}

//========================================
// スワップチェーン生成
//========================================

void Dx12Renderer::CreateSwapChain() {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width_;
    swapChainDesc.Height = height_;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    /* まず汎用の SwapChain1 を作り、あとで必要な SwapChain3 へ昇格します。 */
    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(
        factory_->CreateSwapChainForHwnd(
            commandQueue_.Get(),
            hwnd_,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1),
        "IDXGIFactory4::CreateSwapChainForHwnd");

    /* Alt+Enter の既定動作は使わない */
    ThrowIfFailed(
        factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER),
        "IDXGIFactory4::MakeWindowAssociation");

    /* 実際に使う IDXGISwapChain3 へ昇格 */
    ThrowIfFailed(
        swapChain1.As(&swapChain_),
        "IDXGISwapChain1::QueryInterface");

    /* 現在のバックバッファ番号を保存 */
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

//========================================
// RTV 生成
//========================================

void Dx12Renderer::CreateRenderTargetViews() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = kFrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask = 0;

    /* RTV ヒープ生成 */
    ThrowIfFailed(
        device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)),
        "ID3D12Device::CreateDescriptorHeap");
    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    /* 各バックバッファに RTV を割り当てる */
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < kFrameCount; ++index) {
        ThrowIfFailed(
            swapChain_->GetBuffer(index, IID_PPV_ARGS(&renderTargets_[index])),
            "IDXGISwapChain3::GetBuffer");
        device_->CreateRenderTargetView(renderTargets_[index].Get(), nullptr, handle);
        handle.ptr += static_cast<SIZE_T>(rtvDescriptorSize_);
    }
}

//========================================
// パイプライン生成
//========================================

void Dx12Renderer::CreatePipeline() {
    //========================================
    // シェーダーソース
    //========================================

    static constexpr char kVertexShader[] = R"(
struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput main(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}
)";

    static constexpr char kPixelShader[] = R"(
struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
    return input.color;
}
)";

    //========================================
    // シェーダーコンパイル
    //========================================

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        kVertexShader,
        sizeof(kVertexShader) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        compileFlags,
        0,
        &vertexShader,
        &errorBlob);
    if (FAILED(hr)) {
        std::string message = "vertex shader compilation failed.";
        AppendBlobMessage(message, errorBlob.Get());
        ThrowWithMessage(message);
    }

    errorBlob.Reset();
    hr = D3DCompile(
        kPixelShader,
        sizeof(kPixelShader) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        compileFlags,
        0,
        &pixelShader,
        &errorBlob);
    if (FAILED(hr)) {
        std::string message = "pixel shader compilation failed.";
        AppendBlobMessage(message, errorBlob.Get());
        ThrowWithMessage(message);
    }

    //========================================
    // ルートシグネチャ
    //========================================

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 0;
    rootSignatureDesc.pParameters = nullptr;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rootSignatureBlob;
    errorBlob.Reset();
    ThrowIfFailed(
        D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &rootSignatureBlob,
            &errorBlob),
        "D3D12SerializeRootSignature");
    ThrowIfFailed(
        device_->CreateRootSignature(
            0,
            rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)),
        "ID3D12Device::CreateRootSignature");

    //========================================
    // 入力レイアウト
    //========================================

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    //========================================
    // 固定機能設定
    //========================================

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    auto& renderTargetBlend = blendDesc.RenderTarget[0];
    renderTargetBlend.BlendEnable = TRUE;
    renderTargetBlend.LogicOpEnable = FALSE;
    renderTargetBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    renderTargetBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    renderTargetBlend.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    renderTargetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTargetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
    renderTargetBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    //========================================
    // PSO 生成
    //========================================

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize(),
    };
    psoDesc.PS = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize(),
    };
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.InputLayout = {inputLayout, static_cast<UINT>(_countof(inputLayout))};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(
        device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_)),
        "ID3D12Device::CreateGraphicsPipelineState");

    //========================================
    // 付随リソース生成
    //========================================

    CreateVertexBuffer();
    CreateFence();
}

//========================================
// 頂点バッファ生成
//========================================

void Dx12Renderer::CreateVertexBuffer() {
    const UINT64 bufferSize = sizeof(Vertex) * kMaxVertices;

    /* upload heap 上に固定長バッファを取る */
    const D3D12_HEAP_PROPERTIES heapProperties =
        MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC bufferDesc = MakeBufferDesc(bufferSize);

    ThrowIfFailed(
        device_->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer_)),
        "ID3D12Device::CreateCommittedResource");

    /* 常時 Map して CPU から直接書き込める状態にする */
    D3D12_RANGE readRange = {0, 0};
    ThrowIfFailed(
        vertexBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&mappedVertexData_)),
        "ID3D12Resource::Map");

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.StrideInBytes = sizeof(Vertex);
    vertexBufferView_.SizeInBytes = 0;
}

//========================================
// フェンス生成
//========================================

void Dx12Renderer::CreateFence() {
    /* GPU 完了通知用フェンス */
    ThrowIfFailed(
        device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
        "ID3D12Device::CreateFence");
    fenceValue_ = 1;

    /* 待機イベント */
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent_ == nullptr) {
        ThrowWithMessage("CreateEvent failed.");
    }
}

//========================================
// GPU 同期
//========================================

void Dx12Renderer::WaitForGpu() {
    /* 現在フレーム終了をシグナル */
    const UINT64 signalValue = fenceValue_++;
    ThrowIfFailed(
        commandQueue_->Signal(fence_.Get(), signalValue),
        "ID3D12CommandQueue::Signal");

    /* GPU が追い付いていなければ待つ */
    if (fence_->GetCompletedValue() < signalValue) {
        ThrowIfFailed(
            fence_->SetEventOnCompletion(signalValue, fenceEvent_),
            "ID3D12Fence::SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    /* Present 後のバックバッファ番号を取り直す */
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

//========================================
// 現在 RTV 取得
//========================================

D3D12_CPU_DESCRIPTOR_HANDLE Dx12Renderer::CurrentRenderTargetView() const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(frameIndex_) * static_cast<SIZE_T>(rtvDescriptorSize_);
    return handle;
}
