#include "Primitive.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include <cassert>
#include "Core/Utility/UtilityFunctions.h"

void Primitive::Initialize(ID3D12Device* device) {
    modelData_.vertices.clear();
    modelData_.indices.clear();

    GenerateModelData();
    CreateBuffers(device);
}

void Primitive::Draw() {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(static_cast<UINT>(modelData_.indices.size()), 1, 0, 0, 0);
}

void Primitive::CreateBuffers(ID3D12Device* device) {
    HRESULT hr;
    vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    assert(SUCCEEDED(hr));
    std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
    vertexResource_->Unmap(0, nullptr);

    indexResource_ = CreateBufferResource(device, sizeof(uint32_t) * modelData_.indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* indexData = nullptr;
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    assert(SUCCEEDED(hr));
    std::memcpy(indexData, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
    indexResource_->Unmap(0, nullptr);
}
