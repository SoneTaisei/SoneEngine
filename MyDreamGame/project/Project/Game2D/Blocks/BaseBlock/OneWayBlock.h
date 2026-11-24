#pragma once
#include "Game2D/Blocks/BaseBlock.h"

class OneWayBlock : public BaseMapObject {
public:
    using BaseMapObject::BaseMapObject;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsOneWay() const override { return true; }
};
