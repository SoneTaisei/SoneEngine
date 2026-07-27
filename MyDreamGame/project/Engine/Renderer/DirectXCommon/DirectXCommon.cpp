#include "DirectXCommon.h"
#include <cassert>
#include "Graphics\TextureManager.h"
#include "Renderer/SrvManager.h"
#include "Graphics/CameraManager.h"
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
    CreatePostEffectPipelines();
    // Skybox用のパイプラインを作る指示を出す
    CreateSkyboxPipeline();
    InitializeFixFPS();
    startTime_ = std::chrono::steady_clock::now();

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

    // ★修正2：画面のクリアも RenderTexture に対して行う（Initialize時のクリア値と合わせる）
    float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // InitializeRenderTextureで設定した値と一致させる
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
    commandList_->RSSetViewports(1, &swapchainViewport_);
    commandList_->RSSetScissorRects(1, &swapchainScissorRect_);
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

void DirectXCommon::ResizeSwapchain(int32_t width, int32_t height) {
    if (!swapChain_) return;

    // 1. GPUが現在のコマンドリストの実行を完了するまで待機
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // 2. スワップチェーンのバッファ参照を解放
    for (int i = 0; i < 2; ++i) {
        swapChainResources_[i].Reset();
    }

    // 3. バッファをリサイズ
    HRESULT hr = swapChain_->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr)) return;

    // 4. ウィンドウサイズを更新
    windowWidth_ = width;
    windowHeight_ = height;

    // 5. 新しいバッファを取得してRTVを作り直す
    for (int i = 0; i < 2; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));
        device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc_, rtvHandles_[i]);
    }

    // 6. ビューポートとシザー矩形を更新
    swapchainViewport_.Width = (float)width;
    swapchainViewport_.Height = (float)height;
    swapchainScissorRect_.right = width;
    swapchainScissorRect_.bottom = height;
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

    // CameraManagerの初期化
    CameraManager::GetInstance()->Initialize(device_.Get());
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

    // RTVの枠を「3」から「4」に増やす！（スワップチェーン2個 + RenderTexture1個 + PostProcess1個）
    rtvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, false);

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
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].NumDescriptors = 1;     // t0 の 1枚分
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRangeEnv[1] = {};
    descriptorRangeEnv[0].BaseShaderRegister = 1; // t1
    descriptorRangeEnv[0].NumDescriptors = 1;     // t1 の 1枚分
    descriptorRangeEnv[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeEnv[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[9] = {};
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

    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[7].DescriptorTable.pDescriptorRanges = descriptorRangeEnv;
    rootParameters[7].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnv);

    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // Used in Outline VS
    rootParameters[8].Descriptor.ShaderRegister = 1;                     // register(b1)
    rootParameters[8].Descriptor.RegisterSpace = 0;

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

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {};
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
    inputElementDescs[3].SemanticName = "COLOR";
    inputElementDescs[3].SemanticIndex = 0;
    inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
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
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 背面をカリングする
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
    rasterizerDesc.FrontCounterClockwise = FALSE;

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    assert(vertexShaderBlob != nullptr);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
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

    // =====================================================================================
    // スキンメッシュ用のパイプラインステート (skinningPipelineState_)
    // =====================================================================================

    // RootSignature は通常描画のもの (rootParameters) に Palette用 (VS, t1) を追加したもの
    CD3DX12_DESCRIPTOR_RANGE paletteRange{};
    paletteRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

    D3D12_ROOT_PARAMETER skinningRootParameters[10] = {};
    for (int i = 0; i < 9; ++i) {
        skinningRootParameters[i] = rootParameters[i];
    }

    // 追加の パレットSRV (t1, VS)
    skinningRootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    skinningRootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    skinningRootParameters[9].DescriptorTable.pDescriptorRanges = &paletteRange;
    skinningRootParameters[9].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC skinningRootSignatureDesc{};
    skinningRootSignatureDesc.pParameters = skinningRootParameters;
    skinningRootSignatureDesc.NumParameters = 10;
    skinningRootSignatureDesc.pStaticSamplers = staticSamplers;
    skinningRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
    skinningRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> skinningSignatureBlob = nullptr;
    D3D12SerializeRootSignature(&skinningRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &skinningSignatureBlob, &errorBlob);
    device_->CreateRootSignature(0, skinningSignatureBlob->GetBufferPointer(), skinningSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&skinningRootSignature_));

    // 2. InputLayout の拡張 (WEIGHT, BONEINDICES の追加)
    D3D12_INPUT_ELEMENT_DESC skinningInputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "INDEX",    0, DXGI_FORMAT_R32G32B32A32_SINT,  1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_INPUT_LAYOUT_DESC skinningInputLayoutDesc{};
    skinningInputLayoutDesc.pInputElementDescs = skinningInputElementDescs;
    skinningInputLayoutDesc.NumElements = _countof(skinningInputElementDescs);

    // 3. Shader のコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> skinningVertexShaderBlob = CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    // ピクセルシェーダは既存の Object3d.PS.hlsl を流用

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinningPipelineStateDesc = graphicsPipelineStateDesc;
    skinningPipelineStateDesc.pRootSignature = skinningRootSignature_.Get();
    skinningPipelineStateDesc.InputLayout = skinningInputLayoutDesc;
    skinningPipelineStateDesc.VS = { skinningVertexShaderBlob->GetBufferPointer(), skinningVertexShaderBlob->GetBufferSize() };

    device_->CreateGraphicsPipelineState(&skinningPipelineStateDesc, IID_PPV_ARGS(&skinningPipelineState_));

    assert(SUCCEEDED(hr));

    // カリングなし
    graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateNoCull_));
    assert(SUCCEEDED(hr));

    // --- 加算合成のパイプラインも作成 ---
    graphicsPipelineStateDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    // 半透明系はデプス書き込みをしない
    graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    
    // カリングあり・加算
    graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateAdditive_));
    assert(SUCCEEDED(hr));

    // カリングなし・加算
    graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateNoCullAdditive_));
    assert(SUCCEEDED(hr));

    // --- 通常合成・デプス書き込みなしのパイプラインも作成 ---
    graphicsPipelineStateDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateTransparent_));
    assert(SUCCEEDED(hr));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC spritePipelineStateDesc = graphicsPipelineStateDesc;
    spritePipelineStateDesc.DepthStencilState.DepthEnable = false;
    hr = device_->CreateGraphicsPipelineState(&spritePipelineStateDesc, IID_PPV_ARGS(&spritePipelineState_));
    assert(SUCCEEDED(hr));

    // --- Outline Pipeline State ---
    Microsoft::WRL::ComPtr<IDxcBlob> outlineVSBlob = CompileShader(L"resources/shaders/Outline.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    assert(outlineVSBlob != nullptr);
    Microsoft::WRL::ComPtr<IDxcBlob> outlinePSBlob = CompileShader(L"resources/shaders/Outline.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    assert(outlinePSBlob != nullptr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC outlinePipelineStateDesc = graphicsPipelineStateDesc;
    outlinePipelineStateDesc.VS = { outlineVSBlob->GetBufferPointer(), outlineVSBlob->GetBufferSize() };
    outlinePipelineStateDesc.PS = { outlinePSBlob->GetBufferPointer(), outlinePSBlob->GetBufferSize() };
    
    // 背面カリングは行わない（アウトラインパスは片面/両面・平面に関わらず動作するよう。先にアウトラインを描いてから本体を重ねるため）
    outlinePipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    outlinePipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    
    // メインパスとのZバッファ競合（Z-fighting）を防ぐための深度バイアス設定
    outlinePipelineStateDesc.RasterizerState.DepthBias = -1000;
    outlinePipelineStateDesc.RasterizerState.DepthBiasClamp = 0.0f;
    outlinePipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    
    // 深度設定: 深度テストは有効にするが、メインパスとのZバッファ競合を防ぐため深度書き込みは無効（DISABLE）にする
    outlinePipelineStateDesc.DepthStencilState.DepthEnable = true;
    outlinePipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    outlinePipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    
    // アウトライン用の不透明描画設定
    outlinePipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

    hr = device_->CreateGraphicsPipelineState(&outlinePipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateOutline_));
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

    swapchainViewport_ = viewport_;
    swapchainScissorRect_ = scissorRect_;
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
    const Vector4 kRenderTargetClearValue{0.1f, 0.25f, 0.5f, 1.0f}; // PreDrawでのクリア色と合わせる
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

    // --- ポストプロセス用テクスチャの作成 ---
    const Vector4 kPostProcessClearValue{0.0f, 0.0f, 0.0f, 1.0f};
    postProcessResource_ = CreateRenderTextureResource(
        device_.Get(), windowWidth_, windowHeight_,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kPostProcessClearValue, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // RTVの作成 (4番目: インデックス3)
    postProcessRtvHandle_ = GetCPUDescriptorHandle(
        rtvDescriptorHeap_.Get(),
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), 3);
    
    device_->CreateRenderTargetView(postProcessResource_.Get(), &rtvDesc, postProcessRtvHandle_);

    // SRVの作成
    SrvManager::GetInstance()->Allocate(&postProcessSrvHandleCPU_, &postProcessSrvHandleGPU_);
    device_->CreateShaderResourceView(postProcessResource_.Get(), &srvDesc, postProcessSrvHandleCPU_);

    // --- デプスバッファのSRVを作成 ---
    SrvManager::GetInstance()->Allocate(&depthStencilSrvHandleCPU_, &depthStencilSrvHandleGPU_);

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels = 1;

    device_->CreateShaderResourceView(depthStencilResource_.Get(), &depthSrvDesc, depthStencilSrvHandleCPU_);

    // 最終出力先SRVハンドルの初期値
    finalPostProcessSRVHandle_ = postProcessSrvHandleGPU_;
}

void DirectXCommon::CreatePostEffectPipelines() {
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

    // 深度テクスチャ用 (t1) の DescriptorRange
    D3D12_DESCRIPTOR_RANGE depthDescriptorRange[1] = {};
    depthDescriptorRange[0].BaseShaderRegister = 1; // t1
    depthDescriptorRange[0].NumDescriptors = 1;
    depthDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    depthDescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameterにセット
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    // rootParameters[0]: t0
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // rootParameters[1]: b0 (VignetteParams 等)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // rootParameters[2]: t1 (DepthTexture)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = depthDescriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // rootParameters[3]: b1 (ProjectionInverseParams)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // Sampler用 (s0, s1)
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
    // Linear (s0)
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Point (s1)
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[1].ShaderRegister = 1; // s1
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&copyImageRootSignature_));

    // 2. シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psCopyBlob = CompileShader(L"resources/shaders/CopyImage.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psGrayscaleBlob = CompileShader(L"resources/shaders/Grayscale.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psSepiaBlob = CompileShader(L"resources/shaders/Sepia.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psVignetteBlob = CompileShader(L"resources/shaders/Vignette.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psSmoothingBlob = CompileShader(L"resources/shaders/Smoothing.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psGaussianBlob = CompileShader(L"resources/shaders/GaussianFilter.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psCompositeBlob = CompileShader(L"resources/shaders/CompositePostProcess.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psDepthBasedOutlineBlob = CompileShader(L"resources/shaders/DepthBasedOutline.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());


    // 3. PipelineState (PSO) の作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = copyImageRootSignature_.Get();
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psCopyBlob->GetBufferPointer(), psCopyBlob->GetBufferSize()};

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

    // グレースケール用のPSOを作成
    psoDesc.PS = {psGrayscaleBlob->GetBufferPointer(), psGrayscaleBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&grayscalePipelineState_));
    assert(SUCCEEDED(hr));

    // セピア用のPSOを作成
    psoDesc.PS = {psSepiaBlob->GetBufferPointer(), psSepiaBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sepiaPipelineState_));
    assert(SUCCEEDED(hr));

    // ヴィネット用のPSOを作成
    psoDesc.PS = {psVignetteBlob->GetBufferPointer(), psVignetteBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&vignettePipelineState_));
    assert(SUCCEEDED(hr));

    // ヴィネット用定数バッファの作成
    vignetteParamResource_ = CreateBufferResource(device_.Get(), sizeof(VignetteParams));
    vignetteParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteParamsData_));
    vignetteParamsData_->color[0] = 0.0f;
    vignetteParamsData_->color[1] = 0.0f;
    vignetteParamsData_->color[2] = 0.0f;
    vignetteParamsData_->color[3] = 1.0f;
    vignetteParamsData_->scale = 16.0f;
    vignetteParamsData_->power = 0.8f;

    // アウトライン用定数バッファの作成
    outlineParamResource_ = CreateBufferResource(device_.Get(), 256);
    outlineParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&outlineParamsData_));
    outlineParamsData_->thickness = 0.015f;

    // スムージング用のPSOを作成
    psoDesc.PS = {psSmoothingBlob->GetBufferPointer(), psSmoothingBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&smoothingPipelineState_));
    assert(SUCCEEDED(hr));

    // スムージング用定数バッファの作成
    smoothingParamResource_ = CreateBufferResource(device_.Get(), sizeof(SmoothingParams));
    smoothingParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&smoothingParamsData_));
    smoothingParamsData_->kernelSize = 2;
    smoothingParamsData_->texelSize[0] = 1.0f / (float)windowWidth_;
    smoothingParamsData_->texelSize[1] = 1.0f / (float)windowHeight_;
    smoothingParamsData_->strength = 1.0f;

    // ガウスフィルター用のPSOを作成
    psoDesc.PS = {psGaussianBlob->GetBufferPointer(), psGaussianBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gaussianPipelineState_));
    assert(SUCCEEDED(hr));

    // ガウスフィルター用定数バッファの作成
    gaussianParamResource_ = CreateBufferResource(device_.Get(), sizeof(GaussianParams));
    gaussianParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&gaussianParamsData_));
    gaussianParamsData_->sigma = 2.0f;
    gaussianParamsData_->texelSize[0] = 1.0f / (float)windowWidth_;
    gaussianParamsData_->texelSize[1] = 1.0f / (float)windowHeight_;

    // 統合ポストプロセス用のPSOを作成
    psoDesc.PS = {psCompositeBlob->GetBufferPointer(), psCompositeBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&compositePipelineState_));
    assert(SUCCEEDED(hr));

    // 統合ポストプロセス用定数バッファの作成
    compositeParamResource_ = CreateBufferResource(device_.Get(), (sizeof(CompositeParams) + 255) & ~255);
    compositeParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&compositeParamsData_));
    compositeParamsData_->grayscaleStrength = 0.0f;
    compositeParamsData_->sepiaStrength = 0.0f;
    compositeParamsData_->enableVignette = 0;
    compositeParamsData_->vignetteScale = 16.0f;
    compositeParamsData_->vignettePower = 0.8f;
    compositeParamsData_->blurType = 0;
    compositeParamsData_->boxBlurKernelSize = 1;
    compositeParamsData_->boxBlurStrength = 1.0f;
    compositeParamsData_->vignetteColor[0] = 0.0f;
    compositeParamsData_->vignetteColor[1] = 0.0f;
    compositeParamsData_->vignetteColor[2] = 0.0f;
    compositeParamsData_->vignetteColor[3] = 1.0f;
    compositeParamsData_->gaussianSigma = 2.0f;
    compositeParamsData_->texelSize[0] = 1.0f / (float)windowWidth_;
    compositeParamsData_->texelSize[1] = 1.0f / (float)windowHeight_;
    compositeParamsData_->padding = 0.0f;
    compositeParamsData_->enableRadialBlur = 0;
    compositeParamsData_->radialBlurCenter[0] = 0.5f;
    compositeParamsData_->radialBlurCenter[1] = 0.5f;
    compositeParamsData_->radialBlurWidth = 0.01f;
    compositeParamsData_->radialBlurSamples = 10;
    std::memset(compositeParamsData_->radialPadding, 0, sizeof(compositeParamsData_->radialPadding));
    compositeParamsData_->enableDissolve = 0;
    compositeParamsData_->dissolveThreshold = 0.0f;
    compositeParamsData_->dissolveEdgeWidth = 0.03f;
    compositeParamsData_->dissolvePadding = 0.0f;
    compositeParamsData_->dissolveEdgeColor[0] = 1.0f;
    compositeParamsData_->dissolveEdgeColor[1] = 0.4f;
    compositeParamsData_->dissolveEdgeColor[2] = 0.3f;
    compositeParamsData_->dissolvePadding2 = 0.0f;
    compositeParamsData_->dissolveBgColor[0] = 0.0f;
    compositeParamsData_->dissolveBgColor[1] = 0.0f;
    compositeParamsData_->dissolveBgColor[2] = 0.0f;
    compositeParamsData_->dissolvePadding3 = 0.0f;

    // Noise
    compositeParamsData_->enableNoise = 0;
    compositeParamsData_->noiseStrength = 0.3f;
    compositeParamsData_->noiseBlendMode = 1;
    compositeParamsData_->noiseScale = 128.0f;
    compositeParamsData_->noiseTime = 0.0f;
    std::memset(compositeParamsData_->noisePadding, 0, sizeof(compositeParamsData_->noisePadding));

    // DepthBasedOutline 用のPSOを作成
    psoDesc.PS = {psDepthBasedOutlineBlob->GetBufferPointer(), psDepthBasedOutlineBlob->GetBufferSize()};
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthBasedOutlinePipelineState_));
    assert(SUCCEEDED(hr));



    // 逆プロジェクション行列用定数バッファの作成
    projectionInverseParamResource_ = CreateBufferResource(device_.Get(), sizeof(ProjectionInverseParams));
    projectionInverseParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&projectionInverseParamsData_));
    projectionInverseParamsData_->projectionInverse = TransformFunctions::MakeIdentity4x4();
}

void DirectXCommon::ExecutePostEffect() {
    if (!isPostEffectEnabled_) {
        // ポストプロセス無効時は、単に renderTexture -> postProcess へコピーするだけにする
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[0].Transition.pResource = renderTextureResource_.Get();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[1].Transition.pResource = postProcessResource_.Get();
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList_->ResourceBarrier(2, barriers);

        commandList_->OMSetRenderTargets(1, &postProcessRtvHandle_, false, nullptr);
        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        commandList_->ClearRenderTargetView(postProcessRtvHandle_, clearColor, 0, nullptr);

        commandList_->RSSetViewports(1, &viewport_);
        commandList_->RSSetScissorRects(1, &scissorRect_);

        commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
        commandList_->SetPipelineState(copyImagePipelineState_.Get());

        commandList_->SetGraphicsRootDescriptorTable(0, renderTextureSrvHandleGPU_);
        commandList_->DrawInstanced(3, 1, 0, 0);

        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList_->ResourceBarrier(2, barriers);

        finalPostProcessSRVHandle_ = postProcessSrvHandleGPU_;
        return;
    }

    if (isDepthBasedOutlineEnabled_) {
        // --- 1パス目: 深度ベース・アウトライン (renderTexture -> postProcess) ---
        // 1. バリアを張る (RenderTexture: RT -> SRV, PostProcess: SRV -> RT, DepthStencil: WRITE -> SRV)
        D3D12_RESOURCE_BARRIER barriers[3] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[0].Transition.pResource = renderTextureResource_.Get();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[1].Transition.pResource = postProcessResource_.Get();
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[2].Transition.pResource = depthStencilResource_.Get();
        barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList_->ResourceBarrier(3, barriers);

        // 2. 描画先を PostProcessTexture に設定
        commandList_->OMSetRenderTargets(1, &postProcessRtvHandle_, false, nullptr);
        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        commandList_->ClearRenderTargetView(postProcessRtvHandle_, clearColor, 0, nullptr);

        // ビューポートとシザーの設定
        commandList_->RSSetViewports(1, &viewport_);
        commandList_->RSSetScissorRects(1, &scissorRect_);

        // 3. パイプライン設定
        commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
        commandList_->SetPipelineState(depthBasedOutlinePipelineState_.Get());

        // 逆プロジェクション行列の更新
        if (projectionInverseParamsData_) {
            Matrix4x4 projectionMatrix = CameraManager::GetInstance()->GetProjectionMatrix();
            projectionInverseParamsData_->projectionInverse = TransformFunctions::Inverse(projectionMatrix);
        }
        commandList_->SetGraphicsRootConstantBufferView(3, projectionInverseParamResource_->GetGPUVirtualAddress());

        // ディスクリプタテーブルのバインド
        commandList_->SetGraphicsRootDescriptorTable(0, renderTextureSrvHandleGPU_);
        commandList_->SetGraphicsRootDescriptorTable(2, depthStencilSrvHandleGPU_);

        commandList_->DrawInstanced(3, 1, 0, 0);

        // 4. バリア遷移 (PostProcess: RT -> SRV, DepthStencil: SRV -> WRITE)
        // ※ RenderTexture は SRV のまま（次のパスで RENDER_TARGET に遷移させるため）
        D3D12_RESOURCE_BARRIER midBarriers[2] = {};
        midBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        midBarriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        midBarriers[0].Transition.pResource = postProcessResource_.Get();
        midBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        midBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        midBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        midBarriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        midBarriers[1].Transition.pResource = depthStencilResource_.Get();
        midBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        midBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        commandList_->ResourceBarrier(2, midBarriers);

        // --- 2パス目: コンポジット (postProcess -> renderTexture) ---
        // 1. RenderTexture を RT に遷移
        D3D12_RESOURCE_BARRIER renderTexToRTBarrier = {};
        renderTexToRTBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        renderTexToRTBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        renderTexToRTBarrier.Transition.pResource = renderTextureResource_.Get();
        renderTexToRTBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        renderTexToRTBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList_->ResourceBarrier(1, &renderTexToRTBarrier);

        // 2. 描画先を RenderTexture に設定
        commandList_->OMSetRenderTargets(1, &renderTextureRtvHandle_, false, nullptr);
        commandList_->ClearRenderTargetView(renderTextureRtvHandle_, clearColor, 0, nullptr);

        // 3. パイプライン設定
        commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
        
        if (compositeParamsData_) {
            auto now = std::chrono::steady_clock::now();
            float elapsedSeconds = std::chrono::duration<float>(now - startTime_).count();
            compositeParamsData_->noiseTime = elapsedSeconds;
        }
        commandList_->SetPipelineState(compositePipelineState_.Get());
        commandList_->SetGraphicsRootConstantBufferView(1, compositeParamResource_->GetGPUVirtualAddress());
        commandList_->SetGraphicsRootDescriptorTable(2, dissolveMaskSrvHandleGPU_);

        // 入力として postProcessResource_ (パス1の出力) をバインド
        commandList_->SetGraphicsRootDescriptorTable(0, postProcessSrvHandleGPU_);
        commandList_->DrawInstanced(3, 1, 0, 0);

        // 4. RenderTexture を SRV に遷移 (コピー処理のために読み取れるようにする)
        D3D12_RESOURCE_BARRIER renderTexToSRVBarrier = {};
        renderTexToSRVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        renderTexToSRVBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        renderTexToSRVBarrier.Transition.pResource = renderTextureResource_.Get();
        renderTexToSRVBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        renderTexToSRVBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList_->ResourceBarrier(1, &renderTexToSRVBarrier);

        finalPostProcessSRVHandle_ = renderTextureSrvHandleGPU_;
    }
    else {
        // --- 1パスのみ: コンポジット (renderTexture -> postProcess) ---
        // 1. バリアを張る (RenderTexture: RT -> SRV, PostProcess: SRV -> RT)
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[0].Transition.pResource = renderTextureResource_.Get();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[1].Transition.pResource = postProcessResource_.Get();
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList_->ResourceBarrier(2, barriers);

        // 2. 描画先を PostProcessTexture に設定
        commandList_->OMSetRenderTargets(1, &postProcessRtvHandle_, false, nullptr);
        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        commandList_->ClearRenderTargetView(postProcessRtvHandle_, clearColor, 0, nullptr);

        // ビューポートとシザーの設定
        commandList_->RSSetViewports(1, &viewport_);
        commandList_->RSSetScissorRects(1, &scissorRect_);

        // 3. パイプライン設定
        commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
        
        // 選択されたエフェクトに応じて PSO を切り替える
        if (postEffect_ == PostEffect::kGrayscale) {
            commandList_->SetPipelineState(grayscalePipelineState_.Get());
        } else if (postEffect_ == PostEffect::kSepia) {
            commandList_->SetPipelineState(sepiaPipelineState_.Get());
        } else if (postEffect_ == PostEffect::kVignette) {
            commandList_->SetPipelineState(vignettePipelineState_.Get());
            commandList_->SetGraphicsRootConstantBufferView(1, vignetteParamResource_->GetGPUVirtualAddress());
        } else if (postEffect_ == PostEffect::kSmoothing) {
            commandList_->SetPipelineState(smoothingPipelineState_.Get());
            commandList_->SetGraphicsRootConstantBufferView(1, smoothingParamResource_->GetGPUVirtualAddress());
        } else if (postEffect_ == PostEffect::kGaussian) {
            commandList_->SetPipelineState(gaussianPipelineState_.Get());
            commandList_->SetGraphicsRootConstantBufferView(1, gaussianParamResource_->GetGPUVirtualAddress());
        } else if (postEffect_ == PostEffect::kComposite) {
            if (compositeParamsData_) {
                auto now = std::chrono::steady_clock::now();
                float elapsedSeconds = std::chrono::duration<float>(now - startTime_).count();
                compositeParamsData_->noiseTime = elapsedSeconds;
            }
            commandList_->SetPipelineState(compositePipelineState_.Get());
            commandList_->SetGraphicsRootConstantBufferView(1, compositeParamResource_->GetGPUVirtualAddress());
            commandList_->SetGraphicsRootDescriptorTable(2, dissolveMaskSrvHandleGPU_);

        } else {
            // kNone または想定外
            commandList_->SetPipelineState(copyImagePipelineState_.Get());
        }

        commandList_->SetGraphicsRootDescriptorTable(0, renderTextureSrvHandleGPU_);
        commandList_->DrawInstanced(3, 1, 0, 0);

        // 4. バリアを戻す (PostProcess: RT -> SRV, RenderTexture: SRV -> RT)
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList_->ResourceBarrier(2, barriers);

        // 最終出力先は postProcessSrvHandleGPU_
        finalPostProcessSRVHandle_ = postProcessSrvHandleGPU_;
    }
}

void DirectXCommon::DrawRenderTexture() {
    // 1. スワップチェーンのサイズと論理サイズ(1280x720)からレターボックス用のビューポートを計算
    float scale = (std::min)((float)windowWidth_ / 1280.0f, (float)windowHeight_ / 720.0f);
    float vpWidth = 1280.0f * scale;
    float vpHeight = 720.0f * scale;
    float vpX = ((float)windowWidth_ - vpWidth) / 2.0f;
    float vpY = ((float)windowHeight_ - vpHeight) / 2.0f;

    D3D12_VIEWPORT letterboxViewport{};
    letterboxViewport.TopLeftX = vpX;
    letterboxViewport.TopLeftY = vpY;
    letterboxViewport.Width = vpWidth;
    letterboxViewport.Height = vpHeight;
    letterboxViewport.MinDepth = 0.0f;
    letterboxViewport.MaxDepth = 1.0f;

    D3D12_RECT letterboxScissor{};
    letterboxScissor.left = static_cast<LONG>(vpX);
    letterboxScissor.top = static_cast<LONG>(vpY);
    letterboxScissor.right = static_cast<LONG>(vpX + vpWidth);
    letterboxScissor.bottom = static_cast<LONG>(vpY + vpHeight);

    // ビューポートとシザーを適用（元のswapchainViewport_からは一時的に変更）
    commandList_->RSSetViewports(1, &letterboxViewport);
    commandList_->RSSetScissorRects(1, &letterboxScissor);

    // ポストプロセス済みのテクスチャ(SRV)を Swapchain (RT) にコピーする
    commandList_->SetGraphicsRootSignature(copyImageRootSignature_.Get());
    commandList_->SetPipelineState(copyImagePipelineState_.Get());

    commandList_->SetGraphicsRootDescriptorTable(0, finalPostProcessSRVHandle_);
    commandList_->DrawInstanced(3, 1, 0, 0);

    // もし最終結果が renderTextureResource_ にある場合、次のフレームで RENDER_TARGET として使えるように戻す
    if (finalPostProcessSRVHandle_.ptr == renderTextureSrvHandleGPU_.ptr) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList_->ResourceBarrier(1, &barrier);
    }

    // ビューポートとシザーを元に戻す（念のため）
    commandList_->RSSetViewports(1, &swapchainViewport_);
    commandList_->RSSetScissorRects(1, &swapchainScissorRect_);
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
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = CompileShader(L"resources/shaders/Skybox.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = CompileShader(L"resources/shaders/Skybox.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());

    // 4. PSO作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = skyboxRootSignature_.Get();
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    // ★重要設定1：カリングは内側を見るため FRONT (または NONE)
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

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
