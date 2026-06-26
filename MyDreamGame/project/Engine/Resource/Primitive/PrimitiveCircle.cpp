#include "PrimitiveCircle.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveCircle::PrimitiveCircle(float size, uint32_t segments) : size_(size), segments_(segments) {
}

void PrimitiveCircle::GenerateModelData() {

    VertexData center{};
    center.position = {0.0f, 0.0f, 0.0f, 1.0f};
    center.normal = {0.0f, 0.0f, 1.0f};
    center.texcoord = {0.5f, 0.5f};
    center.color = {1.0f, 1.0f, 1.0f, 1.0f};
    modelData_.vertices.push_back(center);

    for (uint32_t i = 0; i <= segments_; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments_;
        VertexData v{};
        v.position = {size_ * cosf(angle), size_ * sinf(angle), 0.0f, 1.0f};
        v.normal = {0.0f, 0.0f, 1.0f};
        v.texcoord = {0.5f + 0.5f * cosf(angle), 0.5f - 0.5f * sinf(angle)};
        v.color = {1.0f, 1.0f, 1.0f, 1.0f};
        modelData_.vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= segments_; ++i) {
        modelData_.indices.push_back(0);
        modelData_.indices.push_back(i);
        modelData_.indices.push_back(i + 1);
    }

}
