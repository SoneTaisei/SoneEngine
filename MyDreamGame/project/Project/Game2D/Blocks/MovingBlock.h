#pragma once
#include "BaseBlock.h"
#include <string>

class MovingBlock : public BaseBlock {
public:
    MovingBlock(MapChip2D* map, int chipX, int chipY);
    ~MovingBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    
    bool IsSolid() const override { return true; }
    bool IsMoving() const override { return true; }
    Vector3 GetVelocity() const override { return currentVelocity_; }
    void OnPlayerStand(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;

    // リプレイ復元用（位相タイマーを保存・復元する）
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 deltaPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 currentVelocity_ = {0.0f, 0.0f, 0.0f};

    std::string moveAxis_ = "X"; // "X" or "Y"
    float moveRange_ = 3.0f;
    float moveSpeed_ = 2.0f;
    float timer_ = 0.0f;      // ゲーム内時刻（ReplayManager の共有クロックと同期する）
    bool hasPrevPosition_ = false; // 初回更新かどうか（速度の跳ね上がり防止）

    // 現在の時刻から座標を求める（時間の純粋な関数にすることで再生・シークでも一致する）
    Vector3 CalcPositionAt(float time) const;
};
