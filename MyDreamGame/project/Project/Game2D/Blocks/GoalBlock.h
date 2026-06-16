#pragma once
#include "BaseBlock.h"

class GoalBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float size) override;
    void OnCollision(Player2D* player) override;
};
