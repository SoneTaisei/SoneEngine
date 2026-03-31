#include "Camera.h"
#include "Core/Utility/TransformFunctions.h" // 行列計算関数

void Camera::Initialize(int kClientWidth, int kClientHeight) {
    kClientWidth_ = kClientWidth;
    kClientHeight_ = kClientHeight;
    UpdateMatrix(); // 最初の行列を作っておく
}

void Camera::UpdateMatrix() {
    // [DebugCamera.cpp] にあった計算処理をここに移動します

    // 回転行列
    Matrix4x4 rotationMatrix = TransformFunctions::Multiply(
        TransformFunctions::MakeRoteXMatrix(transform_.rotate.x),
        TransformFunctions::MakeRoteYMatrix(transform_.rotate.y)
    );

    // 平行移動行列
    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(
        { -transform_.translate.x, -transform_.translate.y, -transform_.translate.z }
    );

    // 逆回転行列（カメラの向きの逆）
    Matrix4x4 rotateMatrixInv = TransformFunctions::Transpose(rotationMatrix);

    // ビュー行列の合成
    viewMatrix_ = TransformFunctions::Multiply(translateMatrix, rotateMatrixInv);

    // 射影行列の計算
    projectionMatrix_ = TransformFunctions::MakePerspectiveFovMatrix(
        fov_,
        float(kClientWidth_) / float(kClientHeight_),
        0.1f, 100.0f
    );
}