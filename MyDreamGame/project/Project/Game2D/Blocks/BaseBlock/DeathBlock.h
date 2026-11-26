#pragma once
#include "Game2D/Blocks/BaseBlock.h"

class DeathBlock : public BaseMapObject {
public:
    using BaseMapObject::BaseMapObject;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    bool IsSolid() const override { return true; }
    bool IsHazard() const override { return true; }
    void OnCollision(Player2D* player) override;
};
