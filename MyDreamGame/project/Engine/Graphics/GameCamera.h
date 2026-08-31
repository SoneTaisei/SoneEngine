#pragma once
#include "Camera.h"

// 普通のカメラ（将来プレイヤー追従などを入れる場所）
class GameCamera : public Camera {
public:
    void Initialize(int kClientWidth, int kClientHeight) override;
    void Update(); // 特に操作はないが、追従処理などをここに書く
    void UpdateMatrix() override;

    // 画面揺れの開始
    void Shake(float strength, float duration);

    // 2Dモード用：正射影カメラとして初期化
    void InitializeOrthographic(int kClientWidth, int kClientHeight, float viewWidth, float viewHeight);

    // 追従ターゲットの設定（2Dスクロール用）
    void SetFollowTarget(const Vector3* target) { followTarget_ = target; }

    // 2Dモードかどうか
    bool IsOrthographic() const { return isOrthographic_; }

    // 正射影のビューサイズを設定
    void SetOrthoViewSize(float width, float height) { orthoWidth_ = width; orthoHeight_ = height; }
    float GetOrthoWidth() const { return orthoWidth_; }
    float GetOrthoHeight() const { return orthoHeight_; }

    // ルーム遷移中かどうか
    bool IsTransitioning() const { return isTransitioning_; }
    
    // 現在のルーム座標を取得
    int GetCurrentRoomX() const { return currentRoomX_; }
    int GetCurrentRoomY() const { return currentRoomY_; }

    // カスタム境界線データ（フリップスクロール用）
    void SetRooms(const std::vector<StageRoom>& rooms) {
        rooms_ = rooms;
    }

    float GetFollowLerp() const { return followLerp_; }
    void SetFollowLerp(float lerp) { followLerp_ = lerp; }

    float GetTransitionLerp() const { return transitionLerp_; }
    void SetTransitionLerp(float lerp) { transitionLerp_ = lerp; }

    float GetScale() const { return scale_; }
    void SetScale(float scale) {
        scale_ = (scale > 0.01f) ? scale : 0.01f;
        orthoWidth_ = 20.0f / scale_;
        orthoHeight_ = 11.25f / scale_;
    }

    void LoadConfig(const std::string& filepath = "resources/json/shared/camera_config.json");
    void SaveConfig(const std::string& filepath = "resources/json/shared/camera_config.json");

private:
    // 正射影行列でUpdateMatrixをオーバーライド的に使う
    void UpdateMatrixOrthographic();

    const Vector3* followTarget_ = nullptr; // 追従ターゲット
    bool isOrthographic_ = false;

    float orthoWidth_ = 20.0f;  // 正射影の横幅（ワールド座標単位）
    float orthoHeight_ = 11.25f; // 正射影の縦幅
    float scale_ = 1.0f;         // 正射影のスケール（拡大率）

    std::vector<StageRoom> rooms_;

    // カメラ追従の滑らかさ
    float followLerp_ = 0.1f;
    float transitionLerp_ = 0.15f; // 画面切り替え時のカメラスピード

    // ルーム管理用
    int currentRoomX_ = 0;
    int currentRoomY_ = 0;
    bool isTransitioning_ = false;

    // 画面揺れ用
    float shakeStrength_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimer_ = 0.0f;
    Vector3 shakeOffset_ = {0.0f, 0.0f, 0.0f};
};