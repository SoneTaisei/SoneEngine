#include "PrimitiveCircle.h"
#include <numbers>

PrimitiveCircle::PrimitiveCircle(float size, uint32_t segments) : size_(size), segments_(segments) {
}

void PrimitiveCircle::GenerateModelData() {
    auto &modelData = GetModelDataRef();

    VertexData center{};
    center.position = {0.0f, 0.0f, 0.0f, 1.0f};
    center.normal = {0.0f, 0.0f, 1.0f};
    center.texcoord = {0.5f, 0.5f};
    center.color = {1.0f, 1.0f, 1.0f, 1.0f};
    modelData.vertices.push_back(center);

    for (uint32_t i = 0; i <= segments_; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments_;
        VertexData v{};
        v.position = {size_ * cosf(angle), size_ * sinf(angle), 0.0f, 1.0f};
        v.normal = {0.0f, 0.0f, 1.0f};
        v.texcoord = {0.5f + 0.5f * cosf(angle), 0.5f - 0.5f * sinf(angle)};
        v.color = {1.0f, 1.0f, 1.0f, 1.0f};
        modelData.vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= segments_; ++i) {
        modelData.indices.push_back(0);
        modelData.indices.push_back(i);
        modelData.indices.push_back(i + 1);
    }

}
