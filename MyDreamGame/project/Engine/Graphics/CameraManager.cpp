#include "CameraManager.h"
#include "Core/Utility/UtilityFunctions.h"
#include <cassert>

void CameraManager::Initialize(ID3D12Device* device) {
    if (cameraResource_) return;

    cameraResource_ = CreateBufferResource(device, (sizeof(CameraForGPU) + 255) & ~255u);
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCamera_));
    UpdateBuffer();
}

void CameraManager::UpdateBuffer() {
    if (mappedCamera_) {
        mappedCamera_->worldPosition = cameraPos_;
    }
}
