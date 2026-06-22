#include "PrimitivePlane.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitivePlane::PrimitivePlane(float size) : size_(size) {
}

void PrimitivePlane::GenerateModelData() {

    float halfSize = size_ * 0.5f;
    modelData_.vertices = {
        {{-halfSize, 0.0f,  halfSize, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ halfSize, 0.0f,  halfSize, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-halfSize, 0.0f, -halfSize, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ halfSize, 0.0f, -halfSize, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    modelData_.indices = {0, 1, 2, 2, 1, 3};

}
