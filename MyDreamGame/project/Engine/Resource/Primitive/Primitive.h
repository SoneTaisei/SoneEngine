#pragma once
#include "Core/Utility/Structs.h"
#include <d3d12.h>
#include <numbers>
#include <string>
#include <vector>
#include <wrl.h>

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
    virtual void Initialize(ID3D12Device *device);
    void Draw();

    const ModelData &GetModelData() const { return modelData; }

protected:
    void CreateBuffers(ID3D12Device *device);
    // 派生クラスの頂点生成（GenerateModelData）用アクセサ
    ModelData &GetModelDataRef() { return modelData; }

private:

    ModelData modelData;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
};
