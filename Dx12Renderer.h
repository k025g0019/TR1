#pragma once

#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <vector>

#include "SharedTypes.h"

//========================================
// Dx12Renderer クラス宣言
//========================================
// 画面初期化、頂点アップロード、1 フレーム描画を担当する DirectX12 ラッパーです。

class Dx12Renderer {
public:
    //========================================
    // 公開操作
    //========================================

    /* Map したバッファと同期イベントを安全に解放します。 */
    ~Dx12Renderer();

    /* Device 生成から PSO 構築まで、描画に必要な DirectX12 オブジェクトをそろえます。 */
    void Initialize(HWND hwnd, UINT width, UINT height);

    /* CPU で組んだ頂点列を upload heap へコピーし、描画ビューを更新します。 */
    void UploadVertices(const std::vector<Vertex>& vertices);

    /* コマンド記録から Present までを 1 フレーム分まとめて実行します。 */
    void Render(UINT vertexCount);

private:
    //========================================
    // 固定設定
    //========================================

    /* Present と描画を交互に回しやすい、最小構成のダブルバッファです。 */
    static constexpr UINT kFrameCount = 2;

    /* 毎フレーム再確保しないよう、十分大きい頂点バッファを最初に確保します。 */
    static constexpr size_t kMaxVertices = 262144;

    //========================================
    // 初期化工程
    //========================================

    /* DXGI ファクトリを作り、使える GPU から D3D12 Device を確立します。 */
    void CreateFactoryAndDevice();

    /* コマンドキュー、アロケータ、コマンドリストをそろえます。 */
    void CreateCommandObjects();

    /* ウィンドウ表示用のバックバッファ列を作ります。 */
    void CreateSwapChain();

    /* 各バックバッファへ描き込むための RTV を割り当てます。 */
    void CreateRenderTargetViews();

    /* シェーダーをコンパイルし、固定機能設定と合わせて PSO を作ります。 */
    void CreatePipeline();

    /* 毎フレーム使い回す upload heap の頂点バッファを確保します。 */
    void CreateVertexBuffer();

    /* Present 後に GPU 完了を待つためのフェンスとイベントを作ります。 */
    void CreateFence();

    //========================================
    // 描画補助
    //========================================

    /* 直前フレームの GPU 実行完了を待ち、次のフレームで安全に再利用できるようにします。 */
    void WaitForGpu();

    /* 現在の frameIndex_ が指すバックバッファ用 RTV ハンドルを計算します。 */
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRenderTargetView() const;

    //========================================
    // 基本情報
    //========================================

    /* Present 先になる Win32 ウィンドウです。 */
    HWND hwnd_ = nullptr;

    /* ビューポートとスワップチェーンに使う描画幅です。 */
    UINT width_ = 0;

    /* ビューポートとスワップチェーンに使う描画高さです。 */
    UINT height_ = 0;

    //========================================
    // 実行時状態
    //========================================

    /* 今から描く先のバックバッファ番号です。Present ごとに切り替わります。 */
    UINT frameIndex_ = 0;

    /* RTV ヒープ内で次ハンドルへ進むためのサイズです。 */
    UINT rtvDescriptorSize_ = 0;

    /* 各フレーム送信後に GPU 完了待ちへ使う単調増加カウンタです。 */
    UINT64 fenceValue_ = 0;

    /* フェンス完了時に OS が通知するイベントハンドルです。 */
    HANDLE fenceEvent_ = nullptr;

    /* upload heap を Map したまま保持する CPU 書き込み先です。 */
    Vertex* mappedVertexData_ = nullptr;

    /* 今回描く頂点数に合わせて毎フレーム更新する IA 用ビューです。 */
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

    //========================================
    // DirectX オブジェクト
    //========================================

    /* DXGI オブジェクト群です。 */
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;

    /* GPU 本体へリソース生成やコマンド作成を依頼する窓口です。 */
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    /* 記録済みコマンド列を GPU へ送るキューです。 */
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

    /* 画面表示用のバックバッファ列です。 */
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

    /* バックバッファ用 RTV を並べるディスクリプタヒープです。 */
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;

    /* 実際のバックバッファリソース群です。 */
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[kFrameCount];

    /* フレームごとのコマンド記録領域です。 */
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;

    /* 描画コマンドを書き込むリストです。 */
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    /* GPU 完了を CPU 側が待つための同期オブジェクトです。 */
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

    /* このアプリの単純な頂点色描画に必要なルートシグネチャです。 */
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    /* シェーダーと固定機能設定を束ねた描画パイプライン状態です。 */
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    /* 毎フレーム CPU から直接書き込む頂点バッファです。 */
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
};
