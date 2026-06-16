#pragma once
#include "BaseBlock.h"

class OneWayBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsOneWay() const override { return true; }
};
