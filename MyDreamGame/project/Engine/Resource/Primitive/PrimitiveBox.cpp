#include "PrimitiveBox.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveBox::PrimitiveBox(float size) : size_(size) {
}

void PrimitiveBox::GenerateModelData() {

    float h = size_ * 0.5f;
    modelData_.vertices = {
        // Front
        {{-h, -h,  h, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h, -h,  h, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h,  h,  h, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h,  h,  h, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        // Back
        {{ h, -h, -h, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h, -h, -h, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h,  h, -h, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h,  h, -h, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        // Left
        {{-h, -h, -h, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h, -h,  h, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h,  h, -h, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h,  h,  h, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        // Right
        {{ h, -h,  h, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h, -h, -h, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h,  h,  h, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h,  h, -h, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        // Top
        {{-h,  h,  h, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h,  h,  h, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h,  h, -h, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h,  h, -h, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        // Bottom
        {{-h, -h, -h, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h, -h, -h, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h, -h,  h, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h, -h,  h, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
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
