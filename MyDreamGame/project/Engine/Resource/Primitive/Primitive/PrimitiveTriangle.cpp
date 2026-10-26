#include "PrimitiveTriangle.h"

PrimitiveTriangle::PrimitiveTriangle(float size) : size_(size) {
}

void PrimitiveTriangle::GenerateModelData() {
    auto &modelData = GetModelDataRef();

    float h = size_ * 0.5f;
    modelData.vertices = {
        {{ 0.0f,  h, 0.0f, 1.0f}, {0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ h, -h, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-h, -h, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    modelData.indices = {0, 1, 2};

}
