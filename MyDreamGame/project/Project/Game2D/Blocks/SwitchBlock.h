#pragma once
#include "BaseBlock.h"

class SwitchBlock : public BaseBlock {
public:
    SwitchBlock(MapChip2D* map, int chipX, int chipY);
    ~SwitchBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    
    // スイッチ自体はすり抜ける（当たり判定なし）
    bool IsSolid() const override { return false; }
    
    void OnCollision(Player2D* player) override;
    // 鎖（投げた鎖・落とした鎖・宝石）が乗っている間も押される
    void OnChainTouch(const Vector3& pos, float radius, float speed) override;
    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（押下タイマーを保存・復元する）
    bool IsReplayTracked() const override { return true; }
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

    int GetLinkId() const { return linkId_; }
    bool IsPressed() const { return isPressed_; }

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float startWidth_ = 1.0f;
    float startHeight_ = 1.0f;

    int linkId_ = 1;
    bool isPressed_ = false;
    float pressedTimer_ = 0.0f;
};