#pragma once
#include "BaseBlock.h"

class NormalBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float size) override;
    bool IsSolid() const override { return true; }
};
