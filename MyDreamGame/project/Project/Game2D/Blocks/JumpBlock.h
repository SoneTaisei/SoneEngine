#pragma once
#include "BaseBlock.h"

class JumpBlock : public BaseBlock {
public:
    enum class BounceDirection {
        kUp,
        kDown,
        kLeft,
        kRight,
        kCenter
    };

    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    
    // 当たり判定：すり抜けないようにする
    bool IsSolid() const override { return true; }

    // 毎フレームの更新（バウンドアニメーションのシミュレーション）
    void Update() override;

    // リセット処理（アニメーション状態を戻す）
    void Reset() override;

    // コライダー・判定用AABB（アニメーション変形の影響を受けない固定AABB）
    AABB2D GetAABB() const override;

    // プレイヤーが上に乗った際の処理
    void OnPlayerStand() override;

    // プレイヤーと接触した際の処理
    void OnCollision(Player2D* player) override;

    // Jsonプロパティの受け取り
    void SetProperties(const nlohmann::json& properties) override;

    // バウンドアニメーションのトリガー
    void TriggerBounce(BounceDirection dir);

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    float jumpVelocityVertical_ = 15.0f; // 縦ジャンプの威力
    float jumpVelocityHorizontal_ = 15.0f; // 横ジャンプの威力

    // バウンドアニメーション用変数（物理スプリングシミュレーション）
    bool isBouncing_ = false;
    float bounceAmount_ = 0.0f;     // 主軸の変位（マイナスで縮み、プラスで伸び）
    float bounceVelocity_ = 0.0f;   // 主軸の変位速度
    BounceDirection bounceDir_ = BounceDirection::kUp;

    // ベースTransform（初期配置基準）
    Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
    AABB2D baseAABB_{};
    bool baseCaptured_ = false;

    // アニメーション設定パラメータ
    float stiffness_ = 280.0f;      // バネの硬さ（周波数）
    float damping_ = 14.0f;         // 減衰率（余韻の長さ）
    float initialSquash_ = -0.42f;  // 発動時の縮み変位
    float initialVelocity_ = 12.0f; // 発動時の伸びようとする初速
    float crossScaleRatio_ = 0.35f; // 垂直軸の拡縮比（Squash & Stretch）
};
