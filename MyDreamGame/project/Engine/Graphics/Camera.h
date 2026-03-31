#pragma once
#include "Core/Utility/Structs.h" // Vector3, Matrix4x4 等の定義

class Camera {
public:
    // コンストラクタ・デストラクタ
    Camera() = default;
    virtual ~Camera() = default;

    // 初期化（画面サイズを受け取る）
    virtual void Initialize(int kClientWidth, int kClientHeight);

    // 行列を更新する（位置や角度が変わったら呼ぶ）
    void UpdateMatrix();

    // --- ゲッター ---
    const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }
    const Vector3 &GetRotation() const { return transform_.rotate; }
    const Vector3 &GetTranslation() const { return transform_.translate; }

    // --- セッター（外部からカメラを動かす用） ---
    void SetRotation(const Vector3 &rotation) { transform_.rotate = rotation; }
    void SetTranslation(const Vector3 &translation) { transform_.translate = translation; }
    void SetFov(float fov) { fov_ = fov; }

protected: // 継承先（DebugCamera）でも使えるように private ではなく protected にする
    // Transform構造体を使うと管理が楽です（無ければVector3 rotation, translationでもOK）
    Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };

    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;

    // 射影行列計算用メンバ
    int kClientWidth_ = 1280;
    int kClientHeight_ = 720;
    float fov_ = 0.45f;
};