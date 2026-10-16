#include "PrimitiveSphere.h"
#include "Core/Utility/UtilityFunctions.h"

PrimitiveSphere::PrimitiveSphere(float size, uint32_t segments) : size_(size), segments_(segments) {
}

void PrimitiveSphere::GenerateModelData() {
    auto &modelData = GetModelDataRef();

    CreateSphereMesh(modelData.vertices, modelData.indices, size_, segments_, segments_);

}
