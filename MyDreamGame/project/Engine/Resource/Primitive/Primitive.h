#pragma once
#include <d3d12.h>
#include <vector>
#include <string>
#include <wrl.h>
#include <numbers>
#include "Core/Utility/Structs.h"

enum class PrimitiveType {
    Plane,
    Box,
    Sphere,
    Circle,
    Ring,
    Cylinder,
    Cone,
    Torus,
    Triangle
};

class Primitive {
public:
    Primitive() = default;
    ~Primitive() = default;

    // 初期化
    void Initialize(ID3D12Device* device, PrimitiveType type, float size = 1.0f, uint32_t segments = 16);
    // Ring専用初期化
    void InitializeRing(ID3D12Device* device, float innerRadius, float outerRadius, uint32_t segments, float startAngle, float endAngle, const Vector4& innerColor, const Vector4& outerColor, bool isRadialUV);

    // 描画
    void Draw(ID3D12GraphicsCommandList* commandList);

    // ゲッター
    const ModelData& GetModelData() const { return modelData_; }

private:
    // 各形状の生成
    void CreatePlane(float size);
    void CreateBox(float size);
    void CreateSphere(float size, uint32_t segments);
    void CreateCircle(float size, uint32_t segments);
    void CreateRing(float innerRadius, float outerRadius, uint32_t segments, float startAngle = 0.0f, float endAngle = 2.0f * std::numbers::pi_v<float>, const Vector4& innerColor = {1,1,1,1}, const Vector4& outerColor = {1,1,1,1}, bool isRadialUV = false);
    void CreateCylinder(float radius, float height, uint32_t segments);
    void CreateCone(float radius, float height, uint32_t segments);
    void CreateTorus(float ringRadius, float tubeRadius, uint32_t segments);
    void CreateTriangle(float size);

    // バッファ生成
    void CreateBuffers(ID3D12Device* device);

private:
    ModelData modelData_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
};
