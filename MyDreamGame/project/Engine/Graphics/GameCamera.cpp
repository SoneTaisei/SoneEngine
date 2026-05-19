#include "GameCamera.h"
#include "CameraManager.h"
#include "Core/Utility/TransformFunctions.h"

void GameCamera::Initialize(int kClientWidth, int kClientHeight) {
    Camera::Initialize(kClientWidth, kClientHeight);
    // 初期位置の設定（例えばプレイヤーの少し後ろ）
    transform_.translate = { 0.0f, 0.0f, -10.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    UpdateMatrix();
}

void GameCamera::Update() {
    UpdateMatrix();
}

void GameCamera::UpdateMatrix() {
    if (isOrthographic_) {
        UpdateMatrixOrthographic();
    } else {
        Camera::UpdateMatrix();
    }
}

void GameCamera::InitializeOrthographic(int kClientWidth, int kClientHeight, float viewWidth, float viewHeight) {
    kClientWidth_ = kClientWidth;
    kClientHeight_ = kClientHeight;
    isOrthographic_ = true;
    orthoWidth_ = viewWidth;
    orthoHeight_ = viewHeight;

    // 2Dモード：カメラはZ軸上から見下ろす（Z=-10の位置からZ=0を見る）
    transform_.translate = { 0.0f, 5.0f, -10.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };

    UpdateMatrixOrthographic();
}

void GameCamera::UpdateMatrixOrthographic() {
    // ターゲットへの追従
    if (followTarget_) {
        // X座標のみ追従（横スクロール）
        float targetX = followTarget_->x;
        float targetY = followTarget_->y;

        // 緩やかに追従（Lerp）
        transform_.translate.x += (targetX - transform_.translate.x) * followLerp_;
        // Y方向もある程度追従
        float desiredY = targetY; // プレイヤーを画面中心付近に配置
        transform_.translate.y += (desiredY - transform_.translate.y) * followLerp_;

        // Y座標の下限（地面より下にカメラが行かないように）
        if (transform_.translate.y < orthoHeight_ * 0.5f) {
            transform_.translate.y = orthoHeight_ * 0.5f;
        }
    }

    // ビュー行列：カメラ位置から見るだけ（回転なし）
    // 2Dなので単純な平行移動のみ
    viewMatrix_ = TransformFunctions::MakeTranslateMatrix(
        { -transform_.translate.x, -transform_.translate.y, -transform_.translate.z }
    );

    // 正射影行列
    float halfW = orthoWidth_ * 0.5f;
    float halfH = orthoHeight_ * 0.5f;
    projectionMatrix_ = TransformFunctions::MakeOrthographicMatrix(
        -halfW, halfH, halfW, -halfH, 0.1f, 100.0f
    );

    // CameraManagerにも反映
    CameraManager::GetInstance()->SetCameraInfo(transform_.translate, viewMatrix_, projectionMatrix_);
}
