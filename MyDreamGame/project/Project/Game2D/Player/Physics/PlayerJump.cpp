#include "PlayerJump.h"
#include <algorithm>
#include <cmath>

void PlayerJump::BeginJump(PlayerState& state, const PlayerParams& params) {
    state.isJumping_ = true;
    state.minJumpTimer_ = (std::max)(0.0f, params.minJumpDuration_);
}

void PlayerJump::ApplyPlatformInertia(PlayerState& state) {
    if (state.platformInertiaTimer_ <= 0.0f) {
        return;
    }
    state.externalVelocityDuration_ = (state.lastPlatformInertiaTime_ > 0.0f) ? state.lastPlatformInertiaTime_ : 0.5f;
    state.externalVelocityTimer_ = state.externalVelocityDuration_;
    state.initialExternalVelocityX_ = state.recentPlatformVelocity_.x;
    state.externalVelocityX_ = state.initialExternalVelocityX_;
    state.velocity_.x += state.externalVelocityX_;
    // 上方向（ジャンプ補助）の慣性のみ加算し、下方向へはジャンプ力を殺さないようにする
    if (state.recentPlatformVelocity_.y > 0.0f) {
        state.velocity_.y += state.recentPlatformVelocity_.y;
    }
    state.platformInertiaTimer_ = 0.0f;
}

void PlayerJump::ExecuteWallJump(PlayerState& state, const PlayerParams& params, PlayerVisuals& visuals, float wallSide, bool wantWallKick, bool cancelDash) {
    state.jumpBufferTimer_ = 0.0f;

    if (cancelDash) {
        state.isDashing_ = false;
        state.hasDashInertia_ = false;
    }

    if (!wantWallKick) {
        // 壁張り付き中は真上ジャンプ
        state.velocity_.x = 0.0f;
        state.velocity_.y = params.jumpPower_;

        state.stamina_ -= params.staminaConsumeJump_;
        if (state.stamina_ <= 0.0f) {
            state.stamina_ = 0.0f;
            state.isExhausted_ = true;
        }
        state.wallClingReleaseTimer_ = 0.15f;
    } else {
        // 壁キック（壁と逆方向へ跳ね返る）
        state.facingDirectionX_ = -wallSide;
        state.velocity_.x = -wallSide * params.wallJumpPower_.x;
        state.velocity_.y = params.wallJumpPower_.y;
        state.wallJumpTimer_ = params.wallJumpDuration_;
        state.externalVelocityX_ = 0.0f;
        state.externalVelocityTimer_ = 0.0f;
        visuals.SpawnJumpDust({ state.position_.x + wallSide * params.halfWidth_, state.position_.y, 0.0f }, -wallSide);

        state.wallJumpDirLockTimer_ = params.wallJumpDirLockDuration_;
        state.lockedDirectionX_ = wallSide;
        state.wallClingReleaseTimer_ = 0.15f;
    }

    if (!cancelDash) {
        ApplyPlatformInertia(state);
        state.climbingUpTimer_ = 0.0f;
    }

    if (wallSide > 0.0f) {
        state.isTouchingWallRight_ = false;
    } else {
        state.isTouchingWallLeft_ = false;
    }
    state.isWallSliding_ = false;
    state.isWallClinging_ = false;
    BeginJump(state, params);
}

void PlayerJump::HandleDashCancelJump(PlayerState& state, const PlayerParams& params, PlayerVisuals& visuals) {
    if (state.jumpBufferTimer_ <= 0.0f || state.climbLandingTimer_ > 0.0f) {
        return;
    }

    if (state.isOnGround_ || state.coyoteTimer_ > 0.0f) {
        state.jumpBufferTimer_ = 0.0f;
        // 地上ダッシュジャンプ (スーパーダッシュジャンプ)
        state.isDashing_ = false;
        state.dashJumpWindowTimer_ = 0.0f;
        state.coyoteTimer_ = 0.0f;
        state.velocity_.y = params.jumpPower_ * params.dashJumpPowerMult_;

        float jumpDirX = state.lastDashDirX_;
        if (jumpDirX == 0.0f) {
            jumpDirX = (state.dashVelocity_.x > 0.01f) ? 1.0f : ((state.dashVelocity_.x < -0.01f) ? -1.0f : 0.0f);
        }
        if (jumpDirX != 0.0f) {
            float superJumpSpeed = params.dashSpeed_ * params.dashJumpSpeedMult_ + params.dashJumpBonusSpeed_;
            state.velocity_.x = jumpDirX * superJumpSpeed;
            state.hasDashInertia_ = true;
        } else {
            state.hasDashInertia_ = false;
        }
        ApplyPlatformInertia(state);
        if (params.dashJumpRefreshDash_) {
            state.canDash_ = true;
        }
        state.isOnGround_ = false;
        state.isOnMovingPlatform_ = false;
        BeginJump(state, params);
        visuals.SpawnJumpDust({ state.position_.x, state.position_.y - params.halfHeight_, 0.0f }, 0.0f);
    } else if (state.isTouchingWallRight_) {
        ExecuteWallJump(state, params, visuals, 1.0f, true, true);
    } else if (state.isTouchingWallLeft_) {
        ExecuteWallJump(state, params, visuals, -1.0f, true, true);
    }
}

void PlayerJump::HandleJump(PlayerState& state, const PlayerParams& params, const InputState& input, PlayerVisuals& visuals) {
    if (state.jumpBufferTimer_ > 0.0f && state.climbLandingTimer_ <= 0.0f) {
        if (state.isOnGround_ || state.coyoteTimer_ > 0.0f) {
            state.jumpBufferTimer_ = 0.0f;
            state.coyoteTimer_ = 0.0f;

            bool canSuperJump = (state.dashJumpWindowTimer_ > 0.0f) && (std::abs(state.lastDashDirX_) > 0.1f);
            if (canSuperJump && ((state.lastDashDirX_ > 0.0f && input.moveX < -0.1f) || (state.lastDashDirX_ < 0.0f && input.moveX > 0.1f))) {
                canSuperJump = false;
            }

            if (canSuperJump) {
                state.velocity_.y = params.jumpPower_ * params.dashJumpPowerMult_;
                float superJumpSpeed = params.dashSpeed_ * params.dashJumpSpeedMult_ + params.dashJumpBonusSpeed_;
                state.velocity_.x = state.lastDashDirX_ * superJumpSpeed;
                state.hasDashInertia_ = true;
                state.dashJumpWindowTimer_ = 0.0f;
                if (params.dashJumpRefreshDash_) {
                    state.canDash_ = true;
                }
            } else {
                state.velocity_.y = params.jumpPower_;
            }

            ApplyPlatformInertia(state);
            state.isOnGround_ = false;
            state.isOnMovingPlatform_ = false;
            BeginJump(state, params);
            visuals.SpawnJumpDust({ state.position_.x, state.position_.y - params.halfHeight_, 0.0f }, 0.0f);
        } else if (state.isTouchingWallRight_ || state.isTouchingWallLeft_) {
            float wallSide = state.isTouchingWallRight_ ? 1.0f : -1.0f;
            bool isPressingCling = input.isClingHeld && !state.isExhausted_;
            bool wantWallKick = !isPressingCling || (input.moveX * wallSide < 0.0f);
            ExecuteWallJump(state, params, visuals, wallSide, wantWallKick, false);
        }
    }

    // 最低ジャンプ時間経過後、ジャンプキーが離されていれば上昇速度をカットする
    if (!state.isOnGround_ && !state.isWallClinging_ && state.isJumping_) {
        if (state.velocity_.y <= 0.0f) {
            state.isJumping_ = false;
            state.minJumpTimer_ = 0.0f;
        } else if (state.minJumpTimer_ <= 0.0f && !input.isJumpHeld) {
            float cutMult = std::clamp(params.jumpCutMultiplier_, 0.0f, 1.0f);
            state.velocity_.y *= cutMult;
            state.isJumping_ = false;
        }
    }
}
