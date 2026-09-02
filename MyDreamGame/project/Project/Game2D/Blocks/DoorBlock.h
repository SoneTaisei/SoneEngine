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

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float startWidth_ = 1.0f;
    float startHeight_ = 1.0f;

    int linkId_ = 1;
    float openProgress_ = 0.0f; // 0.0f (閉) ～ 1.0f (開)
    float openSpeed_ = 2.0f;    // 開閉速度
};