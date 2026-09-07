#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <Windows.h>
#include <string>
#include <chrono>
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"

class DirectXCommon {
public:
    static DirectXCommon *GetInstance();
    
    enum class PostEffect {
        kNone,
        kGrayscale,
        kSepia,
        kVignette,
        kSmoothing,
        kGaussian,
        kComposite,
        kDepthBasedOutline,
        kCompositeAndOutline,
        kIris,
    };

    struct IrisParams {
        float center[2] = {0.5f, 0.5f}; // 基準中心座標 (UV: 0.0~1.0)
        float radius = 1.0f;             // 半径
        float smoothness = 0.01f;        // 境界のぼかし幅
        float maskColor[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // 外側の色
        int isIrisIn = 0;                // 0: Iris Out (閉じる), 1: Iris In (開く)
        float aspectRatio = 16.0f / 9.0f;// アスペクト比 (幅 / 高さ)
        float padding[2] = {};
    };

    struct VignetteParams {
        float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float scale = 16.0f;
        float power = 0.8f;
    };

    struct SmoothingParams {
        int kernelSize = 1;       // カーネルの半径 (1=3x3, 2=5x5, 3=7x7)
        float texelSize[2] = {};  // 1.0 / テクスチャ解像度
        float strength = 1.0f;    // スムージングの強度 (0=元画像, 1=完全にぼかす)
    };

    struct GaussianParams {
        float sigma = 2.0f;       // 標準偏差
        float texelSize[2] = {};  // 1.0 / テクスチャ解像度
        float padding[1];         // 16バイトアライメント用のパディング
    };

    struct CompositeParams {
        float grayscaleStrength = 0.0f;
        float sepiaStrength = 0.0f;
        int enableVignette = 0;
        float vignetteScale = 16.0f;

        float vignettePower = 0.8f;
        int blurType = 0; // 0: なし, 1: ボックスぼかし, 2: ガウスぼかし
        int boxBlurKernelSize = 1;
        float boxBlurStrength = 1.0f;

        float vignetteColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        float gaussianSigma = 2.0f;
        float texelSize[2] = {};
        float padding = 0.0f;

        // Radial Blur
        int enableRadialBlur = 0;
        float radialBlurCenter[2] = {0.5f, 0.5f};
        float radialBlurWidth = 0.0f;
        int32_t radialBlurSamples = 10;
        float radialPadding[3] = {}; // 16-byte alignment padding

        // Dissolve
        int enableDissolve = 0;
        float dissolveThreshold = 0.0f;
        float dissolveEdgeWidth = 0.03f;
        float dissolvePadding = 0.0f; // Align dissolveEdgeColor to 16 bytes

        float dissolveEdgeColor[3] = {1.0f, 0.4f, 0.3f};
        float dissolvePadding2 = 0.0f; // Align dissolveBgColor to 16 bytes

        float dissolveBgColor[3] = {0.0f, 0.0f, 0.0f};
        float dissolvePadding3 = 0.0f; // total size alignment (144 bytes total)

        // Noise (32 bytes aligned)
        int enableNoise = 0;
        float noiseStrength = 0.3f;
        int noiseBlendMode = 1; // 0: Normal, 1: Add, 2: Multiply, 3: Screen, 4: Overlay
        float noiseScale = 128.0f; // Default noise scale (grain size)

        float noiseTime = 0.0f;
        float noisePadding[3] = {};

        // Iris (48 bytes aligned)
        int enableIris = 0;
        float irisCenter[2] = {0.5f, 0.5f};
        float irisRadius = 1.0f;

        float irisSmoothness = 0.01f;
        int isIrisIn = 0; // 0: Iris Out, 1: Iris In
        float irisAspectRatio = 16.0f / 9.0f;
        float irisPadding1 = 0.0f;

        float irisMaskColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
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

    // シャドウパス等の後に描画先をRenderTextureに戻すための関数
    void RestoreMainRenderTarget();

	// RenderTextureを初期化する専用関数
    void InitializeRenderTexture();
    
    // ポストエフェクトを実行する関数 (RenderTexture -> PostProcessTexture)
    void ExecutePostEffect();

	// RenderTextureの内容を現在の画面にコピーして描画する関数
    void DrawRenderTexture();

	void CreateSkyboxPipeline();
    void InitializeShadowMap();
    void CreateShadowMapPipelines();
    
    // スワップチェーンのサイズ変更
    void ResizeSwapchain(int32_t width, int32_t height);
    
    // ポストエフェクトの設定
    void SetPostEffect(PostEffect effect) { postEffect_ = effect; }
    PostEffect GetPostEffect() const { return postEffect_; }
    bool IsPostEffectEnabled() const { return isPostEffectEnabled_; }
    void SetPostEffectEnabled(bool enabled) { isPostEffectEnabled_ = enabled; }
    bool IsDepthBasedOutlineEnabled() const { return isDepthBasedOutlineEnabled_; }
    void SetDepthBasedOutlineEnabled(bool enabled) { isDepthBasedOutlineEnabled_ = enabled; }
    int32_t GetWindowWidth() const { return windowWidth_; }
    int32_t GetWindowHeight() const { return windowHeight_; }

	// ゲッター関数
	ID3D12Device *GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList *GetCommandList() const { return commandList_.Get(); }
	DXGI_SWAP_CHAIN_DESC1 GetSwapChainDesc() const { return swapChainDesc_; }
	D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc() const { return rtvDesc_; }
	ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }
	ID3D12PipelineState *GetGraphicsPipelineState() const { return graphicsPipelineState_.Get(); }
	ID3D12RootSignature *GetSkinningRootSignature() const { return skinningRootSignature_.Get(); }
	ID3D12PipelineState *GetSkinningPipelineState() const { return skinningPipelineState_.Get(); }
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
    ID3D12PipelineState *GetSmoothingPipelineState() const { return smoothingPipelineState_.Get(); }
    ID3D12PipelineState *GetGaussianPipelineState() const { return gaussianPipelineState_.Get(); }
    ID3D12PipelineState *GetIrisPipelineState() const { return irisPipelineState_.Get(); }
    ID3D12RootSignature *GetSkyboxRootSignature() const { return skyboxRootSignature_.Get(); }
    ID3D12PipelineState *GetSkyboxPipelineState() const { return skyboxPipelineState_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetRenderTextureSrvHandleGPU() const { return renderTextureSrvHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetPostProcessSrvHandleGPU() const { return finalPostProcessSRVHandle_.ptr != 0 ? finalPostProcessSRVHandle_ : postProcessSrvHandleGPU_; }
    
    // シャドウマップ関連ゲッター
    ID3D12Resource* GetShadowMapResource() const { return shadowMapResource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetShadowMapDsvCPUHandle() const { return shadowMapDsvHandleCPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSrvHandleGPU() const { return shadowMapSrvHandleGPU_; }
    ID3D12RootSignature* GetShadowMapRootSignature() const { return shadowMapRootSignature_.Get(); }
    ID3D12PipelineState* GetShadowMapPipelineState() const { return shadowMapPipelineState_.Get(); }
    ID3D12PipelineState* GetShadowMapSkinningPipelineState() const { return shadowMapSkinningPipelineState_.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetShadowGlobalGPUAddress() const { return shadowGlobalParamResource_ ? shadowGlobalParamResource_->GetGPUVirtualAddress() : 0; }
    void SetShadowLightViewProjection(const Matrix4x4& lightVP) {
        if (shadowGlobalParamData_) {
            *shadowGlobalParamData_ = lightVP;
        }
    }
    const D3D12_VIEWPORT& GetShadowViewport() const { return shadowViewport_; }
    const D3D12_RECT& GetShadowScissorRect() const { return shadowScissorRect_; }
    VignetteParams* GetVignetteParamsData() { return vignetteParamsData_; }
    SmoothingParams* GetSmoothingParamsData() { return smoothingParamsData_; }
    GaussianParams* GetGaussianParamsData() { return gaussianParamsData_; }
    CompositeParams* GetCompositeParamsData() { return compositeParamsData_; }
    IrisParams* GetIrisParamsData() { return irisParamsData_; }

    void SetIrisCenter(float x, float y) {
        if (irisParamsData_) {
            irisParamsData_->center[0] = x;
            irisParamsData_->center[1] = y;
        }
        if (compositeParamsData_) {
            compositeParamsData_->irisCenter[0] = x;
            compositeParamsData_->irisCenter[1] = y;
        }
    }
    void SetIrisCenterFromWorldPos(const Vector3& worldPos, const Matrix4x4& viewProjectionMatrix) {
        Vector3 ndc = TransformFunctions::EulerTransform(worldPos, viewProjectionMatrix);
        float uvX = (ndc.x + 1.0f) * 0.5f;
        float uvY = (1.0f - ndc.y) * 0.5f;
        SetIrisCenter(uvX, uvY);
    }
    void SetIrisRadius(float radius) {
        if (irisParamsData_) {
            irisParamsData_->radius = radius;
        }
        if (compositeParamsData_) {
            compositeParamsData_->irisRadius = radius;
        }
    }
    void SetIrisIn(bool isIrisIn) {
        if (irisParamsData_) {
            irisParamsData_->isIrisIn = isIrisIn ? 1 : 0;
        }
        if (compositeParamsData_) {
            compositeParamsData_->isIrisIn = isIrisIn ? 1 : 0;
        }
    }
    void SetIrisSmoothness(float smoothness) {
        if (irisParamsData_) {
            irisParamsData_->smoothness = smoothness;
        }
        if (compositeParamsData_) {
            compositeParamsData_->irisSmoothness = smoothness;
        }
    }
    void SetIrisMaskColor(float r, float g, float b, float a = 1.0f) {
        if (irisParamsData_) {
            irisParamsData_->maskColor[0] = r;
            irisParamsData_->maskColor[1] = g;
            irisParamsData_->maskColor[2] = b;
            irisParamsData_->maskColor[3] = a;
        }
        if (compositeParamsData_) {
            compositeParamsData_->irisMaskColor[0] = r;
            compositeParamsData_->irisMaskColor[1] = g;
            compositeParamsData_->irisMaskColor[2] = b;
            compositeParamsData_->irisMaskColor[3] = a;
        }
    }
    void SetCompositeIrisEnabled(bool enabled) {
        if (compositeParamsData_) {
            compositeParamsData_->enableIris = enabled ? 1 : 0;
        }
    }

    void SetDissolveMaskTexture(D3D12_GPU_DESCRIPTOR_HANDLE handle) { dissolveMaskSrvHandleGPU_ = handle; }

    struct OutlineParams {
        float thickness = 0.015f;
        float padding[3];
    };

    ID3D12PipelineState *GetGraphicsPipelineStateOutline() const { return graphicsPipelineStateOutline_.Get(); }
    bool IsOutlineEnabled() const {
#ifndef USE_IMGUI
        return false; // リリースビルドのみアウトラインを非表示
#else
        return isOutlineEnabled_;
#endif
    }
    void SetOutlineEnabled(bool enabled) { isOutlineEnabled_ = enabled; }
    float GetOutlineThickness() const { return outlineParamsData_ ? outlineParamsData_->thickness : 0.015f; }
    void SetOutlineThickness(float thickness) {
        if (outlineParamsData_) {
            outlineParamsData_->thickness = thickness;
        }
    }
    D3D12_GPU_VIRTUAL_ADDRESS GetOutlineParamsGPUAddress() const {
        return outlineParamResource_->GetGPUVirtualAddress();
    }

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
    Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateNoCull_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateAdditive_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateNoCullAdditive_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateTransparent_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spritePipelineState_;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};

    // Swapchain用のビューポートとシザー矩形
	D3D12_VIEWPORT swapchainViewport_{};
	D3D12_RECT swapchainScissorRect_{};
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
    D3D12_GPU_DESCRIPTOR_HANDLE finalPostProcessSRVHandle_{};

	Microsoft::WRL::ComPtr<ID3D12RootSignature> copyImageRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyImagePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> sepiaPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> smoothingPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> irisPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vignetteParamResource_;
    VignetteParams* vignetteParamsData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> smoothingParamResource_;
    SmoothingParams* smoothingParamsData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> gaussianParamResource_;
    GaussianParams* gaussianParamsData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> compositeParamResource_;
    CompositeParams* compositeParamsData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> irisParamResource_;
    IrisParams* irisParamsData_ = nullptr;


    D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskSrvHandleGPU_{};

    PostEffect postEffect_ = PostEffect::kComposite;
    bool isPostEffectEnabled_ = true;
    bool isDepthBasedOutlineEnabled_ = true;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skyboxRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skyboxPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateOutline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> outlineParamResource_;
    OutlineParams* outlineParamsData_ = nullptr;
    bool isOutlineEnabled_ = true;

    // --- DepthBasedOutline ポストエフェクト関連 ---
    struct ProjectionInverseParams {
        Matrix4x4 projectionInverse;
    };
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthBasedOutlinePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> projectionInverseParamResource_;
    ProjectionInverseParams* projectionInverseParamsData_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthStencilSrvHandleGPU_{};
	
    // --- シャドウマップ関連 ---
    static constexpr uint32_t kShadowMapWidth = 2048;
    static constexpr uint32_t kShadowMapHeight = 2048;
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowMapDsvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE shadowMapDsvHandleCPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE shadowMapSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE shadowMapSrvHandleGPU_{};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> shadowMapRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowMapPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowMapSkinningPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowGlobalParamResource_;
    Matrix4x4* shadowGlobalParamData_ = nullptr;
    D3D12_VIEWPORT shadowViewport_{};
    D3D12_RECT shadowScissorRect_{};
	
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

    // エフェクト時間管理用
    std::chrono::steady_clock::time_point startTime_;
};

