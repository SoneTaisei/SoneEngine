#pragma once
#include "BaseBlock.h"

class CoinBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float size) override;
    void Update() override;
    void OnCollision(Player2D* player) override;
private:
    float rotationY_ = 0.0f;
};
