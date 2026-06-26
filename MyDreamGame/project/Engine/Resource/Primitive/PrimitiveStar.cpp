#include "PrimitiveStar.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveStar::PrimitiveStar(float size) : size_(size) {
}

void PrimitiveStar::GenerateModelData() {

    float maxRadius = size_ * 0.5f;
    float minRadius = size_ * 0.15f;
    
    VertexData center{};
    center.position = {0.0f, 0.0f, 0.0f, 1.0f};
    center.normal = {0.0f, 0.0f, -1.0f};
    center.texcoord = {0.5f, 0.5f};
    center.color = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow center
    modelData_.vertices.push_back(center);

    for (uint32_t i = 0; i < 8; ++i) {
        float angle = i * std::numbers::pi_v<float> / 4.0f;
        float radius = (i % 2 == 0) ? maxRadius : minRadius;
        
        VertexData v{};
        v.position = {radius * cosf(angle), radius * sinf(angle), 0.0f, 1.0f};
        v.normal = {0.0f, 0.0f, -1.0f};
        v.texcoord = {0.5f + 0.5f * (radius/maxRadius) * cosf(angle), 0.5f - 0.5f * (radius/maxRadius) * sinf(angle)};
        v.color = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow edges
        modelData_.vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= 8; ++i) {
        modelData_.indices.push_back(0);
        modelData_.indices.push_back(i);
        modelData_.indices.push_back((i % 8) + 1);
    }

}
