#pragma once
#include "BaseBlock.h"

class PatrolEnemyBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    
    void Update() override;
    
    bool IsSolid() const override { return false; }
    void OnCollision(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;

    void Reset() override;

private:
    Vector3 currentPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 startPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
    float moveSpeed_ = 2.5f;
    float moveDir_ = 1.0f; // 1.0f: 右, -1.0f: 左
    float gravity_ = -20.0f;
    float rollAngle_ = 0.0f;
    float radius_ = 0.5f;
    bool isOnGround_ = false;
};
