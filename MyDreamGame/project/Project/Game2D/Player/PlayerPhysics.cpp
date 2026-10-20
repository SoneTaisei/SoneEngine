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
    if (state_.isOnMovingPlatform_ && state_.isOnGround_) {
        state_.position_.y += state_.platformVelocity_.y * deltaTime;
    }
    if (state_.isWallClinging_ || state_.isWallSliding_) {
        state_.position_.y += state_.wallPlatformVelocity_.y * deltaTime;
    }
    state_.position_.y += state_.velocity_.y * deltaTime;
    ResolveCollisionY(state_, params_);

    // --- X軸（左右）の移動と当たり判定 ---
    if (state_.isOnMovingPlatform_ && state_.isOnGround_) {
        state_.position_.x += state_.platformVelocity_.x * deltaTime;
    }
    if (state_.isWallClinging_ || state_.isWallSliding_) {
        state_.position_.x += state_.wallPlatformVelocity_.x * deltaTime;
    }
    state_.position_.x += state_.velocity_.x * deltaTime;
    ResolveCollisionX(state_, params_);

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
        float moveY = input_.moveY;
        
        if (moveY > 0.1f) {
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
            state_.velocity_.y = moveY * params_.wallClimbSpeed_; // W/スティック上で上、S/スティック下で下へ移動
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

void PlayerPhysics::ResolveCollisionY(PlayerState& state_, const PlayerParams& params_) {
    state_.isOnGround_ = false;
    state_.isOnMovingPlatform_ = false;
    state_.standingPlatformCollider_ = nullptr;
    state_.platformVelocity_ = {0.0f, 0.0f, 0.0f};

    AABB2D aabb = GetAABB(state_, params_);

    // 静的ブロック判定
    ResolveStaticCollisionY(state_, params_, aabb);

    // 動的ブロック判定
    aabb = GetAABB(state_, params_); // 静的ブロックで位置が変わった可能性があるので再取得
    ResolveDynamicCollisionY(state_, params_, aabb);
}

void PlayerPhysics::ResolveStaticCollisionY(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb) {
    AABB2D queryAABB = aabb;
    
    // 左右を少し削る（壁との引っかかり防止）
    queryAABB.left += 0.05f;
    queryAABB.right -= 0.05f;

    // 上下に拡張（速度ゼロでもめり込みを検知するため）
    queryAABB.top += 0.1f;
    queryAABB.bottom -= 0.1f;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (collider->IsMoving()) continue; // Staticのみ
        if (!collider->IsSolid() && !collider->IsOneWay()) continue;

        AABB2D blockAABB = collider->GetAABB();
        
        if (collider->IsOneWay()) {
            float previousBottom = state_.position_.y - state_.velocity_.y * TimeManager::GetInstance().GetDeltaTime() - params_.halfHeight_;
            if (previousBottom < blockAABB.top - 0.05f) {
                continue;
            }
        }

        if (aabb.right > blockAABB.left && aabb.left < blockAABB.right &&
            aabb.top > blockAABB.bottom && aabb.bottom < blockAABB.top) {
            
            float overlapX = (std::min)(aabb.right, blockAABB.right) - (std::max)(aabb.left, blockAABB.left);
            float overlapY = (std::min)(aabb.top, blockAABB.top) - (std::max)(aabb.bottom, blockAABB.bottom);
            if (overlapY > overlapX) continue; // 壁への横からのめり込みの場合はY軸判定をスキップ
            
            float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
            if (state_.position_.y > blockCenterY) {
                // 上に乗った
                state_.position_.y = blockAABB.top + params_.halfHeight_;
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
                state_.position_.y = blockAABB.bottom - params_.halfHeight_;
                state_.velocity_.y = 0.0f;
            }
        }
    }
}

void PlayerPhysics::ResolveDynamicCollisionY(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb) {
    AABB2D shrunkAABBY = aabb;
    shrunkAABBY.left += 0.05f;
    shrunkAABBY.right -= 0.05f;

    if (state_.isWallClinging_ || state_.isWallSliding_) {
        if (state_.isTouchingWallRight_) shrunkAABBY.right -= 0.15f;
        if (state_.isTouchingWallLeft_) shrunkAABBY.left += 0.15f;
    }

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(shrunkAABBY, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider->IsSolid() || !collider->IsMoving()) continue;
        
        AABB2D blockAABB = collider->GetAABB();

        if (CheckAABBCollision(shrunkAABBY, blockAABB)) {
            float overlapX = (std::min)(shrunkAABBY.right, blockAABB.right) - (std::max)(shrunkAABBY.left, blockAABB.left);
            float overlapY = (std::min)(shrunkAABBY.top, blockAABB.top) - (std::max)(shrunkAABBY.bottom, blockAABB.bottom);
            if (overlapY > overlapX) continue; // 壁への横からのめり込みの場合はY軸判定をスキップ
            
            float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
            if (state_.position_.y > blockCenterY) { // 上から乗った
                state_.position_.y = blockAABB.top + params_.halfHeight_;
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
                state_.position_.y = blockAABB.bottom - params_.halfHeight_;
                state_.velocity_.y = 0.0f;
            }
        }
    }
}

void PlayerPhysics::ResolveCollisionX(PlayerState& state_, const PlayerParams& params_) {
    state_.wasTouchingWallLeft_ = state_.isTouchingWallLeft_;
    state_.wasTouchingWallRight_ = state_.isTouchingWallRight_;
    state_.isTouchingWallLeft_ = false;
    state_.isTouchingWallRight_ = false;

    AABB2D aabb = GetAABB(state_, params_);

    // 静的ブロック判定
    ResolveStaticCollisionX(state_, params_, aabb);

    // 動的ブロック判定
    aabb = GetAABB(state_, params_); // 静的ブロックで位置が変わった可能性があるので再取得
    ResolveDynamicCollisionX(state_, params_, aabb);
}

void PlayerPhysics::ResolveStaticCollisionX(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb) {
    AABB2D queryAABB = aabb;
    // 上下を少し削る
    queryAABB.top -= 0.05f;
    queryAABB.bottom += 0.05f;
    
    // 左右に拡張（速度ゼロでもめり込みを検知するため）
    queryAABB.left -= 0.1f;
    queryAABB.right += 0.1f;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (collider->IsMoving() || !collider->IsSolid()) continue;
        
        AABB2D blockAABB = collider->GetAABB();
        if (aabb.right > blockAABB.left && aabb.left < blockAABB.right &&
            queryAABB.top > blockAABB.bottom && queryAABB.bottom < blockAABB.top) {
            
            float overlapX = (std::min)(aabb.right, blockAABB.right) - (std::max)(aabb.left, blockAABB.left);
            float overlapY = (std::min)(queryAABB.top, blockAABB.top) - (std::max)(queryAABB.bottom, blockAABB.bottom);
            if (overlapX > overlapY) continue; // 上下へのめり込みの場合はX軸判定をスキップ
            
            float blockCenterX = (blockAABB.left + blockAABB.right) * 0.5f;
            if (state_.position_.x < blockCenterX) {
                state_.position_.x = blockAABB.left - params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                state_.isTouchingWallRight_ = true;
            } else {
                state_.position_.x = blockAABB.right + params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                state_.isTouchingWallLeft_ = true;
            }
        }
    }
}

void PlayerPhysics::ResolveDynamicCollisionX(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb) {
    state_.wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f }; // 毎フレームリセット
    state_.wallPlatformCollider_ = nullptr;
    
    AABB2D shrunkAABBX = aabb;
    shrunkAABBX.top -= 0.05f;
    shrunkAABBX.bottom += 0.05f;

    if (state_.isWallClinging_ || state_.isWallSliding_) {
        if (state_.isTouchingWallRight_ || state_.wasTouchingWallRight_) shrunkAABBX.right += 0.2f;
        if (state_.isTouchingWallLeft_ || state_.wasTouchingWallLeft_) shrunkAABBX.left -= 0.2f;
    }

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(shrunkAABBX, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider->IsSolid() || !collider->IsMoving()) continue;
        
        AABB2D blockAABB = collider->GetAABB();

        if (CheckAABBCollision(shrunkAABBX, blockAABB)) {
            float overlapX = (std::min)(shrunkAABBX.right, blockAABB.right) - (std::max)(shrunkAABBX.left, blockAABB.left);
            float overlapY = (std::min)(shrunkAABBX.top, blockAABB.top) - (std::max)(shrunkAABBX.bottom, blockAABB.bottom);
            if (overlapY > overlapX) continue; // 上下へのめり込みの場合はX軸判定をスキップ
            
            float blockCenterX = (blockAABB.left + blockAABB.right) * 0.5f;
            if (state_.position_.x < blockCenterX) {
                state_.position_.x = blockAABB.left - params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                state_.isTouchingWallRight_ = true;
                state_.wallPlatformCollider_ = collider;
                state_.wallPlatformVelocity_ = collider->GetVelocity();
            } else {
                state_.position_.x = blockAABB.right + params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.externalVelocityX_ = 0.0f;
                state_.externalVelocityTimer_ = 0.0f;
                state_.hasDashInertia_ = false;
                state_.dashJumpWindowTimer_ = 0.0f;
                state_.lastDashDirX_ = 0.0f;
                state_.isTouchingWallLeft_ = true;
                state_.wallPlatformCollider_ = collider;
                state_.wallPlatformVelocity_ = collider->GetVelocity();
            }
            if (collider->GetUserData()) {
                BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                block->OnPlayerTouch();
            }
        }
    }
}

void PlayerPhysics::SimulateCollisions(PlayerState& state_, const PlayerParams& params_, Player2D* player) {
    AABB2D aabb = GetAABB(state_, params_);
    const float margin = 0.02f;
    AABB2D playerAABB = {
        aabb.left - margin,
        aabb.top + margin,
        aabb.right + margin,
        aabb.bottom - margin
    };

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(playerAABB, kLayerBlock);
    for (auto* collider : colliders) {
        AABB2D blockAABB = collider->GetAABB();

        if (playerAABB.right > blockAABB.left && playerAABB.left < blockAABB.right &&
            playerAABB.top > blockAABB.bottom && playerAABB.bottom < blockAABB.top) {
            
            if (collider->GetUserData()) {
                BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                block->OnCollision(player);
            }
        }
    }
}

AABB2D PlayerPhysics::GetAABB(const PlayerState& state_, const PlayerParams& params_) const {
    return {
        state_.position_.x - params_.halfWidth_,   // left
        state_.position_.y + params_.halfHeight_,  // top
        state_.position_.x + params_.halfWidth_,   // right
        state_.position_.y - params_.halfHeight_   // bottom
    };
}

bool PlayerPhysics::CheckAABBCollision(const AABB2D& a, const AABB2D& b) const {
    if (a.right < b.left || a.left > b.right) return false;
    if (a.top < b.bottom || a.bottom > b.top) return false;
    return true;
}
