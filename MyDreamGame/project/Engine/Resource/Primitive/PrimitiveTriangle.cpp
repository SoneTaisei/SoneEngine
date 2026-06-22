#include "PrimitiveTriangle.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <numbers>

PrimitiveTriangle::PrimitiveTriangle(float size) : size_(size) {
}

void PrimitiveTriangle::GenerateModelData() {

    float h = size_ * 0.5f;
    modelData_.vertices = {
        {{ 0.0f,  h, 0.0f, 1.0f}, {0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h, -h, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h, -h, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    modelData_.indices = {0, 1, 2};

}
