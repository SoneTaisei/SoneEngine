#pragma once
#include "BaseBlock.h"

class DeathBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsSolid() const override { return true; }
    void OnCollision(Player2D* player) override;
};
