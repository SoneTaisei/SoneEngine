#include "PlayerPhysics.h"
#include "Player2D.h"
#include "Core/TimeManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/Camera/GameCamera.h"
#include "../Blocks/BaseBlock.h"
#include <algorithm>
#include <cmath>

#include "Collision/CollisionManager.h"

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

    HandleInputLogic(state_, params_, input_, visuals_, deltaTime, player);
    ApplyGravity(state_, params_, input_, deltaTime);

    // ダッシュタイマーの更新
    if (state_.isDashing_) {
        state_.dashTimer_ += deltaTime;
        if (state_.dashTimer_ >= params_.dashDuration_) {
            state_.isDashing_ = false;
            // ダッシュ終了時、水平速度に慣性保持率を適用
            if (std::abs(state_.dashVelocity_.x) > 0.01f) {
                state_.velocity_.x = state_.dashVelocity_.x * params_.dashEndInertiaRatio_;
                state_.hasDashInertia_ = true;
            } else {
                state_.hasDashInertia_ = false;
            }
            // ダッシュ終了時、上向きの速度が残っている場合は設定した上限値にする。
            if (state_.velocity_.y > params_.dashEndUpwardVelocity_) {
                state_.velocity_.y = params_.dashEndUpwardVelocity_;
            }
        } else {
            // ダッシュ中は固定速度
            state_.velocity_ = state_.dashVelocity_;
        }
    }

    // --- 足場速度の取得 ---
    if (state_.isOnMovingPlatform_ && state_.standingPlatformCollider_) {
        state_.platformVelocity_ = state_.standingPlatformCollider_->GetVelocity();
    }
    if (state_.isWallClinging_ || state_.isWallSliding_) {
        if (state_.wallPlatformCollider_) {
            state_.wallPlatformVelocity_ = state_.wallPlatformCollider_->GetVelocity();
        }
        
        if (std::abs(state_.wallPlatformVelocity_.x) > 0.01f || std::abs(state_.wallPlatformVelocity_.y) > 0.01f) {
            state_.recentPlatformVelocity_ = state_.wallPlatformVelocity_;
            float inertiaTime = 0.5f;
            if (state_.wallPlatformCollider_ && state_.wallPlatformCollider_->GetUserData()) {
                auto* block = static_cast<BaseBlock*>(state_.wallPlatformCollider_->GetUserData());
                inertiaTime = block->GetInertiaTime();
            }
            state_.lastPlatformInertiaTime_ = inertiaTime;
            state_.platformInertiaTimer_ = 0.15f;
        }
    }

    // --- Y軸（上下）の移動と当たり判定 ---
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
    ResolveCollisionY(state_, params_, prevPosY, moveDeltaY);

    // --- X軸（左右）の移動と当たり判定 ---
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
    ResolveCollisionX(state_, params_, prevPosX, moveDeltaX);

    // 非Solidブロック（コインなど）や全ブロックのOnCollision処理
    SimulateCollisions(state_, params_, player);
}

void PlayerPhysics::HandleInputLogic(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player) {
    if (state_.wallClingReleaseTimer_ > 0.0f) {
        state_.wallClingReleaseTimer_ -= deltaTime;
    }
    if (state_.springControlDisableTimer_ > 0.0f) {
        state_.springControlDisableTimer_ -= deltaTime;
    }
    if (state_.climbingUpTimer_ > 0.0f) {
        state_.climbingUpTimer_ -= deltaTime;
    }
    if (state_.climbLandingTimer_ > 0.0f) {
        state_.climbLandingTimer_ -= deltaTime;
    }
    if (state_.dashJumpWindowTimer_ > 0.0f) {
        state_.dashJumpWindowTimer_ -= deltaTime;
        if (state_.dashJumpWindowTimer_ <= 0.0f) {
            state_.lastDashDirX_ = 0.0f;
        }
    }

    // ジャンプ先行入力バッファ（約0.12秒保持）
    if (input_.isJumpPressed) {
        state_.jumpBufferTimer_ = 0.12f;
    } else if (state_.jumpBufferTimer_ > 0.0f) {
        state_.jumpBufferTimer_ -= deltaTime;
    }

    // 接地コヨーテタイム（地面を離れても約0.1秒間は地上ジャンプを許可）
    if (state_.isOnGround_) {
        state_.coyoteTimer_ = 0.1f;
        state_.stamina_ = params_.maxStamina_;
        state_.isExhausted_ = false;
    } else if (state_.coyoteTimer_ > 0.0f) {
        state_.coyoteTimer_ -= deltaTime;
    }

    state_.wasWallPose_ = (state_.isWallSliding_ || state_.isWallClinging_);
    state_.isWallSliding_ = false;
    state_.isWallClinging_ = false;

    // リフトの慣性猶予（コヨーテタイム）の更新
    float dt = TimeManager::GetInstance().GetDeltaTime();
    if (state_.isOnMovingPlatform_) {
        // リフトが動いていれば最新の速度を記録し、猶予時間をリセット
        if (std::abs(state_.platformVelocity_.x) > 0.01f || std::abs(state_.platformVelocity_.y) > 0.01f) {
            state_.recentPlatformVelocity_ = state_.platformVelocity_;
            float inertiaTime = 0.5f;
            if (state_.standingPlatformCollider_ && state_.standingPlatformCollider_->GetUserData()) {
                auto* block = static_cast<BaseBlock*>(state_.standingPlatformCollider_->GetUserData());
                inertiaTime = block->GetInertiaTime();
            }
            state_.lastPlatformInertiaTime_ = inertiaTime;
            state_.platformInertiaTimer_ = 0.15f;
        }
    }
    if (state_.platformInertiaTimer_ > 0.0f) {
        state_.platformInertiaTimer_ -= dt;
    }

    if (state_.wallJumpDirLockTimer_ > 0.0f) {
        state_.wallJumpDirLockTimer_ -= dt;
        if (state_.wallJumpDirLockTimer_ <= 0.0f) {
            state_.lockedDirectionX_ = 0.0f;
        }
    }

    // ダッシュ中のジャンプアクション判定（接地ダッシュジャンプ、壁キックなど。空中ではジャンプ不可）
    if (state_.isDashing_) {
        if (state_.jumpBufferTimer_ > 0.0f && state_.climbLandingTimer_ <= 0.0f) {
            if (state_.isOnGround_ || state_.coyoteTimer_ > 0.0f) {
                state_.jumpBufferTimer_ = 0.0f;
                // 地上ダッシュジャンプ (スーパーダッシュジャンプ)
                state_.isDashing_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.coyoteTimer_ = 0.0f;
                state_.velocity_.y = params_.jumpPower_ * params_.dashJumpPowerMult_;
                
                float jumpDirX = state_.lastDashDirX_;
                if (jumpDirX == 0.0f) {
                    jumpDirX = (state_.dashVelocity_.x > 0.01f) ? 1.0f : ((state_.dashVelocity_.x < -0.01f) ? -1.0f : 0.0f);
                }
                if (jumpDirX != 0.0f) {
                    float superJumpSpeed = params_.dashSpeed_ * params_.dashJumpSpeedMult_ + params_.dashJumpBonusSpeed_;
                    state_.velocity_.x = jumpDirX * superJumpSpeed;
                    state_.hasDashInertia_ = true;
                } else {
                    state_.hasDashInertia_ = false;
                }
                // 足場に乗っている（または猶予期間中）場合は慣性を加算
                if (state_.platformInertiaTimer_ > 0.0f) {
                    state_.externalVelocityDuration_ = (state_.lastPlatformInertiaTime_ > 0.0f) ? state_.lastPlatformInertiaTime_ : 0.5f;
                    state_.externalVelocityTimer_ = state_.externalVelocityDuration_;
                    state_.initialExternalVelocityX_ = state_.recentPlatformVelocity_.x;
                    state_.externalVelocityX_ = state_.initialExternalVelocityX_;
                    state_.velocity_.x += state_.externalVelocityX_;
                    if (state_.recentPlatformVelocity_.y > 0.0f) {
                        state_.velocity_.y += state_.recentPlatformVelocity_.y;
                    }
                    state_.platformInertiaTimer_ = 0.0f;
                }
                if (params_.dashJumpRefreshDash_) {
                    state_.canDash_ = true;
                }
                state_.isOnGround_ = false;
                state_.isOnMovingPlatform_ = false;
                visuals_.SpawnJumpDust({state_.position_.x, state_.position_.y - params_.halfHeight_, 0.0f}, 0.0f);
            } else if (state_.isTouchingWallRight_) {
                state_.jumpBufferTimer_ = 0.0f;
                // 右壁キック
                state_.facingDirectionX_ = -1.0f;
                state_.isDashing_ = false;
                state_.hasDashInertia_ = false;
                state_.velocity_.x = -params_.wallJumpPower_.x;
                state_.velocity_.y = params_.wallJumpPower_.y;
                state_.wallJumpTimer_ = params_.wallJumpDuration_;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                visuals_.SpawnJumpDust({state_.position_.x + params_.halfWidth_, state_.position_.y, 0.0f}, -1.0f);
                state_.wallJumpDirLockTimer_ = params_.wallJumpDirLockDuration_;
                state_.lockedDirectionX_ = 1.0f;
                state_.wallClingReleaseTimer_ = 0.15f;
                state_.isTouchingWallRight_ = false;
                state_.isWallSliding_ = false;
                state_.isWallClinging_ = false;
            } else if (state_.isTouchingWallLeft_) {
                state_.jumpBufferTimer_ = 0.0f;
                // 左壁キック
                state_.facingDirectionX_ = 1.0f;
                state_.isDashing_ = false;
                state_.hasDashInertia_ = false;
                state_.velocity_.x = params_.wallJumpPower_.x;
                state_.velocity_.y = params_.wallJumpPower_.y;
                state_.wallJumpTimer_ = params_.wallJumpDuration_;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                visuals_.SpawnJumpDust({state_.position_.x - params_.halfWidth_, state_.position_.y, 0.0f}, 1.0f);
                state_.wallJumpDirLockTimer_ = params_.wallJumpDirLockDuration_;
                state_.lockedDirectionX_ = -1.0f;
                state_.wallClingReleaseTimer_ = 0.15f;
                state_.isTouchingWallLeft_ = false;
                state_.isWallSliding_ = false;
                state_.isWallClinging_ = false;
            }
        }
    }

    // 通常時の左右移動（ダッシュ中でない場合）
    if (!state_.isDashing_) {
        bool inputLeft = input_.moveX < 0.0f;
        bool inputRight = input_.moveX > 0.0f;

        if (inputRight) {
            state_.facingDirectionX_ = 1.0f;
        } else if (inputLeft) {
            state_.facingDirectionX_ = -1.0f;
        }

        // 壁掴み（Kキー）を押している場合、前フレームで壁に接触していれば、その方向への仮想入力を維持する
        // これにより方向キーを離しても張り付き続ける
        if (!state_.isOnGround_ && input_.isClingHeld && !state_.isExhausted_) {
            if (state_.isTouchingWallRight_) {
                inputRight = true;
                inputLeft = false; // 逆方向への入力を無視して壁から降りないようにする
            }
            if (state_.isTouchingWallLeft_) {
                inputLeft = true;
                inputRight = false; // 逆方向への入力を無視して壁から降りないようにする
            }
        }

        float targetVelX = 0.0f;
        
        bool allowLeft = true;
        bool allowRight = true;
        
        if (state_.springControlDisableTimer_ > 0.0f) {
            allowLeft = false;
            allowRight = false;
        }

        if (state_.climbLandingTimer_ > 0.0f) {
            allowLeft = false;
            allowRight = false;
        }
        
        if (state_.wallJumpDirLockTimer_ > 0.0f) {
            if (state_.lockedDirectionX_ == -1.0f) allowLeft = false;
            if (state_.lockedDirectionX_ == 1.0f) allowRight = false;
        }

        if (inputLeft && allowLeft) {
            targetVelX = -params_.moveSpeed_;
        }
        if (inputRight && allowRight) {
            targetVelX = params_.moveSpeed_;
        }

        // 外部速度(慣性)の加算と減衰
        if (state_.isOnGround_) {
            state_.externalVelocityX_ = 0.0f;
            state_.externalVelocityTimer_ = 0.0f;
        } else if (state_.externalVelocityTimer_ > 0.0f && state_.externalVelocityDuration_ > 0.0f) {
            // 空中ではリフトの慣性持続時間 (inertiaTime) に基づいて滑らかに減衰
            float dt = TimeManager::GetInstance().GetDeltaTime();
            state_.externalVelocityTimer_ -= dt;
            if (state_.externalVelocityTimer_ <= 0.0f) {
                state_.externalVelocityTimer_ = 0.0f;
                state_.externalVelocityX_ = 0.0f;
            } else {
                float ratio = state_.externalVelocityTimer_ / state_.externalVelocityDuration_;
                state_.externalVelocityX_ = state_.initialExternalVelocityX_ * ratio;
            }
        } else {
            state_.externalVelocityX_ = 0.0f;
            state_.externalVelocityTimer_ = 0.0f;
        }

        if (state_.climbLandingTimer_ > 0.0f) {
            state_.velocity_.x = 0.0f;
            state_.externalVelocityX_ = 0.0f;
            state_.externalVelocityTimer_ = 0.0f;
            state_.hasDashInertia_ = false;
        } else if (state_.wallJumpTimer_ <= 0.0f) {
            if (state_.hasDashInertia_) {
                float currentVelX = state_.velocity_.x;
                float decayRate = state_.isOnGround_ ? params_.dashInertiaDecayGround_ : params_.dashInertiaDecayAir_;

                // 逆方向に入力している場合はブレーキを強める
                if ((targetVelX > 0.0f && currentVelX < 0.0f) || (targetVelX < 0.0f && currentVelX > 0.0f)) {
                    decayRate *= 2.0f;
                }

                if (currentVelX > targetVelX) {
                    currentVelX -= decayRate * deltaTime;
                    if (currentVelX <= targetVelX) {
                        currentVelX = targetVelX;
                        state_.hasDashInertia_ = false;
                    }
                } else if (currentVelX < targetVelX) {
                    currentVelX += decayRate * deltaTime;
                    if (currentVelX >= targetVelX) {
                        currentVelX = targetVelX;
                        state_.hasDashInertia_ = false;
                    }
                } else {
                    state_.hasDashInertia_ = false;
                }
                state_.velocity_.x = currentVelX;
            } else {
                state_.velocity_.x = targetVelX + state_.externalVelocityX_;
            }
        } else {
            // 壁キック直後の操作不能時間中の速度補間
            // 初速から「現在のキー入力に基づく目標速度」へ滑らかに減速・加速させる
            float t = 1.0f - (state_.wallJumpTimer_ / params_.wallJumpDuration_); // 0.0 (キック直後) -> 1.0 (タイマー終了時)
            
            // state_.lockedDirectionX_ を元にキックした壁の方向を判定し、初速を決定（動くブロックの慣性も乗せる）
            float startVelX = ((state_.lockedDirectionX_ == 1.0f) ? -params_.wallJumpPower_.x : params_.wallJumpPower_.x) + state_.externalVelocityX_;
            float endVelX = targetVelX + state_.externalVelocityX_;
            
            // 線形補間で速度を徐々に落とす
            state_.velocity_.x = startVelX + (endVelX - startVelX) * t;
        }

        // 壁ずり落ち / 壁張り付きの判定 (壁に向かって入力している場合)
        if (!state_.isOnGround_ && state_.wallClingReleaseTimer_ <= 0.0f) {
            bool tryingToCling = (input_.isClingHeld && !state_.isExhausted_);
            bool touchingWall = ((state_.isTouchingWallRight_ && inputRight) || (state_.isTouchingWallLeft_ && inputLeft));
            
            // 落下中、または壁をつかもうとしている時は判定を入れる
            if (touchingWall && (state_.velocity_.y <= 0.0f || tryingToCling)) {
                state_.isWallSliding_ = true;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                if (tryingToCling) {
                    state_.isWallClinging_ = true;
                }
            }
        }

        // ジャンプ
        if (state_.jumpBufferTimer_ > 0.0f && state_.climbLandingTimer_ <= 0.0f) {
            if (state_.isOnGround_ || state_.coyoteTimer_ > 0.0f) {
                state_.jumpBufferTimer_ = 0.0f;
                state_.coyoteTimer_ = 0.0f;

                // 直前にダッシュしており猶予時間内の場合 -> ダッシュジャンプ（前方慣性ジャンプ / Super Jump / Wave Dash）
                bool canSuperJump = (state_.dashJumpWindowTimer_ > 0.0f) && (std::abs(state_.lastDashDirX_) > 0.1f);
                // 逆方向に入力している場合はダッシュジャンプではなく通常ジャンプ
                if (canSuperJump && ((state_.lastDashDirX_ > 0.0f && input_.moveX < -0.1f) || (state_.lastDashDirX_ < 0.0f && input_.moveX > 0.1f))) {
                    canSuperJump = false;
                }

                if (canSuperJump) {
                    state_.velocity_.y = params_.jumpPower_ * params_.dashJumpPowerMult_;
                    float superJumpSpeed = params_.dashSpeed_ * params_.dashJumpSpeedMult_ + params_.dashJumpBonusSpeed_;
                    state_.velocity_.x = state_.lastDashDirX_ * superJumpSpeed;
                    state_.hasDashInertia_ = true;
                    state_.dashJumpWindowTimer_ = 0.0f;
                    if (params_.dashJumpRefreshDash_) {
                        state_.canDash_ = true;
                    }
                } else {
                    state_.velocity_.y = params_.jumpPower_;
                }

                // 足場に乗っている（または猶予期間中）場合は慣性を加算 (セレステ風)
                if (state_.platformInertiaTimer_ > 0.0f) {
                    state_.externalVelocityDuration_ = (state_.lastPlatformInertiaTime_ > 0.0f) ? state_.lastPlatformInertiaTime_ : 0.5f;
                    state_.externalVelocityTimer_ = state_.externalVelocityDuration_;
                    state_.initialExternalVelocityX_ = state_.recentPlatformVelocity_.x;
                    state_.externalVelocityX_ = state_.initialExternalVelocityX_;
                    state_.velocity_.x += state_.externalVelocityX_;
                    // 上方向（ジャンプ補助）の慣性のみ加算し、下方向へはジャンプ力を殺さないようにする
                    if (state_.recentPlatformVelocity_.y > 0.0f) {
                        state_.velocity_.y += state_.recentPlatformVelocity_.y;
                    }
                    
                    // ジャンプしたら猶予期間を終了する
                    state_.platformInertiaTimer_ = 0.0f;
                }
                state_.isOnGround_ = false;
                state_.isOnMovingPlatform_ = false;
                visuals_.SpawnJumpDust({state_.position_.x, state_.position_.y - params_.halfHeight_, 0.0f}, 0.0f);
            } else if (state_.isTouchingWallRight_) {
                state_.jumpBufferTimer_ = 0.0f;
                // 壁張り付き状態（Control入力がある場合）
                bool isPressingCling = input_.isClingHeld && !state_.isExhausted_;
                // 壁掴み中で逆方向(左)に入力がある場合は壁キック。それ以外は真上ジャンプ
                bool wantWallKick = (isPressingCling && input_.moveX < 0.0f) || !isPressingCling;

                if (!wantWallKick) {
                    // 壁張り付き中は真上ジャンプ
                    state_.velocity_.x = 0.0f;
                    state_.velocity_.y = params_.jumpPower_;
                    
                    state_.stamina_ -= params_.staminaConsumeJump_;
                    if (state_.stamina_ <= 0.0f) {
                        state_.stamina_ = 0.0f;
                        state_.isExhausted_ = true;
                    }
                    state_.wallClingReleaseTimer_ = 0.15f;
                } else {
                    // 右壁キック（左へ跳ね返る）
                    state_.facingDirectionX_ = -1.0f;
                    state_.velocity_.x = -params_.wallJumpPower_.x;
                    state_.velocity_.y = params_.wallJumpPower_.y;
                    state_.wallJumpTimer_ = params_.wallJumpDuration_;
                    state_.externalVelocityX_ = 0.0f; // デフォルトでリセット（動くブロックからの場合は後で上書きされる）
                    state_.externalVelocityTimer_ = 0.0f;
                    visuals_.SpawnJumpDust({state_.position_.x + params_.halfWidth_, state_.position_.y, 0.0f}, -1.0f);
                    
                    state_.wallJumpDirLockTimer_ = params_.wallJumpDirLockDuration_;
                    state_.lockedDirectionX_ = 1.0f; // 右方向への入力をロック
                    state_.wallClingReleaseTimer_ = 0.15f;
                }
                
                // 慣性を加算 (真上ジャンプ・壁キック共通)
                if (state_.platformInertiaTimer_ > 0.0f) {
                    state_.externalVelocityDuration_ = (state_.lastPlatformInertiaTime_ > 0.0f) ? state_.lastPlatformInertiaTime_ : 0.5f;
                    state_.externalVelocityTimer_ = state_.externalVelocityDuration_;
                    state_.initialExternalVelocityX_ = state_.recentPlatformVelocity_.x;
                    state_.externalVelocityX_ = state_.initialExternalVelocityX_;
                    state_.velocity_.x += state_.externalVelocityX_;
                    if (state_.recentPlatformVelocity_.y > 0.0f) {
                        state_.velocity_.y += state_.recentPlatformVelocity_.y;
                    }
                    state_.platformInertiaTimer_ = 0.0f;
                }
                state_.isTouchingWallRight_ = false;
                state_.isWallSliding_ = false;
                state_.isWallClinging_ = false;
                state_.climbingUpTimer_ = 0.0f;
            } else if (state_.isTouchingWallLeft_) {
                state_.jumpBufferTimer_ = 0.0f;
                // 壁張り付き状態（Control入力がある場合）
                bool isPressingCling = input_.isClingHeld && !state_.isExhausted_;
                // 壁掴み中で逆方向(右)に入力がある場合は壁キック。それ以外は真上ジャンプ
                bool wantWallKick = (isPressingCling && input_.moveX > 0.0f) || !isPressingCling;

                if (!wantWallKick) {
                    // 壁張り付き中は真上ジャンプ
                    state_.velocity_.x = 0.0f;
                    state_.velocity_.y = params_.jumpPower_;
                    
                    state_.stamina_ -= params_.staminaConsumeJump_;
                    if (state_.stamina_ <= 0.0f) {
                        state_.stamina_ = 0.0f;
                        state_.isExhausted_ = true;
                    }
                    state_.wallClingReleaseTimer_ = 0.15f;
                } else {
                    // 左壁キック（右へ跳ね返る）
                    state_.facingDirectionX_ = 1.0f;
                    state_.velocity_.x = params_.wallJumpPower_.x;
                    state_.velocity_.y = params_.wallJumpPower_.y;
                    state_.wallJumpTimer_ = params_.wallJumpDuration_;
                    state_.externalVelocityX_ = 0.0f; // デフォルトでリセット（動くブロックからの場合は後で上書きされる）
                    state_.externalVelocityTimer_ = 0.0f;
                    visuals_.SpawnJumpDust({state_.position_.x - params_.halfWidth_, state_.position_.y, 0.0f}, 1.0f);
                    
                    state_.wallJumpDirLockTimer_ = params_.wallJumpDirLockDuration_;
                    state_.lockedDirectionX_ = -1.0f; // 左方向への入力をロック
                    state_.wallClingReleaseTimer_ = 0.15f;
                }
                
                // 慣性を加算 (真上ジャンプ・壁キック共通)
                if (state_.platformInertiaTimer_ > 0.0f) {
                    state_.externalVelocityDuration_ = (state_.lastPlatformInertiaTime_ > 0.0f) ? state_.lastPlatformInertiaTime_ : 0.5f;
                    state_.externalVelocityTimer_ = state_.externalVelocityDuration_;
                    state_.initialExternalVelocityX_ = state_.recentPlatformVelocity_.x;
                    state_.externalVelocityX_ = state_.initialExternalVelocityX_;
                    state_.velocity_.x += state_.externalVelocityX_;
                    if (state_.recentPlatformVelocity_.y > 0.0f) {
                        state_.velocity_.y += state_.recentPlatformVelocity_.y;
                    }
                    state_.platformInertiaTimer_ = 0.0f;
                }
                state_.isTouchingWallLeft_ = false;
                state_.isWallSliding_ = false;
                state_.isWallClinging_ = false;
                state_.climbingUpTimer_ = 0.0f;
            }
        }

        // ジャンプキーを離したら下降が始まるようにする
        if (!state_.isOnGround_ && state_.velocity_.y > 0.0f) {
            if (input_.isJumpReleased) {
                state_.velocity_.y = 0.0f;
            }
        }
    }

    // ダッシュの入力検知（Jキー）
    if (state_.canDash_ && input_.isDashPressed && state_.climbLandingTimer_ <= 0.0f) {
        // 入力方向の取得
        Vector3 inputDir = {0.0f, 0.0f, 0.0f};
        if (input_.moveX < 0.0f) inputDir.x -= 1.0f;
        if (input_.moveX > 0.0f) inputDir.x += 1.0f;
        if (input_.moveY > 0.5f) inputDir.y += 1.0f;
        if (input_.moveY < -0.5f) inputDir.y -= 1.0f;

        // 入力が無い場合は向いている方向にするなどの処理が必要だが、とりあえず右とする
        if (inputDir.x == 0.0f && inputDir.y == 0.0f) {
            inputDir.x = (state_.facingDirectionX_ != 0.0f) ? state_.facingDirectionX_ : 1.0f;
        } else {
            inputDir = TransformFunctions::Normalize(inputDir);
        }

        if (inputDir.x > 0.01f) {
            state_.facingDirectionX_ = 1.0f;
        } else if (inputDir.x < -0.01f) {
            state_.facingDirectionX_ = -1.0f;
        }

        state_.dashVelocity_ = { inputDir.x * params_.dashSpeed_, inputDir.y * params_.dashSpeed_, 0.0f };
        state_.velocity_ = state_.dashVelocity_;
        state_.isDashing_ = true;
        state_.canDash_ = false;
        state_.dashTimer_ = 0.0f;
        state_.hasDashInertia_ = false;
        state_.lastDashDirX_ = (inputDir.x > 0.01f) ? 1.0f : ((inputDir.x < -0.01f) ? -1.0f : 0.0f);
        state_.dashJumpWindowTimer_ = params_.dashDuration_ + params_.dashJumpWindow_;
        
        // ダッシュ波紋を発生
        visuals_.SpawnDashRing(state_.position_, inputDir);

        // 画面揺れをトリガー (強さ 0.2f, 時間 0.15秒)
        if (player && player->GetCamera()) {
            player->GetCamera()->Shake(0.2f, 0.15f);
        }
    }
}

void PlayerPhysics::ApplyGravity(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime) {
    if (state_.isDashing_) return; // ダッシュ中は重力を無視

    if (state_.isWallClinging_ && !state_.isExhausted_) {
        // 斜め入力による壁登り速度の減衰をなくすため、上下入力は方向のみ (-1, 0, 1) に正規化する
        float climbDirY = 0.0f;
        if (input_.moveY > 0.1f) {
            climbDirY = 1.0f;
        } else if (input_.moveY < -0.1f) {
            climbDirY = -1.0f;
        }
        
        if (climbDirY > 0.0f) {
            state_.stamina_ -= params_.staminaConsumeClimb_ * deltaTime;
            state_.climbingUpTimer_ = 0.25f; // 上登り中を記録（着地検知用の猶予時間）
        } else {
            state_.stamina_ -= params_.staminaConsumeCling_ * deltaTime;
        }
        
        if (state_.stamina_ <= 0.0f) {
            state_.stamina_ = 0.0f;
            state_.isExhausted_ = true;
            state_.isWallClinging_ = false;
            state_.climbingUpTimer_ = 0.0f;
        }
        
        if (state_.isWallClinging_) {
            state_.velocity_.y = climbDirY * params_.wallClimbSpeed_; // W/スティック上で上、S/スティック下で下へ移動
            return;
        }
    }

    state_.velocity_.y += params_.gravity_ * deltaTime;

    // 最大落下速度を制限
    float currentMaxFallSpeed = params_.maxFallSpeed_;
    if (state_.isWallSliding_ && state_.velocity_.y < 0.0f) {
        if (state_.isExhausted_) {
            currentMaxFallSpeed = params_.wallSlideSpeed_ * 3.0f; // 疲労時は速くずり落ちる
        } else {
            currentMaxFallSpeed = params_.wallSlideSpeed_; // ずり落ち中はゆっくり落下
        }
    }

    if (state_.velocity_.y < currentMaxFallSpeed) {
        state_.velocity_.y = currentMaxFallSpeed;
    }
}

std::array<PlayerPhysics::HitPart, 2> PlayerPhysics::BuildHitParts(const PlayerState& state_, const PlayerParams& params_, bool usePreviousWallTouch) const {
    const bool isWallPose = state_.isWallClinging_ || state_.isWallSliding_;

    HitPart body;
    body.offsetX = 0.0f;
    body.offsetY = params_.bodyOffsetY_;
    body.halfWidth = params_.bodyHalfWidth_;
    body.halfHeight = params_.bodyHalfHeight_;
    if (isWallPose && params_.bodyWallBottomShrink_ > 0.0f) {
        float origTop = params_.bodyOffsetY_ + params_.bodyHalfHeight_;
        float newHalfH = (std::max)(0.05f, params_.bodyHalfHeight_ - params_.bodyWallBottomShrink_ * 0.5f);
        body.halfHeight = newHalfH;
        body.offsetY = origTop - newHalfH;
    }
    body.allowWallTouch = true;

    HitPart head;
    head.offsetX = params_.headOffsetX_ * state_.facingDirectionX_;
    if (isWallPose) {
        bool touchRight = usePreviousWallTouch ? state_.wasTouchingWallRight_ : (state_.isTouchingWallRight_ || state_.wasTouchingWallRight_);
        bool touchLeft = usePreviousWallTouch ? state_.wasTouchingWallLeft_ : (state_.isTouchingWallLeft_ || state_.wasTouchingWallLeft_);
        if (touchRight) {
            head.offsetX -= params_.headWallOffsetX_;
        } else if (touchLeft) {
            head.offsetX += params_.headWallOffsetX_;
        }
    }
    head.offsetY = params_.headOffsetY_;
    head.halfWidth = params_.headHalfWidth_;
    head.halfHeight = params_.headHalfHeight_;
    head.allowWallTouch = false;

    return { body, head };
}

AABB2D PlayerPhysics::MakePartAABB(const Vector3& position, const HitPart& part) {
    float cx = position.x + part.offsetX;
    float cy = position.y + part.offsetY;
    return {
        cx - part.halfWidth,
        cy + part.halfHeight,
        cx + part.halfWidth,
        cy - part.halfHeight
    };
}

void PlayerPhysics::ResolveCollisionY(PlayerState& state_, const PlayerParams& params_, float prevPosY, float moveDeltaY) {
    state_.isOnGround_ = false;
    state_.isOnMovingPlatform_ = false;
    state_.standingPlatformCollider_ = nullptr;
    state_.platformVelocity_ = {0.0f, 0.0f, 0.0f};

    const bool isWallPose = state_.isWallClinging_ || state_.isWallSliding_;
    float bodyPrevBottomOffset = 0.0f;
    if (!isWallPose && state_.wasWallPose_ && params_.bodyWallBottomShrink_ > 0.0f) {
        float shrunkHalfH = (std::max)(0.05f, params_.bodyHalfHeight_ - params_.bodyWallBottomShrink_ * 0.5f);
        bodyPrevBottomOffset = (params_.bodyHalfHeight_ - shrunkHalfH) * 2.0f;
    }

    auto parts = BuildHitParts(state_, params_);

    // 1. まず胴体パーツで床・天井を判定
    ResolveStaticCollisionY(state_, params_, parts[0], prevPosY, moveDeltaY, bodyPrevBottomOffset);
    ResolveDynamicCollisionY(state_, params_, parts[0], prevPosY, moveDeltaY, bodyPrevBottomOffset);

    // 壁掴み中に床へ着地した場合は立ち姿勢の足元高さに戻してから頭判定を行う
    if (isWallPose && state_.isOnGround_ && params_.bodyWallBottomShrink_ > 0.0f) {
        float actualShrink = (params_.bodyHalfHeight_ - parts[0].halfHeight) * 2.0f;
        state_.position_.y += actualShrink;
        state_.isWallClinging_ = false;
        state_.isWallSliding_ = false;
        parts = BuildHitParts(state_, params_);
    }

    // 2. 頭パーツで天井・床を判定
    ResolveStaticCollisionY(state_, params_, parts[1], prevPosY, moveDeltaY, 0.0f);
    ResolveDynamicCollisionY(state_, params_, parts[1], prevPosY, moveDeltaY, 0.0f);
}

void PlayerPhysics::ResolveStaticCollisionY(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset) {
    const float kSkinX = (std::min)(0.02f, part.halfWidth * 0.5f);
    AABB2D aabb = MakePartAABB(state_.position_, part);
    float partPosY = state_.position_.y + part.offsetY;
    float partPrevPosY = prevPosY + part.offsetY;

    AABB2D queryAABB;
    // 左右をわずかに削る（壁密着時の引っかかり防止）
    queryAABB.left = aabb.left + kSkinX;
    queryAABB.right = aabb.right - kSkinX;

    // 移動前後のY範囲をすべてカバー（高速落下・斜め落下時のすり抜け防止）
    queryAABB.top = (std::max)(partPrevPosY, partPosY) + part.halfHeight + 0.1f;
    queryAABB.bottom = (std::min)(partPrevPosY, partPosY) - part.halfHeight - 0.1f;

    float prevBottom = partPrevPosY - part.halfHeight + prevBottomOffset;
    float prevTop = partPrevPosY + part.halfHeight;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (collider->IsMoving()) continue; // Staticのみ
        if (!collider->IsSolid() && !collider->IsOneWay()) continue;

        AABB2D blockAABB = collider->GetAABB();

        if (aabb.right - kSkinX <= blockAABB.left || aabb.left + kSkinX >= blockAABB.right) {
            continue;
        }
        
        if (collider->IsOneWay()) {
            if (prevBottom < blockAABB.top - 0.08f || moveDeltaY > 0.0001f) {
                continue;
            }
        }

        bool wasAbove = (prevBottom >= blockAABB.top - 0.08f);
        bool wasBelow = (prevTop <= blockAABB.bottom + 0.08f);

        // 1. 移動前にブロックの上面より上にいた場合（斜め落下や角への着地でも確実にキャッチ）
        if (wasAbove && moveDeltaY <= 0.0001f && aabb.bottom <= blockAABB.top && prevBottom >= blockAABB.bottom) {
            state_.position_.y = blockAABB.top + part.halfHeight - part.offsetY;
            state_.velocity_.y = 0.0f;
            state_.isOnGround_ = true;
            state_.canDash_ = true;
            state_.wallJumpDirLockTimer_ = 0.0f;
            state_.lockedDirectionX_ = 0.0f;

            if (state_.climbingUpTimer_ > 0.0f) {
                state_.climbLandingTimer_ = params_.climbLandingDuration_;
                state_.climbingUpTimer_ = 0.0f;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
            }

            if (collider->GetUserData()) {
                BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                block->OnPlayerStand();
            }
            aabb = MakePartAABB(state_.position_, part);
            continue;
        }

        // 2. 移動前にブロックの下面より下にいた場合（下からの天井衝突）
        if (wasBelow && moveDeltaY >= -0.0001f && aabb.top >= blockAABB.bottom && prevTop <= blockAABB.top) {
            if (collider->IsOneWay()) continue;
            state_.position_.y = blockAABB.bottom - part.halfHeight - part.offsetY;
            state_.velocity_.y = 0.0f;
            aabb = MakePartAABB(state_.position_, part);
            continue;
        }

        // 3. 移動前から既にY範囲が重なっていた場合（横からのめり込み等）
        if (aabb.right > blockAABB.left && aabb.left < blockAABB.right &&
            aabb.top > blockAABB.bottom && aabb.bottom < blockAABB.top) {
            
            float overlapX = (std::min)(aabb.right, blockAABB.right) - (std::max)(aabb.left, blockAABB.left);
            float overlapY = (std::min)(aabb.top, blockAABB.top) - (std::max)(aabb.bottom, blockAABB.bottom);
            if (overlapY > overlapX) continue; // 壁への横からのめり込みの場合はY軸判定をスキップ
            
            float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
            partPosY = state_.position_.y + part.offsetY;
            if (partPosY > blockCenterY) {
                // 上に乗った
                state_.position_.y = blockAABB.top + part.halfHeight - part.offsetY;
                state_.velocity_.y = 0.0f;
                state_.isOnGround_ = true;
                state_.canDash_ = true;
                state_.wallJumpDirLockTimer_ = 0.0f;
                state_.lockedDirectionX_ = 0.0f;
                
                // 崖から登って地面についたときは少しの間停止する
                if (state_.climbingUpTimer_ > 0.0f) {
                    state_.climbLandingTimer_ = params_.climbLandingDuration_;
                    state_.climbingUpTimer_ = 0.0f;
                    state_.velocity_.x = 0.0f;
                    state_.externalVelocityX_ = 0.0f;
                    state_.externalVelocityTimer_ = 0.0f;
                }
                
                if (collider->GetUserData()) {
                    BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                    block->OnPlayerStand();
                }
            } else {
                // 下からぶつかった
                if (collider->IsOneWay()) continue; // OneWayは下からぶつかってもすり抜ける
                state_.position_.y = blockAABB.bottom - part.halfHeight - part.offsetY;
                state_.velocity_.y = 0.0f;
            }
            aabb = MakePartAABB(state_.position_, part);
        }
    }
}

void PlayerPhysics::ResolveDynamicCollisionY(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset) {
    AABB2D aabb = MakePartAABB(state_.position_, part);
    AABB2D shrunkAABBY = aabb;
    const float skinX = (std::min)(0.02f, part.halfWidth * 0.5f);
    shrunkAABBY.left += skinX;
    shrunkAABBY.right -= skinX;

    if (state_.isWallClinging_ || state_.isWallSliding_) {
        if (state_.isTouchingWallRight_) shrunkAABBY.right -= 0.15f;
        if (state_.isTouchingWallLeft_) shrunkAABBY.left += 0.15f;
    }

    float partPrevPosY = prevPosY + part.offsetY;
    float prevBottom = partPrevPosY - part.halfHeight + prevBottomOffset;
    float prevTop = partPrevPosY + part.halfHeight;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(shrunkAABBY, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider->IsSolid() || !collider->IsMoving()) continue;
        
        AABB2D blockAABB = collider->GetAABB();

        if (CheckAABBCollision(shrunkAABBY, blockAABB)) {
            bool wasAbove = (prevBottom >= blockAABB.top - 0.15f);
            bool wasBelow = (prevTop <= blockAABB.bottom + 0.15f);

            if (!wasAbove && !wasBelow) {
                float overlapX = (std::min)(shrunkAABBY.right, blockAABB.right) - (std::max)(shrunkAABBY.left, blockAABB.left);
                float overlapY = (std::min)(shrunkAABBY.top, blockAABB.top) - (std::max)(shrunkAABBY.bottom, blockAABB.bottom);
                if (overlapY > overlapX) continue; // 壁への横からのめり込みの場合はY軸判定をスキップ
            }
            
            float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
            float partPosY = state_.position_.y + part.offsetY;
            if (wasAbove || (!wasBelow && partPosY > blockCenterY)) { // 上から乗った
                state_.position_.y = blockAABB.top + part.halfHeight - part.offsetY;
                state_.velocity_.y = 0.0f;
                state_.isOnGround_ = true;
                state_.canDash_ = true;
                state_.wallJumpDirLockTimer_ = 0.0f;
                state_.lockedDirectionX_ = 0.0f;
                
                // 崖から登って地面についたときは少しの間停止する
                if (state_.climbingUpTimer_ > 0.0f) {
                    state_.climbLandingTimer_ = params_.climbLandingDuration_;
                    state_.climbingUpTimer_ = 0.0f;
                    state_.velocity_.x = 0.0f;
                    state_.externalVelocityX_ = 0.0f;
                    state_.externalVelocityTimer_ = 0.0f;
                }

                if (collider->GetUserData()) {
                    BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                    block->OnPlayerStand();
                }
                state_.isOnMovingPlatform_ = true;
                state_.standingPlatformCollider_ = collider;
                state_.platformVelocity_ = collider->GetVelocity();
            } else { // 下からぶつかった
                state_.position_.y = blockAABB.bottom - part.halfHeight - part.offsetY;
                state_.velocity_.y = 0.0f;
            }
            aabb = MakePartAABB(state_.position_, part);
            shrunkAABBY.top = aabb.top;
            shrunkAABBY.bottom = aabb.bottom;
        }
    }
}

void PlayerPhysics::ResolveCollisionX(PlayerState& state_, const PlayerParams& params_, float prevPosX, float moveDeltaX) {
    state_.wasTouchingWallLeft_ = state_.isTouchingWallLeft_;
    state_.wasTouchingWallRight_ = state_.isTouchingWallRight_;
    auto parts = BuildHitParts(state_, params_, true);

    state_.isTouchingWallLeft_ = false;
    state_.isTouchingWallRight_ = false;
    state_.wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
    state_.wallPlatformCollider_ = nullptr;

    // 静的ブロック判定
    for (const auto& part : parts) {
        ResolveStaticCollisionX(state_, params_, part, prevPosX, moveDeltaX);
    }

    // 動的ブロック判定
    for (const auto& part : parts) {
        ResolveDynamicCollisionX(state_, params_, part, prevPosX, moveDeltaX);
    }
}

void PlayerPhysics::ResolveStaticCollisionX(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosX, float moveDeltaX) {
    float partCenterX = state_.position_.x + part.offsetX;
    float partCenterY = state_.position_.y + part.offsetY;
    float partPrevPosX = prevPosX + part.offsetX;

    // 移動前後の全範囲をカバーするBroadphase AABB（高速移動時のトンネリング防止）
    float minX = (std::min)(partPrevPosX, partCenterX) - part.halfWidth - 0.05f;
    float maxX = (std::max)(partPrevPosX, partCenterX) + part.halfWidth + 0.05f;

    const float skinY = (std::min)(0.05f, part.halfHeight * 0.5f);
    AABB2D queryAABB;
    queryAABB.left = minX;
    queryAABB.right = maxX;
    // 上下を少し削る（足元の床ブロックや頭上の天井ブロックとの誤判定防止）
    queryAABB.top = partCenterY + part.halfHeight - skinY;
    queryAABB.bottom = partCenterY - part.halfHeight + skinY;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);

    // 静的Solidブロックを抽出
    std::vector<AABB2D> hitBlocks;
    hitBlocks.reserve(colliders.size());
    for (auto* collider : colliders) {
        if (!collider || collider->IsMoving() || !collider->IsSolid()) continue;
        AABB2D blockAABB = collider->GetAABB();
        // 上下方向が実際に重なっているか確認
        if (queryAABB.top > blockAABB.bottom && queryAABB.bottom < blockAABB.top) {
            hitBlocks.push_back(blockAABB);
        }
    }

    if (hitBlocks.empty()) return;

    // 移動方向に応じた確実な押し戻し
    // 1. 右向きへの移動
    if (moveDeltaX > 0.0001f || state_.velocity_.x > 0.0001f) {
        float earliestPartX = partCenterX;
        bool collided = false;
        for (const auto& blockAABB : hitBlocks) {
            // 現在の右端がブロック左端を超えており、かつ移動前の右端がブロック左端の手前側にあった
            if (partCenterX + part.halfWidth > blockAABB.left &&
                partPrevPosX + part.halfWidth <= blockAABB.left + 0.05f) {
                float targetPartX = blockAABB.left - part.halfWidth;
                if (!collided || targetPartX < earliestPartX) {
                    earliestPartX = targetPartX;
                    collided = true;
                }
            }
        }
        if (collided) {
            state_.position_.x = earliestPartX - part.offsetX;
            state_.velocity_.x = 0.0f;
            state_.externalVelocityX_ = 0.0f;
            state_.externalVelocityTimer_ = 0.0f;
            state_.hasDashInertia_ = false;
            state_.dashJumpWindowTimer_ = 0.0f;
            state_.lastDashDirX_ = 0.0f;
            if (part.allowWallTouch) {
                state_.isTouchingWallRight_ = true;
            }
        }
    }
    // 2. 左向きへの移動
    else if (moveDeltaX < -0.0001f || state_.velocity_.x < -0.0001f) {
        float earliestPartX = partCenterX;
        bool collided = false;
        for (const auto& blockAABB : hitBlocks) {
            // 現在の左端がブロック右端を超えており、かつ移動前の左端がブロック右端の手前側にあった
            if (partCenterX - part.halfWidth < blockAABB.right &&
                partPrevPosX - part.halfWidth >= blockAABB.right - 0.05f) {
                float targetPartX = blockAABB.right + part.halfWidth;
                if (!collided || targetPartX > earliestPartX) {
                    earliestPartX = targetPartX;
                    collided = true;
                }
            }
        }
        if (collided) {
            state_.position_.x = earliestPartX - part.offsetX;
            state_.velocity_.x = 0.0f;
            state_.externalVelocityX_ = 0.0f;
            state_.externalVelocityTimer_ = 0.0f;
            state_.hasDashInertia_ = false;
            state_.dashJumpWindowTimer_ = 0.0f;
            state_.lastDashDirX_ = 0.0f;
            if (part.allowWallTouch) {
                state_.isTouchingWallLeft_ = true;
            }
        }
    }
    // 3. 残りのめり込み解決（斜め移動時や静止状態でのすり抜け防止）
    {
        AABB2D currentAABB = MakePartAABB(state_.position_, part);
        for (const auto& blockAABB : hitBlocks) {
            if (currentAABB.right > blockAABB.left && currentAABB.left < blockAABB.right) {
                float overlapLeft = currentAABB.right - blockAABB.left;
                float overlapRight = blockAABB.right - currentAABB.left;
                if (overlapLeft < overlapRight) {
                    state_.position_.x = blockAABB.left - part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state_.isTouchingWallRight_ = true;
                    }
                } else {
                    state_.position_.x = blockAABB.right + part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state_.isTouchingWallLeft_ = true;
                    }
                }
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                currentAABB = MakePartAABB(state_.position_, part);
            }
        }
    }

    // 押し出し後、壁に密着している場合の壁接触フラグの補正（壁掴み・壁スライドを確実に維持）
    if (part.allowWallTouch) {
        const float kContactMargin = 0.02f;
        AABB2D finalAABB = MakePartAABB(state_.position_, part);
        for (const auto& blockAABB : hitBlocks) {
            if (std::abs(finalAABB.right - blockAABB.left) <= kContactMargin) {
                state_.isTouchingWallRight_ = true;
            }
            if (std::abs(finalAABB.left - blockAABB.right) <= kContactMargin) {
                state_.isTouchingWallLeft_ = true;
            }
        }
    }
}

void PlayerPhysics::ResolveDynamicCollisionX(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosX, float moveDeltaX) {
    float partCenterX = state_.position_.x + part.offsetX;
    float partCenterY = state_.position_.y + part.offsetY;
    float partPrevPosX = prevPosX + part.offsetX;

    float minX = (std::min)(partPrevPosX, partCenterX) - part.halfWidth - 0.05f;
    float maxX = (std::max)(partPrevPosX, partCenterX) + part.halfWidth + 0.05f;

    const float skinY = (std::min)(0.05f, part.halfHeight * 0.5f);
    AABB2D queryAABB;
    queryAABB.left = minX;
    queryAABB.right = maxX;
    queryAABB.top = partCenterY + part.halfHeight - skinY;
    queryAABB.bottom = partCenterY - part.halfHeight + skinY;

    if (state_.isWallClinging_ || state_.isWallSliding_) {
        if (state_.isTouchingWallRight_ || state_.wasTouchingWallRight_) queryAABB.right += 0.2f;
        if (state_.isTouchingWallLeft_ || state_.wasTouchingWallLeft_) queryAABB.left -= 0.2f;
    }

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider || !collider->IsSolid() || !collider->IsMoving()) continue;
        
        AABB2D blockAABB = collider->GetAABB();
        if (queryAABB.top <= blockAABB.bottom || queryAABB.bottom >= blockAABB.top) continue;

        Vector3 platVel = collider->GetVelocity();
        float dt = TimeManager::GetInstance().GetDeltaTime();
        float relMoveDeltaX = moveDeltaX - platVel.x * dt;

        AABB2D currentAABB = MakePartAABB(state_.position_, part);
        partCenterX = state_.position_.x + part.offsetX;
        if (CheckAABBCollision(currentAABB, blockAABB) || 
            (partCenterX + part.halfWidth > blockAABB.left && partCenterX - part.halfWidth < blockAABB.right)) {
            
            // 相対移動方向または移動前の位置から押し戻し方向を決定
            if ((relMoveDeltaX > 0.0001f && partPrevPosX + part.halfWidth <= blockAABB.left + 0.05f) || (partPrevPosX + part.halfWidth <= blockAABB.left)) {
                state_.position_.x = blockAABB.left - part.halfWidth - part.offsetX;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                if (part.allowWallTouch) {
                    state_.isTouchingWallRight_ = true;
                    state_.wallPlatformCollider_ = collider;
                    state_.wallPlatformVelocity_ = platVel;
                }
            } else if ((relMoveDeltaX < -0.0001f && partPrevPosX - part.halfWidth >= blockAABB.right - 0.05f) || (partPrevPosX - part.halfWidth >= blockAABB.right)) {
                state_.position_.x = blockAABB.right + part.halfWidth - part.offsetX;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                if (part.allowWallTouch) {
                    state_.isTouchingWallLeft_ = true;
                    state_.wallPlatformCollider_ = collider;
                    state_.wallPlatformVelocity_ = platVel;
                }
            } else {
                float overlapLeft = (currentAABB.right - blockAABB.left);
                float overlapRight = (blockAABB.right - currentAABB.left);
                if (overlapLeft < overlapRight) {
                    state_.position_.x = blockAABB.left - part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state_.isTouchingWallRight_ = true;
                    }
                } else {
                    state_.position_.x = blockAABB.right + part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state_.isTouchingWallLeft_ = true;
                    }
                }
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                if (part.allowWallTouch) {
                    state_.wallPlatformCollider_ = collider;
                    state_.wallPlatformVelocity_ = platVel;
                }
            }

            if (part.allowWallTouch && collider->GetUserData()) {
                BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                block->OnPlayerTouch();
            }
        }
    }
}

void PlayerPhysics::SimulateCollisions(PlayerState& state_, const PlayerParams& params_, Player2D* player) {
    AABB2D bodyAABB = GetBodyAABB(state_, params_);
    AABB2D headAABB = GetHeadAABB(state_, params_);

    const float normalMargin = 0.02f;
    AABB2D queryAABB = {
        (std::min)(bodyAABB.left, headAABB.left) - normalMargin,
        (std::max)(bodyAABB.top, headAABB.top) + normalMargin,
        (std::max)(bodyAABB.right, headAABB.right) + normalMargin,
        (std::min)(bodyAABB.bottom, headAABB.bottom) - normalMargin
    };

    auto checkPartHit = [](const AABB2D& partAABB, const AABB2D& blockAABB, float margin, bool ignoreCornerGraze) -> bool {
        float left = partAABB.left - margin;
        float top = partAABB.top + margin;
        float right = partAABB.right + margin;
        float bottom = partAABB.bottom - margin;

        if (right > blockAABB.left && left < blockAABB.right &&
            top > blockAABB.bottom && bottom < blockAABB.top) {
            if (ignoreCornerGraze) {
                float overlapX = (std::min)(right, blockAABB.right) - (std::max)(left, blockAABB.left);
                float overlapY = (std::min)(top, blockAABB.top) - (std::max)(bottom, blockAABB.bottom);
                // 壁に張り付いている状態で上にあるブロックの角と数ミリ接しただけの点接触は除外
                if (overlapX <= 0.02f && overlapY <= 0.02f) {
                    return false;
                }
            }
            return true;
        }
        return false;
    };

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider->GetUserData()) continue;
        BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
        AABB2D blockAABB = collider->GetAABB();

        const bool isHazard = block->IsHazard();
        const float margin = isHazard ? 0.005f : normalMargin;
        if (checkPartHit(bodyAABB, blockAABB, margin, isHazard) ||
            checkPartHit(headAABB, blockAABB, margin, isHazard)) {
            block->OnCollision(player);
        }
    }
}

AABB2D PlayerPhysics::GetAABB(const PlayerState& state_, const PlayerParams& params_) const {
    AABB2D body = GetBodyAABB(state_, params_);
    AABB2D head = GetHeadAABB(state_, params_);
    return {
        (std::min)(body.left, head.left),
        (std::max)(body.top, head.top),
        (std::max)(body.right, head.right),
        (std::min)(body.bottom, head.bottom)
    };
}

AABB2D PlayerPhysics::GetBodyAABB(const PlayerState& state_, const PlayerParams& params_) const {
    auto parts = BuildHitParts(state_, params_);
    return MakePartAABB(state_.position_, parts[0]);
}

AABB2D PlayerPhysics::GetHeadAABB(const PlayerState& state_, const PlayerParams& params_) const {
    auto parts = BuildHitParts(state_, params_);
    return MakePartAABB(state_.position_, parts[1]);
}

bool PlayerPhysics::CheckAABBCollision(const AABB2D& a, const AABB2D& b) const {
    if (a.right < b.left || a.left > b.right) return false;
    if (a.top < b.bottom || a.bottom > b.top) return false;
    return true;
}
