#pragma once
#include "Game2D/Blocks/BaseBlock.h"

// Celeste-style creeping dust/miasma hazard block
class DustBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsSolid() const override { return false; } // Can be passed through, kills on contact
    void OnCollision(Player2D* player) override;
};
