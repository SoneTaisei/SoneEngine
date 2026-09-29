#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "Core/Utility/Structs.h"

class DirectXCommon;

struct LineVertex {
    Vector4 position;
    Vector4 color;
};

class LineRenderer {
public:
    static LineRenderer* GetInstance();

    void Initialize(ID3D12Device* device, DirectXCommon* dxCommon);

    // 描画プリミティブの追加
    void AddLine(const Vector3& p1, const Vector3& p2, const Vector4& color);
    void AddAABB(const Vector3& min, const Vector3& max, const Vector4& color);
    void AddOBB(const Vector3& center, const Vector3& size, const Vector3& rotate, const Vector4& color);
    void AddSphere(const Vector3& center, float radius, const Vector4& color, int segments = 24);

    // 描画実行
    void Render(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, bool depthTestEnabled);

    // リセット
    void Clear();

private:
    LineRenderer() = default;
    ~LineRenderer() = default;
    LineRenderer(const LineRenderer&) = delete;
    LineRenderer& operator=(const LineRenderer&) = delete;

private:
    static const UINT kMaxVertices = 131072; // 最大約65,536本の線分

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoDepthEnabled_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> psoDepthDisabled_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    LineVertex* mappedVertices_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBufferResource_;
    Matrix4x4* mappedConstantBuffer_ = nullptr;

    std::vector<LineVertex> vertices_;
};
