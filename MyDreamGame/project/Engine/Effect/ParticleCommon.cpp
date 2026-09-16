#include "ParticleCommon.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include <format>
#include <dxcapi.h>
#include "Graphics/TextureManager.h"
#include "Renderer/SrvManager.h"
#include "Effect/ParticleManager.h" // Particleの定義が必要なため追加

#pragma comment(lib, "dxcompiler.lib")

void ParticleCommon::Initialize(ID3D12Device *device) {
    assert(device);
    device_ = device;

    CreateRootSignature();
    CreatePipelineState();
    CreateMesh();

    // 1. リソース（箱）を作る
    wvpResource_ = CreateBufferResource(device_, sizeof(Matrix4x4));

    // 2. リソースのアドレスを wvpData_ に紐付ける
    wvpResource_->Map(0, nullptr, reinterpret_cast<void **>(&wvpData_));

    // 3. データを入れる
    viewProjection_ = TransformFunctions::MakeIdentity4x4();
    *wvpData_ = viewProjection_;
}

void ParticleCommon::PreDraw() {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    assert(commandList);
    commandList_ = commandList;

    // ■ ここでメンバ変数の値を定数バッファに書き込む
    *wvpData_ = viewProjection_;

    // パイプラインステートの設定
    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    // ★ デバッグのため、デフォルトを強制的に加算ブレンド（kBlendModeAdd）に変更
    commandList_->SetPipelineState(pipelineStates_[kBlendModeAdd].Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // DescriptorHeapの設定 (TextureManagerから借りる)
    ID3D12DescriptorHeap *descriptorHeaps[] = {SrvManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);

    // ■ 追加: RootParameter[4] に ViewProjection をセット
    // (ルートシグネチャの修正も忘れずに行ってくださいね！)
    commandList_->SetGraphicsRootConstantBufferView(4, wvpResource_->GetGPUVirtualAddress());
}

void ParticleCommon::SetBlendMode(BlendMode blendMode) {
    SetPipelineState("resources/shaders/Particle.VS.hlsl", "resources/shaders/Particle.PS.hlsl", blendMode);
}

void ParticleCommon::SetPipelineState(const std::string& vsPath, const std::string& psPath, BlendMode blendMode) {
    if (!commandList_) return;
    ID3D12PipelineState* pso = GetOrCreatePipelineState(vsPath, psPath, blendMode);
    if (pso) {
        commandList_->SetPipelineState(pso);
    }
}

void ParticleCommon::ReloadShaders() {
    psoCache_.clear();
    shaderBlobCache_.clear();
    CreatePipelineState();
}

ID3D12PipelineState* ParticleCommon::GetOrCreatePipelineState(const std::string& vsPath, const std::string& psPath, BlendMode blendMode) {
    // 範囲外なら通常ブレンド
    if (blendMode < 0 || blendMode >= kCountOfBlendMode) {
        blendMode = kBlendModeNormal;
    }

    std::string normVsPath = vsPath.empty() ? "resources/shaders/Particle.VS.hlsl" : vsPath;
    std::string normPsPath = psPath.empty() ? "resources/shaders/Particle.PS.hlsl" : psPath;

    // デフォルトシェーダーの場合、事前生成済み配列を直接返す
    if (normVsPath == "resources/shaders/Particle.VS.hlsl" && normPsPath == "resources/shaders/Particle.PS.hlsl") {
        return pipelineStates_[blendMode].Get();
    }

    ParticlePipelineKey key{ normVsPath, normPsPath, blendMode };
    auto it = psoCache_.find(key);
    if (it != psoCache_.end()) {
        return it->second.Get();
    }

    // シェーダーのコンパイル
    auto vsBlob = CompileShaderInternal(normVsPath, L"vs_6_0");
    auto psBlob = CompileShaderInternal(normPsPath, L"ps_6_0");

    if (!vsBlob || !psBlob) {
        // コンパイル失敗時はデフォルトに安全にフォールバック
        return pipelineStates_[blendMode].Get();
    }

    auto pso = CreateSinglePipelineState(vsBlob.Get(), psBlob.Get(), blendMode);
    if (!pso) {
        return pipelineStates_[blendMode].Get();
    }

    psoCache_[key] = pso;
    return pso.Get();
}

// リストへの追加
void ParticleCommon::AddParticle(ParticleManager *ParticleManager) {
    particles_.push_back(ParticleManager);
}

// リストからの削除
void ParticleCommon::RemoveParticle(ParticleManager *ParticleManager) {
    particles_.remove(ParticleManager);
}

// 一括描画
void ParticleCommon::DrawAll(const Matrix4x4 &viewProjection) {
    for(ParticleManager *particle : particles_) {
        // マネージャのDrawに viewProjection だけ渡す
        particle->Draw(viewProjection);
    }
}

void ParticleCommon::CreateRootSignature() {
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_ROOT_PARAMETER rootParameters[5] = {};

    // [0] Material (CBV b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] Instancing Data (DescriptorTable t0)
    D3D12_DESCRIPTOR_RANGE descriptorRangeInstancing[1] = {};
    descriptorRangeInstancing[0].BaseShaderRegister = 0; // t0
    descriptorRangeInstancing[0].NumDescriptors = 1;
    descriptorRangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeInstancing;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    // [2] Texture (DescriptorTable t3)
    D3D12_DESCRIPTOR_RANGE descriptorRangeTexture[1] = {};
    descriptorRangeTexture[0].BaseShaderRegister = 0; // t3
    descriptorRangeTexture[0].NumDescriptors = 1;
    descriptorRangeTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeTexture;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // [3] DirectionalLight (CBV b1)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
    rootParameters[4].Descriptor.ShaderRegister = 2; // b2 レジスタを使用

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if(errorBlob) {
        OutputDebugStringA((char *)errorBlob->GetBufferPointer());
        assert(false);
    }
    device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
}

Microsoft::WRL::ComPtr<ID3DBlob> ParticleCommon::CompileShaderInternal(const std::string& filePath, const wchar_t* profile) {
    if (filePath.empty()) return nullptr;

    auto it = shaderBlobCache_.find(filePath);
    if (it != shaderBlobCache_.end()) {
        return it->second;
    }

    if (!dxcUtils_ || !dxcCompiler_ || !includeHandler_) {
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
        if (dxcUtils_) {
            dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
        }
    }
    if (!dxcUtils_ || !dxcCompiler_) {
        return nullptr;
    }

    std::wstring wFilePath = ConvertString(filePath);
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    if (FAILED(dxcUtils_->LoadFile(wFilePath.c_str(), nullptr, &sourceBlob))) {
        OutputDebugStringA(std::format("ParticleCommon::CompileShaderInternal: Failed to load {}\n", filePath).c_str());
        return nullptr;
    }

    LPCWSTR arguments[] = { wFilePath.c_str(), L"-E", L"main", L"-T", profile, L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr" };
    DxcBuffer buffer = {};
    buffer.Ptr = sourceBlob->GetBufferPointer();
    buffer.Size = sourceBlob->GetBufferSize();
    buffer.Encoding = DXC_CP_UTF8;

    Microsoft::WRL::ComPtr<IDxcResult> result;
    HRESULT hr = dxcCompiler_->Compile(&buffer, arguments, _countof(arguments), includeHandler_.Get(), IID_PPV_ARGS(&result));
    if (FAILED(hr) || !result) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);
    if (errorBlob && errorBlob->GetStringLength() > 0) {
        OutputDebugStringA((char*)errorBlob->GetStringPointer());
    }

    HRESULT status;
    result->GetStatus(&status);
    if (FAILED(status)) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (shaderBlob) {
        shaderBlobCache_[filePath] = shaderBlob;
    }
    return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> ParticleCommon::CreateSinglePipelineState(
    ID3DBlob* vsBlob, ID3DBlob* psBlob, BlendMode blendMode) {
    if (!device_ || !rootSignature_ || !vsBlob || !psBlob) return nullptr;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();

    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 両面描画

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // パーティクルはデプス書き込みOFF

    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    D3D12_RENDER_TARGET_BLEND_DESC &blendDesc = psoDesc.BlendState.RenderTarget[0];
    blendDesc.BlendEnable = true;
    blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;

    switch(blendMode) {
    case kBlendModeNone:
        blendDesc.BlendEnable = false;
        break;
    case kBlendModeNormal:
        blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        break;
    case kBlendModeAdd:
        blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.DestBlend = D3D12_BLEND_ONE;
        blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        break;
    case kBlendModeSubtract:
        blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.DestBlend = D3D12_BLEND_ONE;
        blendDesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
        break;
    case kBlendModeMultiply:
        blendDesc.SrcBlend = D3D12_BLEND_ZERO;
        blendDesc.DestBlend = D3D12_BLEND_SRC_COLOR;
        blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        break;
    case kBlendModeScreen:
        blendDesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
        blendDesc.DestBlend = D3D12_BLEND_ONE;
        blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
        break;
    default:
        break;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) {
        return nullptr;
    }
    return pso;
}

void ParticleCommon::CreatePipelineState() {
    auto vsBlob = CompileShaderInternal("resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    auto psBlob = CompileShaderInternal("resources/shaders/Particle.PS.hlsl", L"ps_6_0");
    assert(vsBlob && psBlob);

    for(int i = 0; i < kCountOfBlendMode; ++i) {
        pipelineStates_[i] = CreateSinglePipelineState(vsBlob.Get(), psBlob.Get(), static_cast<BlendMode>(i));
        assert(pipelineStates_[i]);
    }
}

void ParticleCommon::CreateMesh() {
    // 共通の四角形ポリゴンを作成
    vertices_ = {
        { {-1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }, // 左上
        { { 1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }, // 右上
        { {-1.0f,-1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }, // 左下
        { {-1.0f,-1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }, // 左下
        { { 1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }, // 右上
        { { 1.0f,-1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }, // 右下
    };

    vertexResource_ = CreateBufferResource(device_, sizeof(ParticleVertexData) * vertices_.size());

    ParticleVertexData *data = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&data));
    std::memcpy(data, vertices_.data(), sizeof(ParticleVertexData) * vertices_.size());
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(ParticleVertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(ParticleVertexData);
}