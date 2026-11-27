#include "PlayerMovement.h"
#include "Game2D/Blocks/BaseBlock.h"
#include "Game2D/Blocks/BaseBlock/BaseMovingBlock.h"
#include "Component/ColliderComponent.h"
#include <algorithm>
#include <cmath>

void PlayerMovement::UpdateTimers(PlayerState& state, const PlayerParams& params, const InputState& input, float deltaTime) {
    if (state.minJumpTimer_ > 0.0f) {
        state.minJumpTimer_ = (std::max)(0.0f, state.minJumpTimer_ - deltaTime);
    }
    if (state.wallClingReleaseTimer_ > 0.0f) {
        state.wallClingReleaseTimer_ -= deltaTime;
    }
    if (state.springControlDisableTimer_ > 0.0f) {
        state.springControlDisableTimer_ -= deltaTime;
    }
    if (state.climbingUpTimer_ > 0.0f) {
        state.climbingUpTimer_ -= deltaTime;
    }
    if (state.climbLandingTimer_ > 0.0f) {
        state.climbLandingTimer_ -= deltaTime;
    }
    if (state.dashJumpWindowTimer_ > 0.0f) {
        state.dashJumpWindowTimer_ -= deltaTime;
        if (state.dashJumpWindowTimer_ <= 0.0f) {
            state.lastDashDirX_ = 0.0f;
        }
    }

    // ジャンプ先行入力バッファ（約0.12秒保持）
    if (input.isJumpPressed) {
        state.jumpBufferTimer_ = 0.12f;
    } else if (state.jumpBufferTimer_ > 0.0f) {
        state.jumpBufferTimer_ -= deltaTime;
    }

    // 接地コヨーテタイム（地面を離れても約0.1秒間は地上ジャンプを許可）
    if (state.isOnGround_) {
        state.coyoteTimer_ = 0.1f;
        state.stamina_ = params.maxStamina_;
        state.isExhausted_ = false;
    } else if (state.coyoteTimer_ > 0.0f) {
        state.coyoteTimer_ -= deltaTime;
    }

    state.wasWallPose_ = (state.isWallSliding_ || state.isWallClinging_);
    state.isWallSliding_ = false;
    state.isWallClinging_ = false;

    // リフトの慣性猶予（コヨーテタイム）の更新
    if (state.isOnMovingPlatform_) {
        if (std::abs(state.platformVelocity_.x) > 0.01f || std::abs(state.platformVelocity_.y) > 0.01f) {
            state.recentPlatformVelocity_ = state.platformVelocity_;
            float inertiaTime = 0.5f;
            if (state.standingPlatformCollider_ && state.standingPlatformCollider_->GetUserData()) {
                auto* base = static_cast<BaseMapObject*>(state.standingPlatformCollider_->GetUserData());
                if (auto* moving = dynamic_cast<BaseMovingBlock*>(base)) {
                    inertiaTime = moving->GetInertiaTime();
                }
            }
            state.lastPlatformInertiaTime_ = inertiaTime;
            state.platformInertiaTimer_ = 0.15f;
        }
    }
    if (state.platformInertiaTimer_ > 0.0f) {
        state.platformInertiaTimer_ -= deltaTime;
    }

    if (state.wallJumpDirLockTimer_ > 0.0f) {
        state.wallJumpDirLockTimer_ -= deltaTime;
        if (state.wallJumpDirLockTimer_ <= 0.0f) {
            state.lockedDirectionX_ = 0.0f;
        }
    }
}

void PlayerMovement::UpdateHorizontal(PlayerState& state, const PlayerParams& params, const InputState& input, float deltaTime) {
    bool inputLeft = input.moveX < 0.0f;
    bool inputRight = input.moveX > 0.0f;

    if (inputRight) {
        state.facingDirectionX_ = 1.0f;
    } else if (inputLeft) {
        state.facingDirectionX_ = -1.0f;
    }

    // 壁掴み中は方向キーを離しても壁への仮想入力を維持する
    if (!state.isOnGround_ && input.isClingHeld && !state.isExhausted_) {
        if (state.isTouchingWallRight_) {
            inputRight = true;
            inputLeft = false;
        }
        if (state.isTouchingWallLeft_) {
            inputLeft = true;
            inputRight = false;
        }
    }

    bool allowLeft = true;
    bool allowRight = true;
    if (state.springControlDisableTimer_ > 0.0f || state.climbLandingTimer_ > 0.0f) {
        allowLeft = false;
        allowRight = false;
    }
    if (state.wallJumpDirLockTimer_ > 0.0f) {
        if (state.lockedDirectionX_ == -1.0f) allowLeft = false;
        if (state.lockedDirectionX_ == 1.0f) allowRight = false;
    }

    float targetVelX = 0.0f;
    if (inputLeft && allowLeft) {
        targetVelX = -params.moveSpeed_;
    }
    if (inputRight && allowRight) {
        targetVelX = params.moveSpeed_;
    }

    // 外部速度(慣性)の加算と減衰
    if (state.isOnGround_) {
        state.externalVelocityX_ = 0.0f;
        state.externalVelocityTimer_ = 0.0f;
    } else if (state.externalVelocityTimer_ > 0.0f && state.externalVelocityDuration_ > 0.0f) {
        state.externalVelocityTimer_ -= deltaTime;
        if (state.externalVelocityTimer_ <= 0.0f) {
            state.externalVelocityTimer_ = 0.0f;
            state.externalVelocityX_ = 0.0f;
        } else {
            float ratio = state.externalVelocityTimer_ / state.externalVelocityDuration_;
            state.externalVelocityX_ = state.initialExternalVelocityX_ * ratio;
        }
    } else {
        state.externalVelocityX_ = 0.0f;
        state.externalVelocityTimer_ = 0.0f;
    }

    if (state.climbLandingTimer_ > 0.0f) {
        state.velocity_.x = 0.0f;
        state.externalVelocityX_ = 0.0f;
        state.externalVelocityTimer_ = 0.0f;
        state.hasDashInertia_ = false;
    } else if (state.wallJumpTimer_ <= 0.0f) {
        if (state.hasDashInertia_) {
            float currentVelX = state.velocity_.x;
            float decayRate = state.isOnGround_ ? params.dashInertiaDecayGround_ : params.dashInertiaDecayAir_;

            if ((targetVelX > 0.0f && currentVelX < 0.0f) || (targetVelX < 0.0f && currentVelX > 0.0f)) {
                decayRate *= 2.0f;
            }

            if (currentVelX > targetVelX) {
                currentVelX -= decayRate * deltaTime;
                if (currentVelX <= targetVelX) {
                    currentVelX = targetVelX;
                    state.hasDashInertia_ = false;
                }
            } else if (currentVelX < targetVelX) {
                currentVelX += decayRate * deltaTime;
                if (currentVelX >= targetVelX) {
                    currentVelX = targetVelX;
                    state.hasDashInertia_ = false;
                }
            } else {
                state.hasDashInertia_ = false;
            }
            state.velocity_.x = currentVelX;
        } else {
            float desiredVelX = targetVelX + state.externalVelocityX_;
            float currentVelX = state.velocity_.x;

            bool hasMoveInput = std::abs(targetVelX) > 0.001f;
            float accelRate = state.isOnGround_ ? params.groundAcceleration_ : params.airAcceleration_;
            float decelRate = state.isOnGround_ ? params.groundDeceleration_ : params.airDeceleration_;

            bool isDecelerating = !hasMoveInput ||
                (std::abs(currentVelX) > std::abs(desiredVelX) && (currentVelX * desiredVelX > 0.0f));
            float rate = (std::max)(0.0f, isDecelerating ? decelRate : accelRate);

            float diff = desiredVelX - currentVelX;
            float maxDelta = rate * deltaTime;
            if (std::abs(diff) <= maxDelta) {
                state.velocity_.x = desiredVelX;
            } else {
                state.velocity_.x = currentVelX + std::copysign(maxDelta, diff);
            }
        }
    } else {
        // 壁キック直後の操作不能時間中の速度補間
        float t = 1.0f - (state.wallJumpTimer_ / params.wallJumpDuration_);
        float startVelX = ((state.lockedDirectionX_ == 1.0f) ? -params.wallJumpPower_.x : params.wallJumpPower_.x) + state.externalVelocityX_;
        float endVelX = targetVelX + state.externalVelocityX_;
        state.velocity_.x = startVelX + (endVelX - startVelX) * t;
    }

    // 壁ずり落ち / 壁張り付きの判定
    if (!state.isOnGround_ && state.wallClingReleaseTimer_ <= 0.0f) {
        bool tryingToCling = (input.isClingHeld && !state.isExhausted_);
        bool touchingWall = ((state.isTouchingWallRight_ && inputRight) || (state.isTouchingWallLeft_ && inputLeft));

        if (touchingWall && (state.velocity_.y <= 0.0f || tryingToCling)) {
            state.isWallSliding_ = true;
            state.externalVelocityX_ = 0.0f;
            state.externalVelocityTimer_ = 0.0f;
            if (tryingToCling) {
                state.isWallClinging_ = true;
                state.isJumping_ = false;
                state.minJumpTimer_ = 0.0f;
            }
        }
    }
}

void PlayerMovement::ApplyGravityAndClimb(PlayerState& state, const PlayerParams& params, const InputState& input, float deltaTime) {
    if (state.isDashing_) {
        return;
    }

    if (state.isWallClinging_ && !state.isExhausted_) {
        float climbDirY = 0.0f;
        if (input.moveY > 0.1f) {
            climbDirY = 1.0f;
        } else if (input.moveY < -0.1f) {
            climbDirY = -1.0f;
        }

        if (climbDirY > 0.0f) {
            state.stamina_ -= params.staminaConsumeClimb_ * deltaTime;
            state.climbingUpTimer_ = 0.25f;
        } else {
            state.stamina_ -= params.staminaConsumeCling_ * deltaTime;
        }

        if (state.stamina_ <= 0.0f) {
            state.stamina_ = 0.0f;
            state.isExhausted_ = true;
            state.isWallClinging_ = false;
            state.climbingUpTimer_ = 0.0f;
        }

        if (state.isWallClinging_) {
            state.velocity_.y = climbDirY * params.wallClimbSpeed_;
            return;
        }
    }

    state.velocity_.y += params.gravity_ * deltaTime;

    float currentMaxFallSpeed = params.maxFallSpeed_;
    if (state.isWallSliding_ && state.velocity_.y < 0.0f) {
        currentMaxFallSpeed = state.isExhausted_ ? (params.wallSlideSpeed_ * 3.0f) : params.wallSlideSpeed_;
    }

    if (state.velocity_.y < currentMaxFallSpeed) {
        state.velocity_.y = currentMaxFallSpeed;
    }
}

void PlayerMovement::UpdatePlatformVelocity(PlayerState& state) {
    if (state.isOnMovingPlatform_ && state.standingPlatformCollider_) {
        state.platformVelocity_ = state.standingPlatformCollider_->GetVelocity();
    }
    if (state.isWallClinging_ || state.isWallSliding_) {
        if (state.wallPlatformCollider_) {
            state.wallPlatformVelocity_ = state.wallPlatformCollider_->GetVelocity();
        }

        if (std::abs(state.wallPlatformVelocity_.x) > 0.01f || std::abs(state.wallPlatformVelocity_.y) > 0.01f) {
            state.recentPlatformVelocity_ = state.wallPlatformVelocity_;
            float inertiaTime = 0.5f;
            if (state.wallPlatformCollider_ && state.wallPlatformCollider_->GetUserData()) {
                auto* base = static_cast<BaseMapObject*>(state.wallPlatformCollider_->GetUserData());
                if (auto* moving = dynamic_cast<BaseMovingBlock*>(base)) {
                    inertiaTime = moving->GetInertiaTime();
                }
            }
            state.lastPlatformInertiaTime_ = inertiaTime;
            state.platformInertiaTimer_ = 0.15f;
        }
    }
}
