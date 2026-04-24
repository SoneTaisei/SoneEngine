#include "Primitive.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cassert>
#include <cmath>
#include <numbers>

void Primitive::Initialize(ID3D12Device* device, PrimitiveType type, float size, uint32_t segments) {
    modelData_.vertices.clear();
    modelData_.indices.clear();

    switch (type) {
    case PrimitiveType::Plane:
        CreatePlane(size);
        break;
    case PrimitiveType::Box:
        CreateBox(size);
        break;
    case PrimitiveType::Sphere:
        CreateSphere(size, segments);
        break;
    case PrimitiveType::Circle:
        CreateCircle(size, segments);
        break;
    case PrimitiveType::Ring:
        CreateRing(size * 0.5f, size, segments);
        break;
    case PrimitiveType::Cylinder:
        CreateCylinder(size, size * 2.0f, segments);
        break;
    case PrimitiveType::Cone:
        CreateCone(size, size * 2.0f, segments);
        break;
    case PrimitiveType::Torus:
        CreateTorus(size, size * 0.3f, segments);
        break;
    case PrimitiveType::Triangle:
        CreateTriangle(size);
        break;
    }

    CreateBuffers(device);
}

void Primitive::Draw(ID3D12GraphicsCommandList* commandList) {
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(static_cast<UINT>(modelData_.indices.size()), 1, 0, 0, 0);
}

void Primitive::CreatePlane(float size) {
    float halfSize = size * 0.5f;
    modelData_.vertices = {
        {{-halfSize, 0.0f,  halfSize, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize, 0.0f,  halfSize, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize, 0.0f, -halfSize, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize, 0.0f, -halfSize, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    };
    modelData_.indices = {0, 1, 2, 2, 1, 3};
}

void Primitive::CreateBox(float size) {
    float h = size * 0.5f;
    modelData_.vertices = {
        // Front
        {{-h, -h,  h, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{ h, -h,  h, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-h,  h,  h, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ h,  h,  h, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // Back
        {{ h, -h, -h, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-h, -h, -h, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{ h,  h, -h, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{-h,  h, -h, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        // Left
        {{-h, -h, -h, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-h, -h,  h, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-h,  h, -h, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-h,  h,  h, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        // Right
        {{ h, -h,  h, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ h, -h, -h, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ h,  h,  h, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ h,  h, -h, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        // Top
        {{-h,  h,  h, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{ h,  h,  h, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-h,  h, -h, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ h,  h, -h, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        // Bottom
        {{-h, -h, -h, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{ h, -h, -h, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{-h, -h,  h, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{ h, -h,  h, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    };
    for (int i = 0; i < 6; ++i) {
        uint32_t offset = i * 4;
        modelData_.indices.push_back(offset + 0);
        modelData_.indices.push_back(offset + 1);
        modelData_.indices.push_back(offset + 2);
        modelData_.indices.push_back(offset + 2);
        modelData_.indices.push_back(offset + 1);
        modelData_.indices.push_back(offset + 3);
    }
}

void Primitive::CreateSphere(float size, uint32_t segments) {
    CreateSphereMesh(modelData_.vertices, modelData_.indices, size, segments, segments);
}

void Primitive::CreateCircle(float size, uint32_t segments) {
    VertexData center{};
    center.position = {0.0f, 0.0f, 0.0f, 1.0f};
    center.normal = {0.0f, 0.0f, 1.0f};
    center.texcoord = {0.5f, 0.5f};
    modelData_.vertices.push_back(center);

    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments;
        VertexData v{};
        v.position = {size * cosf(angle), size * sinf(angle), 0.0f, 1.0f};
        v.normal = {0.0f, 0.0f, 1.0f};
        v.texcoord = {0.5f + 0.5f * cosf(angle), 0.5f - 0.5f * sinf(angle)};
        modelData_.vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= segments; ++i) {
        modelData_.indices.push_back(0);
        modelData_.indices.push_back(i);
        modelData_.indices.push_back(i + 1);
    }
}

void Primitive::CreateRing(float innerRadius, float outerRadius, uint32_t segments) {
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments;
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        VertexData vInner{}, vOuter{};
        vInner.position = {innerRadius * cosA, innerRadius * sinA, 0.0f, 1.0f};
        vInner.normal = {0.0f, 0.0f, 1.0f};
        vInner.texcoord = {0.5f + 0.5f * innerRadius / outerRadius * cosA, 0.5f - 0.5f * innerRadius / outerRadius * sinA};

        vOuter.position = {outerRadius * cosA, outerRadius * sinA, 0.0f, 1.0f};
        vOuter.normal = {0.0f, 0.0f, 1.0f};
        vOuter.texcoord = {0.5f + 0.5f * cosA, 0.5f - 0.5f * sinA};

        modelData_.vertices.push_back(vInner);
        modelData_.vertices.push_back(vOuter);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        modelData_.indices.push_back(base);
        modelData_.indices.push_back(base + 1);
        modelData_.indices.push_back(base + 2);

        modelData_.indices.push_back(base + 2);
        modelData_.indices.push_back(base + 1);
        modelData_.indices.push_back(base + 3);
    }
}

void Primitive::CreateCylinder(float radius, float height, uint32_t segments) {
    float halfH = height * 0.5f;
    // Side
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments;
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        VertexData vBottom{}, vTop{};
        vBottom.position = {radius * cosA, -halfH, radius * sinA, 1.0f};
        vBottom.normal = {cosA, 0.0f, sinA};
        vBottom.texcoord = {static_cast<float>(i) / segments, 1.0f};

        vTop.position = {radius * cosA, halfH, radius * sinA, 1.0f};
        vTop.normal = {cosA, 0.0f, sinA};
        vTop.texcoord = {static_cast<float>(i) / segments, 0.0f};

        modelData_.vertices.push_back(vBottom);
        modelData_.vertices.push_back(vTop);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        modelData_.indices.push_back(base);
        modelData_.indices.push_back(base + 1);
        modelData_.indices.push_back(base + 2);
        modelData_.indices.push_back(base + 2);
        modelData_.indices.push_back(base + 1);
        modelData_.indices.push_back(base + 3);
    }
    // Caps could be added here if needed
}

void Primitive::CreateCone(float radius, float height, uint32_t segments) {
    float halfH = height * 0.5f;
    VertexData tip{};
    tip.position = {0.0f, halfH, 0.0f, 1.0f};
    tip.normal = {0.0f, 1.0f, 0.0f};
    tip.texcoord = {0.5f, 0.0f};
    modelData_.vertices.push_back(tip);

    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments;
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        VertexData v{};
        v.position = {radius * cosA, -halfH, radius * sinA, 1.0f};
        v.normal = TransformFunctions::Normalize({cosA, radius / height, sinA});
        v.texcoord = {static_cast<float>(i) / segments, 1.0f};
        modelData_.vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= segments; ++i) {
        modelData_.indices.push_back(0);
        modelData_.indices.push_back(i);
        modelData_.indices.push_back(i + 1);
    }
}

void Primitive::CreateTorus(float ringRadius, float tubeRadius, uint32_t segments) {
    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = 2.0f * std::numbers::pi_v<float> * i / segments;
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);

        for (uint32_t j = 0; j <= segments; ++j) {
            float phi = 2.0f * std::numbers::pi_v<float> * j / segments;
            float cosPhi = cosf(phi);
            float sinPhi = sinf(phi);

            VertexData v{};
            v.position.x = (ringRadius + tubeRadius * cosPhi) * cosTheta;
            v.position.y = tubeRadius * sinPhi;
            v.position.z = (ringRadius + tubeRadius * cosPhi) * sinTheta;
            v.position.w = 1.0f;

            Vector3 center = {ringRadius * cosTheta, 0.0f, ringRadius * sinTheta};
            v.normal = TransformFunctions::Normalize({v.position.x - center.x, v.position.y - center.y, v.position.z - center.z});
            v.texcoord = {static_cast<float>(i) / segments, static_cast<float>(j) / segments};

            modelData_.vertices.push_back(v);
        }
    }

    for (uint32_t i = 0; i < segments; ++i) {
        for (uint32_t j = 0; j < segments; ++j) {
            uint32_t first = i * (segments + 1) + j;
            uint32_t second = first + segments + 1;

            modelData_.indices.push_back(first);
            modelData_.indices.push_back(first + 1);
            modelData_.indices.push_back(second);

            modelData_.indices.push_back(second);
            modelData_.indices.push_back(first + 1);
            modelData_.indices.push_back(second + 1);
        }
    }
}

void Primitive::CreateTriangle(float size) {
    float h = size * 0.5f;
    modelData_.vertices = {
        {{ 0.0f,  h, 0.0f, 1.0f}, {0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{ h, -h, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-h, -h, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    };
    modelData_.indices = {0, 1, 2};
}

void Primitive::CreateBuffers(ID3D12Device* device) {
    HRESULT hr;
    // Vertex Buffer
    vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    assert(SUCCEEDED(hr));
    std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
    vertexResource_->Unmap(0, nullptr);

    // Index Buffer
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
