#include "PrimitiveTorus.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveTorus::PrimitiveTorus(float ringRadius, float tubeRadius, uint32_t segments) : ringRadius_(ringRadius), tubeRadius_(tubeRadius), segments_(segments) {
}

void PrimitiveTorus::GenerateModelData() {

    for (uint32_t i = 0; i <= segments_; ++i) {
        float theta = 2.0f * std::numbers::pi_v<float> * i / segments_;
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);

        for (uint32_t j = 0; j <= segments_; ++j) {
            float phi = 2.0f * std::numbers::pi_v<float> * j / segments_;
            float cosPhi = cosf(phi);
            float sinPhi = sinf(phi);

            VertexData v{};
            v.position.x = (ringRadius_ + tubeRadius_ * cosPhi) * cosTheta;
            v.position.y = tubeRadius_ * sinPhi;
            v.position.z = (ringRadius_ + tubeRadius_ * cosPhi) * sinTheta;
            v.position.w = 1.0f;

            Vector3 center = {ringRadius_ * cosTheta, 0.0f, ringRadius_ * sinTheta};
            v.normal = TransformFunctions::Normalize({v.position.x - center.x, v.position.y - center.y, v.position.z - center.z});
            v.texcoord = {static_cast<float>(i) / segments_, static_cast<float>(j) / segments_};
            v.color = {1.0f, 1.0f, 1.0f, 1.0f};

            modelData_.vertices.push_back(v);
        }
    }

    for (uint32_t i = 0; i < segments_; ++i) {
        for (uint32_t j = 0; j < segments_; ++j) {
            uint32_t first = i * (segments_ + 1) + j;
            uint32_t second = first + segments_ + 1;

            modelData_.indices.push_back(first);
            modelData_.indices.push_back(first + 1);
            modelData_.indices.push_back(second);

            modelData_.indices.push_back(second);
            modelData_.indices.push_back(first + 1);
            modelData_.indices.push_back(second + 1);
        }
    }

}
