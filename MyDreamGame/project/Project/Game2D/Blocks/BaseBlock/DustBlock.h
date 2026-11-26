#pragma once
#include "Game2D/Blocks/BaseBlock.h"

// Celeste-style creeping dust/miasma hazard block
class DustBlock : public BaseMapObject {
public:
    using BaseMapObject::BaseMapObject;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsSolid() const override { return false; } // Can be passed through, kills on contact
    bool IsHazard() const override { return true; }
    void OnCollision(Player2D* player) override;
};
