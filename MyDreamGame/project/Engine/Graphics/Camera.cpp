#include "Camera.h"
#include "Core/Utility/TransformFunctions.h" // 行列計算関数
#include "CameraManager.h" // カメラ情報を管理するクラス

void Camera::Initialize(int kClientWidth, int kClientHeight) {
    kClientWidth_ = kClientWidth;
    kClientHeight_ = kClientHeight;
    UpdateMatrix(); // 最初の行列を作っておく
}

void Camera::SetResolution(int kClientWidth, int kClientHeight) {
    kClientWidth_ = kClientWidth;
    kClientHeight_ = kClientHeight;
    UpdateMatrix();
}

void Camera::UpdateMatrix() {
    // ビュー行列の作成 (位置と回転を反映)
    viewMatrix_ = TransformFunctions::MakeViewMatrix(transform_.rotate, transform_.translate);

    // 射影行列の計算 (nearClip_, farClip_ を使用)
    projectionMatrix_ = TransformFunctions::MakePerspectiveFovMatrix(
        fov_,
        float(kClientWidth_) / float(kClientHeight_),
        nearClip_, farClip_
    );

    CameraManager::GetInstance()->SetCameraInfo(transform_.translate, viewMatrix_, projectionMatrix_);
}