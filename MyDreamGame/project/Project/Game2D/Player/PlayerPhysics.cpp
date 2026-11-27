#include "PlayerPhysics.h"

void PlayerPhysics::Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player) {
    if (state_.isDead_ || state_.isGoal_) return;

    if (state_.hitstopTimer_ > 0.0f) {
        state_.hitstopTimer_ -= deltaTime;
        return; // ヒットストップ中は物理演算・入力をスキップ
    }

    if (state_.isGrabbingVine_) {
        // ツタに掴まっている間はツタ側で位置と速度を直接制御するため、
        // 通常の重力・移動・壁衝突処理をスキップ
        return;
    }

    // 1. 入力・タイマー・アクション更新
    movement_.UpdateTimers(state_, params_, input_, deltaTime);
    if (state_.isDashing_) {
        jump_.HandleDashCancelJump(state_, params_, visuals_);
    }
    if (!state_.isDashing_) {
        movement_.UpdateHorizontal(state_, params_, input_, deltaTime);
        jump_.HandleJump(state_, params_, input_, visuals_);
    }
    dash_.TryStartDash(state_, params_, input_, visuals_, player);

    // 2. 重力・壁登り・ダッシュ継続・足場速度更新
    movement_.ApplyGravityAndClimb(state_, params_, input_, deltaTime);
    dash_.UpdateDashTimer(state_, params_, deltaTime);
    movement_.UpdatePlatformVelocity(state_);

    // 3. Y軸（上下）の移動と当たり判定
    float prevPosY = state_.position_.y;
    float moveDeltaY = 0.0f;
    if (state_.isOnMovingPlatform_ && state_.isOnGround_) {
        moveDeltaY += state_.platformVelocity_.y * deltaTime;
    }
    if (state_.isWallClinging_ || state_.isWallSliding_) {
        moveDeltaY += state_.wallPlatformVelocity_.y * deltaTime;
    }
    moveDeltaY += state_.velocity_.y * deltaTime;
    state_.position_.y += moveDeltaY;
    collisionSolver_.ResolveCollisionY(state_, params_, prevPosY, moveDeltaY);

    // 4. X軸（左右）の移動と当たり判定
    float prevPosX = state_.position_.x;
    float moveDeltaX = 0.0f;
    if (state_.isOnMovingPlatform_ && state_.isOnGround_) {
        moveDeltaX += state_.platformVelocity_.x * deltaTime;
    }
    if (state_.isWallClinging_ || state_.isWallSliding_) {
        moveDeltaX += state_.wallPlatformVelocity_.x * deltaTime;
    }
    moveDeltaX += state_.velocity_.x * deltaTime;
    state_.position_.x += moveDeltaX;
    collisionSolver_.ResolveCollisionX(state_, params_, prevPosX, moveDeltaX, deltaTime);

    // 5. 非Solidブロック（コインなど）や全ブロックのOnCollision処理
    collisionSolver_.SimulateCollisions(state_, params_, player);
}

void PlayerPhysics::SimulateCollisions(PlayerState& state_, const PlayerParams& params_, Player2D* player) {
    collisionSolver_.SimulateCollisions(state_, params_, player);
}

AABB2D PlayerPhysics::GetAABB(const PlayerState& state_, const PlayerParams& params_) const {
    return collisionSolver_.GetAABB(state_, params_);
}

AABB2D PlayerPhysics::GetBodyAABB(const PlayerState& state_, const PlayerParams& params_) const {
    return collisionSolver_.GetBodyAABB(state_, params_);
}

AABB2D PlayerPhysics::GetHeadAABB(const PlayerState& state_, const PlayerParams& params_) const {
    return collisionSolver_.GetHeadAABB(state_, params_);
}




