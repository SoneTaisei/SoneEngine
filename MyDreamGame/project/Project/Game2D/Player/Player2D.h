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
    bool IsOnGround() const { return state_.isOnGround_; }
    void SetIsHoldingChain(bool holding) { state_.isHoldingChain_ = holding; }
    bool IsHoldingChain() const { return state_.isHoldingChain_; }
    void SetIsSwingingChain(bool swinging) { state_.isSwingingChain_ = swinging; }
    bool IsSwingingChain() const { return state_.isSwingingChain_; }
    void SetChainSwingOmega(float omega) { state_.chainSwingOmega_ = omega; }
    float GetChainSwingOmega() const { return state_.chainSwingOmega_; }
    void SetChainSwingTheta(float theta) { state_.chainSwingTheta_ = theta; }
    float GetChainSwingTheta() const { return state_.chainSwingTheta_; }

    // --- 鎖アクション用フック（物理は触らず、ジャンプ処理と同じ2行で上向き速度を与える） ---
    void LaunchVertical(float velocityY) {
        state_.velocity_.y = velocityY;
        state_.isOnGround_ = false;
    }
    // 任意方向へ発射する。横方向は launchVelocityX_ として着地・壁接触まで残る（通常移動の入力に加算される）
    void Launch(const Vector3& velocity) {
        state_.velocity_ = { velocity.x, velocity.y, 0.0f };
        state_.launchVelocityX_ = velocity.x;
        state_.isOnGround_ = false;
    }
    // 振り子で飛んだ際の一回転（宙返り）を開始
    void TriggerSpinFlip(float omega) {
        state_.spinFlipDuration_ = 0.45f;
        state_.spinFlipTimer_ = state_.spinFlipDuration_;
        // 飛ぶ向き（横速度）に合わせて回転方向を決定（右へ飛ぶなら時計回り -1.0f、左へ飛ぶなら反時計回り +1.0f）
        if (std::abs(state_.velocity_.x) > 0.5f) {
            state_.spinFlipSign_ = (state_.velocity_.x > 0.0f) ? -1.0f : 1.0f;
        } else {
            state_.spinFlipSign_ = (omega >= 0.0f) ? 1.0f : -1.0f;
        }
    }
    // 鎖アクション中（スピンなど）の移動減速・ジャンプ無効。次フレームの入力から反映（1.0f/false で解除）
    void SetActionInputModifier(float moveFactor, bool jumpLocked) {
        actionMoveFactor_ = moveFactor;
        actionJumpLocked_ = jumpLocked;
    }

    const Vector3& GetPosition() const { return state_.position_; }
    void SetPosition(const Vector3& pos) { state_.position_ = pos; }
    const Vector3& GetStartPosition() const { return state_.startPosition_; }
    void SetStartPosition(const Vector3& pos) { state_.startPosition_ = pos; }

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
            state_.launchVelocityX_ = 0.0f;
        }
    }

    void ReachGoal() {
        if (!state_.isGoal_) {
            state_.isGoal_ = true;
            state_.goalTimer_ = 0.0f;
            state_.velocity_ = { 0.0f, 0.0f, 0.0f };
            state_.launchVelocityX_ = 0.0f;
        }
    }

    void SpawnSmokeBomb(const Vector3& pos) { visuals_.SpawnSmokeBomb(pos); }
    void SetClearEscaped(bool escaped) { state_.isClearEscaped_ = escaped; }
    bool IsClearEscaped() const { return state_.isClearEscaped_; }
    void UpdateVisualsOnly(float deltaTime) { visuals_.Update(state_, params_, deltaTime); }
    void UpdateClearAnimation(float clearTimer, float deltaTime) { visuals_.UpdateClearAnimation(state_, params_, clearTimer, deltaTime); }

    bool IsGoalComplete() const { return state_.isGoal_ && state_.goalTimer_ >= params_.goalWaitTime_; }

    void AddChainLength(int amount) { state_.chainLength_ += amount; }
    void SetChainLength(int length) { state_.chainLength_ = length; }
    int GetChainLength() const { return state_.chainLength_; }

    void ResetState(const Vector3& initPos);
    void ClearEffects() { 
        state_.isClearEscaped_ = false;
        visuals_.ClearEffects(); 
    }

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
    float actionMoveFactor_ = 1.0f;  // 鎖アクションによる移動倍率
    bool actionJumpLocked_ = false;  // 鎖アクション中のジャンプ無効
    PlayerPhysics physics_;
    GameCamera* camera_ = nullptr;
};
