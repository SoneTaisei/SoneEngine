#include "SpriteCommon.h"
#include "DirectXCommon/DirectXCommon.h"
#include "Sprite.h" // Spriteの定義が必要
#include <d3dcompiler.h>
#include "Graphics/TextureManager.h"
#include "Graphics/SrvManager.h"
#pragma comment(lib, "d3dcompiler.lib")

void SpriteCommon::Initialize(DirectXCommon *dxCommon, int windowWidth, int windowHeight) {
    // ★ポインタを保存
    dxCommon_ = dxCommon;
    // デバイスを DirectXCommon から取得
    device_ = dxCommon_->GetDevice();

    // 射影行列の計算
    projectionMatrix_ = TransformFunctions::MakeOrthographicMatrix(
        0.0f, 0.0f, float(windowWidth), float(windowHeight), 0.0f, 100.0f);

    CreateCommonResources();
    CreateGraphicsPipeline(); // ここで dxCommon_ を使ってコンパイルできるようになります！
}

void SpriteCommon::Finalize() {
    sprites_.clear();
    // ComPtrなのでリソースは自動解放されます
}

void SpriteCommon::CreateCommonResources() {
    // --- 頂点リソース作成 (元のStaticInitializeの内容) ---
    // 単位矩形(0,0)~(1,1)で作成
    const int kVertexCount = 4;
    vertexResource_ = CreateBufferResource(device_, sizeof(VertexData) * kVertexCount);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData *vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
    vertexData[0] = { {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }; // 左下
    vertexData[1] = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }; // 左上
    vertexData[2] = { {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }; // 右下
    vertexData[3] = { {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }; // 右上
    vertexResource_->Unmap(0, nullptr);

    // --- インデックスリソース作成 ---
    const int kIndexCount = 6;
    indexResource_ = CreateBufferResource(device_, sizeof(uint32_t) * kIndexCount);
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t *indexData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
    indexData[0] = 0; indexData[1] = 1; indexData[2] = 2;
    indexData[3] = 1; indexData[4] = 3; indexData[5] = 2;
    indexResource_->Unmap(0, nullptr);
}

void SpriteCommon::CreateGraphicsPipeline() {
    HRESULT hr = S_OK;

    // 1. シェーダーのコンパイル
    // ※パスや関数名はプロジェクトの環境（Object3Dなど）に合わせて書き換えてください
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    // DirectXCommonからコンパイラ関連のポインタを取得する
    // (エンジンの構造に合わせて、Getterなどで取得できるようにしてください)
    auto dxcUtils = dxCommon_->GetDxcUtils();
    auto dxcCompiler = dxCommon_->GetDxcCompiler();
    auto includeHandler = dxCommon_->GetIncludeHandler();

    // 1. シェーダーのコンパイル (DirectXCommonと全く同じ方式にする)
    // 戻り値の型が IDxcBlob になることに注意
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = CompileShader(
        L"shaders/Sprite.VS.hlsl", L"vs_6_0",
        dxcUtils, dxcCompiler, includeHandler);
    assert(vsBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = CompileShader(
        L"shaders/Sprite.PS.hlsl", L"ps_6_0",
        dxcUtils, dxcCompiler, includeHandler);
    assert(psBlob != nullptr);

    // 2. RootSignature (ルートシグネチャ) の作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // テクスチャ用のサンプラー設定
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    // DescriptorRange (テクスチャ用 t0)
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // RootParameters (Sprite::Drawの順番に合わせる)
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    // 0: Material (b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // 1: TransformMatrix (b1)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    // 2: Texture (t0)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    // シリアライズして作成
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

    // 3. PipelineState (PSO) の作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();

    // InputLayout (VertexDataの構造: pos, uv, normal)
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    psoDesc.InputLayout.pInputElementDescs = inputElementDescs;
    psoDesc.InputLayout.NumElements = _countof(inputElementDescs);

    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};

    // ブレンド設定 (標準的なアルファブレンド)
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // ラスタライザ設定 (背面カリングなし)
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // レンダーターゲット設定 (DirectXCommonの設定に合わせる)
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;

    // デプスステンシル設定 (スプライトは通常、深度比較のみ行うか書き込まない)
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // PSO生成
    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void SpriteCommon::PreDraw(ID3D12GraphicsCommandList *commandList) {
    commandList_ = commandList;

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->SetPipelineState(pipelineState_.Get());

    ID3D12DescriptorHeap *ppHeaps[] = {SrvManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // 共通のバッファをセット
    commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList_->IASetIndexBuffer(&indexBufferView_);
    // トポロジ設定なども必要であればここで行う
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void SpriteCommon::AddSprite(Sprite *sprite) {
    sprites_.push_back(sprite);
}

void SpriteCommon::RemoveSprite(Sprite *sprite) {
    sprites_.remove(sprite);
}

void SpriteCommon::DrawAll() {
    // リストに登録されている全スプライトを描画
    for(Sprite *sprite : sprites_) {
        sprite->Draw();
    }
}
