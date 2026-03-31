#pragma once
#include "Camera.h" // 親クラスをインクルード

// Cameraクラスを継承する
class DebugCamera : public Camera {
public:
    // 初期化（親クラスのInitializeを呼ぶだけなら省略可能だが、保存用変数の初期化のために記述）
    void Initialize(int kClientWidth, int kClientHeight) override;

    // 更新処理（入力処理を行う）
    void Update();

private:
    // リセット用に初期値を覚えておく変数
    Vector3 initialRotation_;
    Vector3 initialTranslation_;
};