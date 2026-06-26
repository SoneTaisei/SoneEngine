#include "PrimitiveCylinder.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveCylinder::PrimitiveCylinder(float radius, float height, uint32_t segments) : radius_(radius), height_(height), segments_(segments) {
}

void PrimitiveCylinder::GenerateModelData() {

    float kTopRadius = radius_;
    float kBottomRadius = radius_;
    float kHeight = height_;
    uint32_t kCylinderDivide = segments_;
    float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide);

    for (uint32_t index = 0; index < kCylinderDivide; ++index) {
        float sinVal = std::sin(index * radianPerDivide);
        float cosVal = std::cos(index * radianPerDivide);
        float sinNext = std::sin((index + 1) * radianPerDivide);
        float cosNext = std::cos((index + 1) * radianPerDivide);
        float u = float(index) / float(kCylinderDivide);
        float uNext = float(index + 1) / float(kCylinderDivide);

        // UV flip
        float vTop = 1.0f;
        float vBottom = 0.0f;

        // 6 vertices per segment
        VertexData v[6];
        
        // 1: Top (index)
        v[0].position = {-sinVal * kTopRadius, kHeight, cosVal * kTopRadius, 1.0f};
        v[0].texcoord = {u, vTop};
        v[0].normal = {-sinVal, 0.0f, cosVal};
        v[0].color = {1.0f, 1.0f, 1.0f, 1.0f};

        // 2: Top (index+1)
        v[1].position = {-sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f};
        v[1].texcoord = {uNext, vTop};
        v[1].normal = {-sinNext, 0.0f, cosNext};
        v[1].color = {1.0f, 1.0f, 1.0f, 1.0f};

        // 3: Bottom (index)
        v[2].position = {-sinVal * kBottomRadius, 0.0f, cosVal * kBottomRadius, 1.0f};
        v[2].texcoord = {u, vBottom};
        v[2].normal = {-sinVal, 0.0f, cosVal};
        v[2].color = {1.0f, 1.0f, 1.0f, 1.0f};

        // 4: Bottom (index)
        v[3].position = {-sinVal * kBottomRadius, 0.0f, cosVal * kBottomRadius, 1.0f};
        v[3].texcoord = {u, vBottom};
        v[3].normal = {-sinVal, 0.0f, cosVal};
        v[3].color = {1.0f, 1.0f, 1.0f, 1.0f};

        // 5: Top (index+1)
        v[4].position = {-sinNext * kTopRadius, kHeight, cosNext * kTopRadius, 1.0f};
        v[4].texcoord = {uNext, vTop};
        v[4].normal = {-sinNext, 0.0f, cosNext};
        v[4].color = {1.0f, 1.0f, 1.0f, 1.0f};

        // 6: Bottom (index+1)
        v[5].position = {-sinNext * kBottomRadius, 0.0f, cosNext * kBottomRadius, 1.0f};
        v[5].texcoord = {uNext, vBottom};
        v[5].normal = {-sinNext, 0.0f, cosNext};
        v[5].color = {1.0f, 1.0f, 1.0f, 1.0f};

        uint32_t baseIndex = static_cast<uint32_t>(modelData_.vertices.size());
        for (int j = 0; j < 6; ++j) {
            modelData_.vertices.push_back(v[j]);
            modelData_.indices.push_back(baseIndex + j);
        }
    }

}
