#pragma once
#include "BaseBlock.h"
#include "Core/TimeManager.h"
#include <algorithm>

class LiftBlock : public BaseBlock {
public:
    LiftBlock(MapChip2D* map, int chipX, int chipY) : BaseBlock(map, chipX, chipY) {}

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;

    Vector3 GetVelocity() const { return velocity_; }
    bool IsMoving() const { return direction_.x != 0.0f || direction_.y != 0.0f; }

private:
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 direction_ = { 0.0f, 0.0f, 0.0f };
    float speed_ = 3.0f;
    
    float minRailWorldX_ = 0.0f;
    float maxRailWorldX_ = 0.0f;
    float minRailWorldY_ = 0.0f;
    float maxRailWorldY_ = 0.0f;
};
