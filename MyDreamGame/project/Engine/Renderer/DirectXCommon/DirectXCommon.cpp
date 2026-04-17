#include "DirectXCommon.h"
#include <cassert>
#include "Graphics\TextureManager.h"
#include "Renderer/SrvManager.h"
#include <format>
#include <vector>
#include <thread>

using namespace Microsoft::WRL;

// ★ 静的変数の実体を定義（ファイルの一番上の方に）
DirectXCommon *DirectXCommon::instance_ = nullptr;

DirectXCommon *DirectXCommon::GetInstance() {
    return instance_;
}

void DirectXCommon::Initialize(HWND hwnd, int32_t windowWidth, int32_t windowHeight) {
    instance_ = this;

	hwnd_ = hwnd;
	windowWidth_ = windowWidth;
	windowHeight_ = windowHeight;

	// DirectX関連の初期化
	CreateDxInstance();
	CreateFinalRenderTargets();
	CreatePipelines();
    CreateCopyImagePipeline();
    // Skybox用のパイプラインを作る指示を出す
    CreateSkyboxPipeline();
    InitializeFixFPS();

	// フェンスの初期化
	HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(SUCCEEDED(hr));

	// 最初のコマンドリストを閉じて実行
	hr = commandList_->Close();
	assert(SUCCEEDED(hr));
	ID3D12CommandList *commandLists[]{ commandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);

	// GPUの完了を待つ
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	if(fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	// 次のフレームのためにコマンドリストをリセット
	hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));

}

void DirectXCommon::Finalize() {
	// フェンスイベントのハンドルを閉じる
	if(fenceEvent_) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
	// その他はComPtrが自動的にリリースする
}

// -------------------------------------------------------------
// 既存の PreDraw を RenderTexture 用に書き換える
// -------------------------------------------------------------
void DirectXCommon::PreDraw() {
    // ※今後、RenderTextureを画像として読み込むようになったらバリア処理が必要になりますが、今は省略します。

    // ★修正1：描画先を Swapchain ではなく RenderTexture の RTV にする！
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    // 引数に renderTextureRtvHandle_ を渡す
    commandList_->OMSetRenderTargets(1, &renderTextureRtvHandle_, false, &dsvHandle);

    // ★修正2：画面のクリアも RenderTexture に対して行う（赤色などのクリアカラー）
    float clearColor[] = {1.0f, 0.0f, 0.0f, 1.0f}; // 先ほど InitializeRenderTexture で決めた赤色
    commandList_->ClearRenderTargetView(renderTextureRtvHandle_, clearColor, 0, nullptr);

    // 深度バッファをクリア (ここはそのまま)
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // テクスチャ用SRVヒープ設定やビューポート設定はそのまま残す
    ID3D12DescriptorHeap *pHeaps[] = {SrvManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(1, pHeaps);
    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->SetPipelineState(graphicsPipelineState_.Get());
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// -------------------------------------------------------------
// 新しく Swapchain（ImGui用）に切り替える関数を作る
// -------------------------------------------------------------
void DirectXCommon::PreDrawSwapchain() {
    backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

    // スワップチェーンのリソースバリア（PreDrawにあったものをこっちに移動）
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    // ★資料の指示：Depthはnullptrを設定する！
    commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, nullptr);

    // Swapchain のクリア（ImGuiの背景になります。見やすいように黒っぽい色に）
    float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex_], clearColor, 0, nullptr);

    // ビューポートとシザー矩形を再設定
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::ExecuteCommands() {
    // 1. リソースバリアを設定 (描画ターゲット状態から表示状態へ)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    // 2. コマンドリストを確定
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // 3. コマンドキューにコマンドリストを実行させる
    ID3D12CommandList *commandLists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, commandLists);
}

void DirectXCommon::Present() {
    // 4. 画面に表示 (垂直同期あり)
    swapChain_->Present(1, 0);

    // 5. GPUの処理完了を待つ (Signal & Wait)
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // 6. FPS固定
    UpdateFixFPS();

    // 7. 次のフレームの準備 (Reset)
    HRESULT hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::PostDraw() {
    ExecuteCommands();
    Present();
}


void DirectXCommon::CreateDxInstance() {
    // (WindowsApplication::CreateDxInstance の中身をそのまま貼り付け)
    HRESULT hr;
#ifdef USE_IMGUI
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter;
    for(UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));
        if(!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            Log(ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
            break;
        }
        useAdapter = nullptr;
    }
    assert(useAdapter != nullptr);

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
    for(size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
        if(SUCCEEDED(hr)) {
            break;
        }
    }
    assert(device_ != nullptr);

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateFinalRenderTargets() {
    HRESULT hr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = windowWidth_;
    swapChainDesc.Height = windowHeight_;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // ★ ここも一旦 UNORM にしてみる
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc_ = swapChainDesc;

    hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd_, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1 **>(swapChain_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // RTVの枠を「2」から「3」に増やす！（スワップチェーン2個 + RenderTexture1個）
    rtvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, false);

    for(int i = 0; i < 2; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));
        rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), i);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc, rtvHandles_[i]);
        rtvDesc_ = rtvDesc;
    }
}

void DirectXCommon::CreatePipelines() {
    HRESULT hr;

    // ★ローカル変数の宣言を削除し、ヘッダーで定義したメンバ変数( _付き )に直接代入する！
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    /*descriptorRange[1].BaseShaderRegister = 4;
    descriptorRange[1].NumDescriptors = 3;
    descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;*/

    D3D12_ROOT_PARAMETER rootParameters[7] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;     // CBVを指定
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VSで使う
    rootParameters[1].Descriptor.ShaderRegister = 0;                     // register(b0)
    rootParameters[1].Descriptor.RegisterSpace = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 3;
    rootParameters[3].Descriptor.RegisterSpace = 0;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 1;
    rootParameters[4].Descriptor.RegisterSpace = 0;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 2; // ここで register(b2) を指定
    rootParameters[5].Descriptor.RegisterSpace = 0;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 4; // register(b4)
    rootParameters[6].Descriptor.RegisterSpace = 0;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MinLOD = 0;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if(FAILED(hr)) {
        Log(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 背面をカリングする
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
    rasterizerDesc.FrontCounterClockwise = FALSE;

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"shaders/Object3D.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    assert(vertexShaderBlob != nullptr);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"shaders/Object3D.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    assert(pixelShaderBlob != nullptr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC spritePipelineStateDesc = graphicsPipelineStateDesc;
    spritePipelineStateDesc.DepthStencilState.DepthEnable = false;
    hr = device_->CreateGraphicsPipelineState(&spritePipelineStateDesc, IID_PPV_ARGS(&spritePipelineState_));
    assert(SUCCEEDED(hr));

    depthStencilResource_ = CreateDepthStencilTextureResource(device_.Get(), windowWidth_, windowHeight_); // 変更
    dsvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    viewport_ = {};
    viewport_.Width = (float)windowWidth_;
    viewport_.Height = (float)windowHeight_;
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_ = {};
    scissorRect_.left = 0;
    scissorRect_.right = windowWidth_;
    scissorRect_.top = 0;
    scissorRect_.bottom = windowHeight_;
}

void DirectXCommon::InitializeFixFPS() {
    // 現在の時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {
    // 1/60秒ぴったりの時間
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
    // 1/60秒よりわずかに短い時間
    const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

    // 現在時間を取得する
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    // 前回記録からの経過時間を取得する
    std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // 1/60秒経過していない場合
    if(elapsed < kMinTime) {
        while(std::chrono::steady_clock::now() - reference_ < kMinTime) {
            // 1マイクロ秒スリープ
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    // 現在の時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::InitializeRenderTexture() {
    // 1. RenderTexture本体の作成 (UtilityFunctionsで作った関数を呼ぶ)
    const Vector4 kRenderTargetClearValue{1.0f, 0.0f, 0.0f, 1.0f}; // 一旦分かりやすいように赤
    renderTextureResource_ = CreateRenderTextureResource(
        device_.Get(), windowWidth_, windowHeight_,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTargetClearValue);

    // 2. RTVの作成
    // ヒープの3番目（インデックス2）の場所を取得
    renderTextureRtvHandle_ = GetCPUDescriptorHandle(
        rtvDescriptorHeap_.Get(),
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), 2);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device_->CreateRenderTargetView(renderTextureResource_.Get(), &rtvDesc, renderTextureRtvHandle_);

    // 3. SRVの作成
    // SrvManagerから空きディスクリプタを割り当ててもらう
    SrvManager::GetInstance()->Allocate(&renderTextureSrvHandleCPU_, &renderTextureSrvHandleGPU_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    device_->CreateShaderResourceView(renderTextureResource_.Get(), &srvDesc, renderTextureSrvHandleCPU_);
}

void DirectXCommon::CreateCopyImagePipeline() {
    HRESULT hr;

    // 1. RootSignatureの作成 (TextureとSamplerが使えるようにする)
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Texture用 (t0) の DescriptorRange
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameterにセット
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // Sampler用 (s0)
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&copyImageRootSignature_));

    // 2. シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = CompileShader(L"shaders/CopyImage.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = CompileShader(L"shaders/CopyImage.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());

    // 3. PipelineState (PSO) の作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = copyImageRootSignature_.Get();
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};

    // ★資料の指示1：InputLayoutは利用しない
    psoDesc.InputLayout.pInputElementDescs = nullptr;
    psoDesc.InputLayout.NumElements = 0;

    // ★資料の指示2：Depthは不要なのでfalse
    psoDesc.DepthStencilState.DepthEnable = false;

    // その他の必須設定（画面に描くための基本的な設定）
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングなし
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // Swapchainのフォーマットに合わせる
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&copyImagePipelineState_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::DrawRenderTexture() {
    // 1. バリアを張る (RenderTarget -> PixelShaderResource に状態遷移)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTextureResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList_->ResourceBarrier(1, &barrier);

    // 2. コピー用パイプラインの設定
    commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
    commandList_->SetPipelineState(copyImagePipelineState_.Get());

    // テクスチャ(SRV)をルートパラメータ0番にセット
    commandList_->SetGraphicsRootDescriptorTable(0, renderTextureSrvHandleGPU_);

    // ★資料の指示通り、3頂点だけを描画 (全画面を覆う三角形が1枚描かれます)
    commandList_->DrawInstanced(3, 1, 0, 0);

    // 3. バリアを戻す (PixelShaderResource -> RenderTarget に状態遷移)
    // ※次のフレームでまたRenderTextureに書き込むため、元の状態に戻しておく必要があります
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);
}

void DirectXCommon::CreateSkyboxPipeline() {
    HRESULT hr;

    // 1. RootSignatureの作成
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    // b0 (Transform用)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    // b1 (Material用)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 1; // ★PS側は b1
    // t0 (TextureCube用)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // サンプラー (s0)
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&skyboxRootSignature_));

    // 2. 頂点レイアウト (Positionだけ！)
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // Vector4に合わせる
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // 3. シェーダーコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = CompileShader(L"shaders/Skybox.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = CompileShader(L"shaders/Skybox.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());

    // 4. PSO作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = skyboxRootSignature_.Get();
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    // ★重要設定1：カリングは内側を見るため FRONT (または NONE)
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;

    // ★重要設定2：深度書き込みを ZERO にする
    psoDesc.DepthStencilState.DepthEnable = true;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // <= にする
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&skyboxPipelineState_));
    assert(SUCCEEDED(hr));
}
