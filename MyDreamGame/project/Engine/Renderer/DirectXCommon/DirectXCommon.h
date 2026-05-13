#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <Windows.h>
#include <string>
#include <chrono>
#include "Core/Utility/UtilityFunctions.h"

class DirectXCommon {
public:
    static DirectXCommon *GetInstance();
    
    enum class PostEffect {
        kNone,
        kGrayscale,
        kSepia,
        kVignette,
    };

    struct VignetteParams {
        float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float scale = 16.0f;
        float power = 0.8f;
    };

	// 初期化処理
	void Initialize(HWND hwnd,int32_t windowWidth,int32_t windowHeight);

	// 終了処理
	void Finalize();

	// 描画前処理
	void PreDraw();

	// 描画後処理
	void PostDraw();

	// 描画後処理を分割して提供
    void ExecuteCommands(); // コマンドを閉じて実行する
    void Present();         // 画面に表示して次フレームの準備をする

	// ImGuiを描く直前に、描画先をSwapchainに戻すための関数
    void PreDrawSwapchain();

	// RenderTextureを初期化する専用関数
    void InitializeRenderTexture();
    
    // ポストエフェクトを実行する関数 (RenderTexture -> PostProcessTexture)
    void ExecutePostEffect();

	// RenderTextureの内容を現在の画面にコピーして描画する関数
    void DrawRenderTexture();

	void CreateSkyboxPipeline();
    
    // ポストエフェクトの設定
    void SetPostEffect(PostEffect effect) { postEffect_ = effect; }
    PostEffect GetPostEffect() const { return postEffect_; }

	// ゲッター関数
	ID3D12Device *GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList *GetCommandList() const { return commandList_.Get(); }
	DXGI_SWAP_CHAIN_DESC1 GetSwapChainDesc() const { return swapChainDesc_; }
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() const { return rtvDesc_; }
	ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }
	ID3D12PipelineState *GetGraphicsPipelineState() const { return graphicsPipelineState_.Get(); }
	ID3D12PipelineState *GetGraphicsPipelineStateNoCull() const { return graphicsPipelineStateNoCull_.Get(); }
	ID3D12PipelineState *GetGraphicsPipelineStateAdditive() const { return graphicsPipelineStateAdditive_.Get(); }
	ID3D12PipelineState *GetGraphicsPipelineStateNoCullAdditive() const { return graphicsPipelineStateNoCullAdditive_.Get(); }
	ID3D12PipelineState *GetGraphicsPipelineStateTransparent() const { return graphicsPipelineStateTransparent_.Get(); }
	ID3D12PipelineState *GetSpritePipelineState() const { return spritePipelineState_.Get(); }
    ID3D12CommandQueue *GetCommandQueue() const { return commandQueue_.Get(); }
    IDxcUtils *GetDxcUtils() const { return dxcUtils_.Get(); }
    IDxcCompiler3 *GetDxcCompiler() const { return dxcCompiler_.Get(); }
    IDxcIncludeHandler *GetIncludeHandler() const { return includeHandler_.Get(); }
    ID3D12RootSignature *GetCopyImageRootSignature() const { return copyImageRootSignature_.Get(); }
    ID3D12PipelineState *GetCopyImagePipelineState() const { return copyImagePipelineState_.Get(); }
    ID3D12PipelineState *GetGrayscalePipelineState() const { return grayscalePipelineState_.Get(); }
    ID3D12PipelineState *GetSepiaPipelineState() const { return sepiaPipelineState_.Get(); }
    ID3D12PipelineState *GetVignettePipelineState() const { return vignettePipelineState_.Get(); }
    ID3D12RootSignature *GetSkyboxRootSignature() const { return skyboxRootSignature_.Get(); }
    ID3D12PipelineState *GetSkyboxPipelineState() const { return skyboxPipelineState_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetRenderTextureSrvHandleGPU() const { return renderTextureSrvHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetPostProcessSrvHandleGPU() const { return postProcessSrvHandleGPU_; }
    VignetteParams* GetVignetteParamsData() { return vignetteParamsData_; }

private:
	// DirectXのインスタンス作成
	void CreateDxInstance();
	// スワップチェーンとRTV(描画先)の作成
	void CreateFinalRenderTargets();
	// パイプラインステートオブジェクト(描画ルール)の作成
	void CreatePipelines();
    // ポストエフェクト専用のパイプライン作成関数
	void CreatePostEffectPipelines();
	// FPS固定初期化
	void InitializeFixFPS();
	// FPS固定更新
	void UpdateFixFPS();

private:
	// ★ インスタンスを保持する静的変数
    static DirectXCommon *instance_;

	// ウィンドウハンドル
	HWND hwnd_ = nullptr;
	// ウィンドウサイズ
	int32_t windowWidth_ = 0;
	int32_t windowHeight_ = 0;

	// DirectX関連のメンバ変数
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2];
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateNoCull_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateAdditive_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateNoCullAdditive_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateTransparent_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spritePipelineState_;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

	// RenderTexture関連の変数
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRtvHandle_{};    // RTV用
    D3D12_CPU_DESCRIPTOR_HANDLE renderTextureSrvHandleCPU_{}; // SRV用 (CPU)
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandleGPU_{}; // SRV用 (GPU)

    // ポストプロセス用のリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> postProcessResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE postProcessRtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE postProcessSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE postProcessSrvHandleGPU_{};

	Microsoft::WRL::ComPtr<ID3D12RootSignature> copyImageRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyImagePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> sepiaPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vignetteParamResource_;
    VignetteParams* vignetteParamsData_ = nullptr;

    PostEffect postEffect_ = PostEffect::kNone;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skyboxRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skyboxPipelineState_;
	
	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_;

	// RTVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2];

	// 現在のバックバッファインデックス
	UINT backBufferIndex_ = 0;

	// 記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;

};

