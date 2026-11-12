#include "CameraManager.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>

void CameraManager::Initialize(ID3D12Device* device) {
    if (cameraResource_) return;

    cameraResource_ = CreateBufferResource(device, (sizeof(CameraForGPU) + 255) & ~255u);
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCamera_));
    UpdateBuffer();
}

void CameraManager::UpdateBuffer() {
    if (mappedCamera_) {
        mappedCamera_->worldPosition = cameraPos_;

        // ビュー行列の逆行列からカメラのワールド前方ベクトル（Z方向）を取得
        Matrix4x4 invView = TransformFunctions::Inverse(viewMatrix_);
        Vector3 fwd = { invView.m[2][0], invView.m[2][1], invView.m[2][2] };
        float lenSq = fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z;
        if (lenSq > 1e-6f) {
            float invLen = 1.0f / std::sqrt(lenSq);
            fwd.x *= invLen;
            fwd.y *= invLen;
            fwd.z *= invLen;
        } else {
            fwd = { 0.0f, 0.0f, 1.0f };
        }
        mappedCamera_->forward = fwd;

        // 正射影判定（m[3][3] が 1.0f なら直交投影、0.0f なら透視投影）
        mappedCamera_->isOrthographic = (std::abs(projectionMatrix_.m[3][3] - 1.0f) < 0.1f) ? 1.0f : 0.0f;
    }
}
