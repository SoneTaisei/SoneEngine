#include "PrimitiveCone.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveCone::PrimitiveCone(float radius, float height, uint32_t segments) : radius_(radius), height_(height), segments_(segments) {
}

void PrimitiveCone::GenerateModelData() {

    float halfH = height_ * 0.5f;
    VertexData tip{};
    tip.position = {0.0f, halfH, 0.0f, 1.0f};
    tip.normal = {0.0f, 1.0f, 0.0f};
    tip.texcoord = {0.5f, 0.0f};
    modelData_.vertices.push_back(tip);

    for (uint32_t i = 0; i <= segments_; ++i) {
        float angle = 2.0f * std::numbers::pi_v<float> * i / segments_;
        float cosA = cosf(angle);
        float sinA = sinf(angle);

        VertexData v{};
        v.position = {radius_ * cosA, -halfH, radius_ * sinA, 1.0f};
        v.normal = TransformFunctions::Normalize({cosA, radius_ / height_, sinA});
        v.texcoord = {static_cast<float>(i) / segments_, 1.0f};
        v.color = {1.0f, 1.0f, 1.0f, 1.0f};
        modelData_.vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= segments_; ++i) {
        modelData_.indices.push_back(0);
        modelData_.indices.push_back(i);
        modelData_.indices.push_back(i + 1);
    }

}
