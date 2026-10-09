#pragma once
#include "Game2D/Blocks/BaseBlock.h"

class RailBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsSolid() const override { return false; }
};
