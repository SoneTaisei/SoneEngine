#pragma once
#include "BaseBlock.h"

class ChainItemBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    
    // プレイヤーとぶつかったときの処理
    void OnCollision(Player2D* player) override;
    
    // 壁ではないのですり抜けられるようにする
    bool IsSolid() const override { return false; }
};
