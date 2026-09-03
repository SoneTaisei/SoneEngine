#pragma once
#include "BaseBlock.h"

class DoorBlock : public BaseBlock {
public:
    DoorBlock(MapChip2D* map, int chipX, int chipY);
    ~DoorBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    
    // ドアは常に動く床扱い（スケールに応じてAABBが変化するため）
    bool IsSolid() const override { return true; }
    bool IsMoving() const override { return true; }
    
    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（開閉の進行度を保存・復元する）
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float startWidth_ = 1.0f;
    float startHeight_ = 1.0f;

    int linkId_ = 1;
    float openProgress_ = 0.0f; // 0.0f (閉) ～ 1.0f (開)
    float openSpeed_ = 2.0f;    // 開く速度
    float closeSpeed_ = 2.0f;   // 閉まる速度
};