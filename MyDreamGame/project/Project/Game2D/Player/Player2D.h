#pragma once
#include "PlayerConfig.h"
#include "PlayerPhysics.h"
#include "PlayerInput.h"
#include "PlayerState.h"
#include "PlayerVisuals.h"
#include "Component/IComponent.h"
#include <memory>
#include <string>

class MapChip2D;
class GameCamera;

class Player2D : public IComponent {
public:
    Player2D() = default;
    ~Player2D() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DisplayImGui() override;

    void UpdateWithMap(MapChip2D& map, bool isTransitioning = false);

    // 速度・位置の設定と取得
    void SetVelocity(const Vector3& velocity) { state_.velocity_ = velocity; }
    Vector3 GetVelocity() const { return state_.velocity_; }
    void SetIsOnGround(bool state) { state_.isOnGround_ = state; }

    const Vector3& GetPosition() const { return state_.position_; }
    void SetPosition(const Vector3& pos) { state_.position_ = pos; }
    const Vector3& GetStartPosition() const { return state_.startPosition_; }

    void SetCamera(GameCamera* camera) { camera_ = camera; }
    GameCamera* GetCamera() const { return camera_; }

    void FindSpawnPoint(const MapChip2D& map);

    PrimitiveObject* GetPrimitiveObject() { return visuals_.GetPrimitiveObject(); }
    Object3D* GetModelObject() { return visuals_.GetModelObject(); }
    AnimatorComponent* GetAnimator() { return visuals_.GetAnimator(); }
    PlayerVisuals& GetVisuals() { return visuals_; }

    void Kill(bool isFallDeath = false) {
        (void)isFallDeath;
        if (!state_.isDead_) {
            state_.isDead_ = true;
            state_.isRespawning_ = false;
            state_.deathTimer_ = 0.0f;
            state_.velocity_ = { 0.0f, 0.0f, 0.0f };
        }
    }

    void ReachGoal() {
        if (!state_.isGoal_) {
            state_.isGoal_ = true;
            state_.goalTimer_ = 0.0f;
            state_.velocity_ = { 0.0f, 0.0f, 0.0f };
            visuals_.SpawnConfetti(state_.position_);
        }
    }

    bool IsGoalComplete() const { return state_.isGoal_ && state_.goalTimer_ >= params_.goalWaitTime_; }

    void AddChainLength(int amount) { state_.chainLength_ += amount; }
    void SetChainLength(int length) { state_.chainLength_ = length; }
    int GetChainLength() const { return state_.chainLength_; }

    void ResetState(const Vector3& initPos);
    void ClearEffects() { visuals_.ClearEffects(); }

    AABB2D GetAABB() const { return physics_.GetAABB(state_, params_); }
    bool IsDead() const { return state_.isDead_; }
    bool IsGoal() const { return state_.isGoal_; }
    const PlayerParams& GetParams() const { return params_; }

private:
    PlayerParams params_;
    PlayerState state_;
    PlayerVisuals visuals_;
    PlayerInput input_;
    InputState currentInput_;
    PlayerPhysics physics_;
    GameCamera* camera_ = nullptr;
};
