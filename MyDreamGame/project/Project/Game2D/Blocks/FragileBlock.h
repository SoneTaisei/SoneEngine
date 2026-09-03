#pragma once
#include "BaseBlock.h"

class FragileBlock : public BaseBlock {
public:
    FragileBlock(MapChip2D* map, int chipX, int chipY);
    ~FragileBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    
    bool IsSolid() const override { return !isDestroyed_; }
    void OnPlayerStand(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（崩壊タイマーを保存・復元する）
    bool IsReplayTracked() const override { return true; }
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;

    int breakWeight_ = 4;        // 崩れるために必要な鎖の数
    float breakDuration_ = 0.5f; // 崩れるまでの時間

    bool isBreaking_ = false;
    float breakTimer_ = 0.0f;
};
