#include "LineRenderer.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cassert>
#include <cmath>
#include <algorithm>

LineRenderer* LineRenderer::GetInstance() {
    static LineRenderer instance;
    return &instance;
}

void LineRenderer::Initialize(ID3D12Device* device, DirectXCommon* dxCommon) {
    assert(device != nullptr && dxCommon != nullptr);
    dxCommon_ = dxCommon;

    // 1. ルートシグネチャの作成 (register(b0) CBV)
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.pParameters = rootParameters;
    rootDesc.NumParameters = _countof(rootParameters);
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // 2. シェーダーコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = CompileShader(
        L"resources/shaders/Line.VS.hlsl", L"vs_6_0",
        dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler());
    assert(vsBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = CompileShader(
        L"resources/shaders/Line.PS.hlsl", L"ps_6_0",
        dxCommon->GetDxcUtils(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler());
    assert(psBlob != nullptr);

    // 3. インプットレイアウト
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.DepthBias = -50;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = -0.5f;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 全サンプルを許可
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    // 深度テスト有効 PSO
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&psoDepthEnabled_));
    assert(SUCCEEDED(hr));

    // 深度テスト無効 PSO (透視用)
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&psoDepthDisabled_));
    assert(SUCCEEDED(hr));

    // 4. 動的頂点バッファの作成
    vertexResource_ = CreateBufferResource(device, sizeof(LineVertex) * kMaxVertices);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(LineVertex) * kMaxVertices;
    vertexBufferView_.StrideInBytes = sizeof(LineVertex);
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_));
    assert(SUCCEEDED(hr));

    // 5. 定数バッファの作成
    constantBufferResource_ = CreateBufferResource(device, (sizeof(Matrix4x4) + 255) & ~255);
    hr = constantBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstantBuffer_));
    assert(SUCCEEDED(hr));

    vertices_.reserve(4096);
}

void LineRenderer::AddLine(const Vector3& p1, const Vector3& p2, const Vector4& color) {
    if (vertices_.size() + 2 > kMaxVertices) return;

    LineVertex v1{ { p1.x, p1.y, p1.z, 1.0f }, color };
    LineVertex v2{ { p2.x, p2.y, p2.z, 1.0f }, color };
    vertices_.push_back(v1);
    vertices_.push_back(v2);
}

void LineRenderer::AddAABB(const Vector3& min, const Vector3& max, const Vector4& color) {
    Vector3 corners[8] = {
        { min.x, max.y, min.z }, // 0
        { max.x, max.y, min.z }, // 1
        { max.x, min.y, min.z }, // 2
        { min.x, min.y, min.z }, // 3
        { min.x, max.y, max.z }, // 4
        { max.x, max.y, max.z }, // 5
        { max.x, min.y, max.z }, // 6
        { min.x, min.y, max.z }, // 7
    };

    // 前面
    AddLine(corners[0], corners[1], color);
    AddLine(corners[1], corners[2], color);
    AddLine(corners[2], corners[3], color);
    AddLine(corners[3], corners[0], color);

    if (std::abs(max.z - min.z) > 0.01f) {
        // 背面
        AddLine(corners[4], corners[5], color);
        AddLine(corners[5], corners[6], color);
        AddLine(corners[6], corners[7], color);
        AddLine(corners[7], corners[4], color);

        // 柱
        AddLine(corners[0], corners[4], color);
        AddLine(corners[1], corners[5], color);
        AddLine(corners[2], corners[6], color);
        AddLine(corners[3], corners[7], color);
    }
}

void LineRenderer::AddOBB(const Vector3& center, const Vector3& size, const Vector3& rotate, const Vector4& color) {
    Matrix4x4 rotMat = TransformFunctions::MakeAffineMatrix({1.0f, 1.0f, 1.0f}, rotate, {0.0f, 0.0f, 0.0f});
    Vector3 h = { size.x * 0.5f, size.y * 0.5f, (size.z > 0.01f ? size.z * 0.5f : 0.0f) };

    Vector3 localCorners[8] = {
        { -h.x,  h.y, -h.z },
        {  h.x,  h.y, -h.z },
        {  h.x, -h.y, -h.z },
        { -h.x, -h.y, -h.z },
        { -h.x,  h.y,  h.z },
        {  h.x,  h.y,  h.z },
        {  h.x, -h.y,  h.z },
        { -h.x, -h.y,  h.z },
    };

    Vector3 worldCorners[8];
    for (int i = 0; i < 8; ++i) {
        Vector3 rot = rotMat * localCorners[i];
        worldCorners[i] = { center.x + rot.x, center.y + rot.y, center.z + rot.z };
    }

    AddLine(worldCorners[0], worldCorners[1], color);
    AddLine(worldCorners[1], worldCorners[2], color);
    AddLine(worldCorners[2], worldCorners[3], color);
    AddLine(worldCorners[3], worldCorners[0], color);

    if (h.z > 0.01f) {
        AddLine(worldCorners[4], worldCorners[5], color);
        AddLine(worldCorners[5], worldCorners[6], color);
        AddLine(worldCorners[6], worldCorners[7], color);
        AddLine(worldCorners[7], worldCorners[4], color);

        AddLine(worldCorners[0], worldCorners[4], color);
        AddLine(worldCorners[1], worldCorners[5], color);
        AddLine(worldCorners[2], worldCorners[6], color);
        AddLine(worldCorners[3], worldCorners[7], color);
    }
}

void LineRenderer::AddSphere(const Vector3& center, float radius, const Vector4& color, int segments) {
    if (segments < 3) segments = 24;
    const float step = 6.2831853f / static_cast<float>(segments);

    // XY平面
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        Vector3 p1 = { center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius, center.z };
        Vector3 p2 = { center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius, center.z };
        AddLine(p1, p2, color);
    }

    // XZ平面
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        Vector3 p1 = { center.x + std::cos(a1) * radius, center.y, center.z + std::sin(a1) * radius };
        Vector3 p2 = { center.x + std::cos(a2) * radius, center.y, center.z + std::sin(a2) * radius };
        AddLine(p1, p2, color);
    }

    // YZ平面
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        Vector3 p1 = { center.x, center.y + std::cos(a1) * radius, center.z + std::sin(a1) * radius };
        Vector3 p2 = { center.x, center.y + std::cos(a2) * radius, center.z + std::sin(a2) * radius };
        AddLine(p1, p2, color);
    }
}

void LineRenderer::Render(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, bool depthTestEnabled) {
    if (vertices_.empty() || !commandList) return;

    if (dxCommon_) {
        commandList->RSSetViewports(1, &dxCommon_->GetViewport());
        commandList->RSSetScissorRects(1, &dxCommon_->GetScissorRect());
    }

    UINT drawCount = (vertices_.size() < kMaxVertices) ? static_cast<UINT>(vertices_.size()) : kMaxVertices;
    std::memcpy(mappedVertices_, vertices_.data(), sizeof(LineVertex) * drawCount);
    *mappedConstantBuffer_ = viewProjection;

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(depthTestEnabled ? psoDepthEnabled_.Get() : psoDepthDisabled_.Get());
    commandList->SetGraphicsRootConstantBufferView(0, constantBufferResource_->GetGPUVirtualAddress());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->DrawInstanced(drawCount, 1, 0, 0);

    vertices_.clear();
}

void LineRenderer::Clear() {
    vertices_.clear();
}
