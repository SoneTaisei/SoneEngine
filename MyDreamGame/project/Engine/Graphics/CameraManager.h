#pragma once
#include "Core/Utility/Structs.h"

class CameraManager {
public:
    static CameraManager *GetInstance() {
        static CameraManager instance;
        return &instance;
    }

    // カメラ情報の更新（カメラクラスから呼ばれる）
    void SetCameraInfo(const Vector3 &pos, const Matrix4x4 &view, const Matrix4x4 &projection) {
        cameraPos_ = pos;
        viewMatrix_ = view;
        projectionMatrix_ = projection;
    }

    // ゲッター
    const Vector3 &GetCameraPos() const { return cameraPos_; }
    const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }

private:
    CameraManager() = default;
    Vector3 cameraPos_{};
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};
};