#include "PrimitivePlane.h"

PrimitivePlane::PrimitivePlane(float size) : size_(size) {
}

void PrimitivePlane::GenerateModelData() {
    auto &modelData = GetModelDataRef();

    float halfSize = size_ * 0.5f;
    modelData.vertices = {
        {{-halfSize, 0.0f,  halfSize, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ halfSize, 0.0f,  halfSize, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{-halfSize, 0.0f, -halfSize, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
        {{ halfSize, 0.0f, -halfSize, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    modelData.indices = {0, 1, 2, 2, 1, 3};

}
