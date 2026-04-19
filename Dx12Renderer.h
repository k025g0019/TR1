#pragma once

#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <vector>

#include "SharedTypes.h"

class Dx12Renderer {
public:
    ~Dx12Renderer();

    void Initialize(HWND hwnd, UINT width, UINT height);
    void UploadVertices(const std::vector<Vertex>& vertices);
    void Render(UINT vertexCount);

private:
    static constexpr UINT kFrameCount = 2;
    static constexpr size_t kMaxVertices = 262144;

    void CreateFactoryAndDevice();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateRenderTargetViews();
    void CreatePipeline();
    void CreateVertexBuffer();
    void CreateFence();

    void WaitForGpu();
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRenderTargetView() const;

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;

    UINT frameIndex_ = 0;
    UINT rtvDescriptorSize_ = 0;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    Vertex* mappedVertexData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
};
