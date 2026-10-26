#pragma once
#include "Game2D/Blocks/BaseBlock.h"

class CoinBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void OnCollision(Player2D* player) override;
    void Reset() override;
private:
    float rotationY_ = 0.0f;
    bool isCollected_ = false;
    float collectTimer_ = 0.0f;
    float collectDuration_ = 0.3f;
    Vector3 position_ = {0.0f, 0.0f, 0.0f};
    Vector3 initialPosition_ = {0.0f, 0.0f, 0.0f};
};
