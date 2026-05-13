#pragma once
#include "Camera.h"

// 普通のカメラ（将来プレイヤー追従などを入れる場所）
class GameCamera : public Camera {
public:
    void Initialize(int kClientWidth, int kClientHeight) override;
    void Update(); // 特に操作はないが、追従処理などをここに書く
    void UpdateMatrix() override;

    // 2Dモード用：正射影カメラとして初期化
    void InitializeOrthographic(int kClientWidth, int kClientHeight, float viewWidth, float viewHeight);

    // 追従ターゲットの設定（2Dスクロール用）
    void SetFollowTarget(const Vector3* target) { followTarget_ = target; }

    // 2Dモードかどうか
    bool IsOrthographic() const { return isOrthographic_; }

    // 正射影のビューサイズを設定
    void SetOrthoViewSize(float width, float height) { orthoWidth_ = width; orthoHeight_ = height; }

private:
    // 正射影行列でUpdateMatrixをオーバーライド的に使う
    void UpdateMatrixOrthographic();

    const Vector3* followTarget_ = nullptr; // 追従ターゲット
    bool isOrthographic_ = false;

    float orthoWidth_ = 20.0f;  // 正射影の横幅（ワールド座標単位）
    float orthoHeight_ = 11.25f; // 正射影の縦幅

    // カメラ追従の滑らかさ
    float followLerp_ = 0.1f;
};