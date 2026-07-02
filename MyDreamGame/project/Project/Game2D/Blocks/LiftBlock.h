#pragma once
#include "BaseBlock.h"
#include "Core/TimeManager.h"
#include <algorithm>

class LiftBlock : public BaseBlock {
public:
    LiftBlock(MapChip2D* map, int chipX, int chipY) : BaseBlock(map, chipX, chipY) {}

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;

    bool IsSolid() const override { return true; }
    void OnPlayerStand() override { isPlayerStandingThisFrame_ = true; }

    void SetProperties(const nlohmann::json& properties) override;

    Vector3 GetVelocity() const override { return velocity_; }
    bool IsMoving() const override { return state_ == LiftState::MovingForward || state_ == LiftState::MovingBackward; }

private:
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 direction_ = { 0.0f, 0.0f, 0.0f };
    float speedForward_ = 6.0f;
    float speedBackward_ = 3.0f;

    bool useProperties_ = false;
    std::string propDirection_ = "horizontal";
    float propRange_ = 10.0f;
    float propSpeed_ = 2.0f;
    
    float minRailWorldX_ = 0.0f;
    float maxRailWorldX_ = 0.0f;
    float minRailWorldY_ = 0.0f;
    float maxRailWorldY_ = 0.0f;

    Vector3 startPos_;
    Vector3 endPos_;

    bool isPlayerStandingThisFrame_ = false;
    float currentT_ = 0.0f; // 端から端までの進行度 (0.0=初期位置, 1.0=終点)
    
    enum class LiftState {
        IdleAtStart,
        MovingForward,
        WaitingAtEnd,
        MovingBackward
    };
    LiftState state_ = LiftState::IdleAtStart;
    float waitTimer_ = 0.0f;
};
