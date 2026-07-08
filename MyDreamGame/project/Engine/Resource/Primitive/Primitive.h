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
    Triangle,
    Star
};

class Primitive {
public:
    virtual ~Primitive() = default;

    virtual void GenerateModelData() = 0;
    virtual void Initialize(ID3D12Device* device);
    void Draw();

    const ModelData& GetModelData() const { return modelData_; }

protected:
    void CreateBuffers(ID3D12Device* device);

    ModelData modelData_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
};
