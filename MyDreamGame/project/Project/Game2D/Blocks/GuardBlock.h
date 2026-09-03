#pragma once
#include "BaseBlock.h"

class GuardBlock : public BaseBlock {
public:
    enum class State {
        Patrol,
        Wait,
        Alert
    };

    GuardBlock(MapChip2D* map, int chipX, int chipY);
    ~GuardBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void Draw() override;
    
    // 警備員本体は触れるとデスするためソリッド（または独自の当たり判定）
    bool IsSolid() const override { return false; }
    bool IsMoving() const override { return true; }
    
    Vector3 GetVelocity() const override { return currentVelocity_; }

    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（巡回状態・警戒ゲージ・向きを保存・復元する）
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

    void OnCollision(Player2D* player) override;

    // 視界領域を取得
    AABB2D GetSightAABB() const;
    
    // プレイヤーが視界に入った時に呼ばれる
    void OnSpottedPlayer(Player2D* player);

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float startWidth_ = 1.0f;
    float startHeight_ = 1.0f;

    Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 deltaPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 currentVelocity_ = {0.0f, 0.0f, 0.0f};

    // パラメータ
    float moveRange_ = 3.0f;      // 片道への最大移動距離
    float patrolSpeed_ = 1.5f;    // パトロール時の速度
    float alertSpeed_ = 3.0f;     // 警戒時の速度
    float sightLength_ = 4.0f;    // 視界の長さ
    float sightHeight_ = 1.0f;    // 視界の高さ
    float maxAlertGauge_ = 1.5f;  // 見つかってからゲームオーバーになるまでの時間(秒)
    int startDirection_ = 1;      // 初期の向き(1:右, -1:左)
    float waitTimeAtEdge_ = 1.0f; // 端に到達した時の待機時間(秒)

    // 状態
    State state_ = State::Patrol;
    int direction_ = 1;           // 1: 右, -1: 左
    float currentFacing_ = 1.0f;  // 滑らかな振り向き用（1.0 ~ -1.0）
    float alertGauge_ = 0.0f;
    float waitTimer_ = 0.0f;
    bool isPlayerInSightThisFrame_ = false;

    // 視界描画用
    std::unique_ptr<GameObject> sightObject_;
};