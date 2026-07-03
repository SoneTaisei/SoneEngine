#include "PlayerPhysics.h"
#include "Player2D.h"
#include "Core/TimeManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "../Blocks/BaseBlock.h"
#include <algorithm>
#include <cmath>

#include "Collision/CollisionManager.h"

void PlayerPhysics::Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player) {
    if (state_.isDead_ || state_.isGoal_) return;

    HandleInputLogic(state_, params_, input_, visuals_, deltaTime);
    ApplyGravity(state_, params_, deltaTime);

    // ダッシュタイマーの更新
    if (state_.isDashing_) {
        state_.dashTimer_ += deltaTime;
        if (state_.dashTimer_ >= params_.dashDuration_) {
            state_.isDashing_ = false;
            // ダッシュ終了時、上向きの速度が残っている場合は設定した上限値にする。
            if (state_.velocity_.y > params_.dashEndUpwardVelocity_) {
                state_.velocity_.y = params_.dashEndUpwardVelocity_;
            }
        } else {
            // ダッシュ中は固定速度
            state_.velocity_ = state_.dashVelocity_;
        }
    }

    // --- 足場による移動（予測） ---
    if (state_.isOnMovingPlatform_ && state_.isOnGround_) {
        state_.position_.x += state_.platformVelocity_.x * deltaTime;
        state_.position_.y += state_.platformVelocity_.y * deltaTime;
    }
    if (state_.isWallClinging_ || state_.isWallSliding_) {
        state_.position_.x += state_.wallPlatformVelocity_.x * deltaTime;
        state_.position_.y += state_.wallPlatformVelocity_.y * deltaTime;
        
        if (std::abs(state_.wallPlatformVelocity_.x) > 0.01f || std::abs(state_.wallPlatformVelocity_.y) > 0.01f) {
            state_.recentPlatformVelocity_ = state_.wallPlatformVelocity_;
            state_.platformInertiaTimer_ = 0.1f;
        }
    }

    // --- Y軸（上下）の移動と当たり判定 ---
    state_.position_.y += state_.velocity_.y * deltaTime;
    ResolveCollisionY(state_, params_);

    // --- X軸（左右）の移動と当たり判定 ---
    state_.position_.x += state_.velocity_.x * deltaTime;
    ResolveCollisionX(state_, params_);

    // 非Solidブロック（コインなど）や全ブロックのOnCollision処理
    SimulateCollisions(state_, params_, player);
}

void PlayerPhysics::HandleInputLogic(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime) {
    
    state_.isWallSliding_ = false;
    state_.isWallClinging_ = false;

    // リフトの慣性猶予（コヨーテタイム）の更新
    float dt = TimeManager::GetInstance().GetDeltaTime();
    if (state_.isOnMovingPlatform_) {
        // リフトが動いていれば最新の速度を記録し、猶予時間をリセット
        if (std::abs(state_.platformVelocity_.x) > 0.01f || std::abs(state_.platformVelocity_.y) > 0.01f) {
            state_.recentPlatformVelocity_ = state_.platformVelocity_;
            state_.platformInertiaTimer_ = 0.1f; // コヨーテタイムを0.1秒に変更
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

    // 通常時の左右移動（ダッシュ中でない場合）
    if (!state_.isDashing_) {
        bool inputLeft = input_.moveX < 0.0f;
        bool inputRight = input_.moveX > 0.0f;

        // 壁掴み（Kキー）を押している場合、前フレームで壁に接触していれば、その方向への仮想入力を維持する
        // これにより方向キーを離しても張り付き続ける
        if (!state_.isOnGround_ && input_.isClingHeld) {
            if (state_.isTouchingWallRight_ && !inputLeft) inputRight = true;
            if (state_.isTouchingWallLeft_ && !inputRight) inputLeft = true;
        }

        float targetVelX = 0.0f;
        
        bool allowLeft = true;
        bool allowRight = true;
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
            state_.externalVelocityX_ = 0.0f; // 地上にいるときは慣性をリセット
        } else {
            // 空中では緩やかに減衰（空気抵抗）
            float dt = TimeManager::GetInstance().GetDeltaTime();
            float decayRate = 5.0f * dt;
            if (state_.externalVelocityX_ > 0.0f) {
                state_.externalVelocityX_ -= decayRate;
                if (state_.externalVelocityX_ < 0.0f) state_.externalVelocityX_ = 0.0f;
            } else if (state_.externalVelocityX_ < 0.0f) {
                state_.externalVelocityX_ += decayRate;
                if (state_.externalVelocityX_ > 0.0f) state_.externalVelocityX_ = 0.0f;
            }
        }

        if (state_.wallJumpTimer_ <= 0.0f) {
            state_.velocity_.x = targetVelX + state_.externalVelocityX_;
        } else {
            // 壁キック直後の操作不能時間中の速度補間
            // 初速から「現在のキー入力に基づく目標速度」へ滑らかに減速・加速させる
            float t = 1.0f - (state_.wallJumpTimer_ / params_.wallJumpDuration_); // 0.0 (キック直後) -> 1.0 (タイマー終了時)
            
            // state_.lockedDirectionX_ を元にキックした壁の方向を判定し、初速を決定
            float startVelX = (state_.lockedDirectionX_ == 1.0f) ? -params_.wallJumpPower_.x : params_.wallJumpPower_.x;
            float endVelX = targetVelX + state_.externalVelocityX_;
            
            // 線形補間で速度を徐々に落とす
            state_.velocity_.x = startVelX + (endVelX - startVelX) * t;
        }

        // 壁ずり落ち / 壁張り付きの判定 (空中で、落下中か静止中の場合のみ)
        if (!state_.isOnGround_ && state_.velocity_.y <= 0.0f) {
            if ((state_.isTouchingWallRight_ && inputRight) || (state_.isTouchingWallLeft_ && inputLeft)) {
                state_.isWallSliding_ = true;
                state_.externalVelocityX_ = 0.0f;
                if (input_.isClingHeld) {
                    state_.isWallClinging_ = true;
                }
            }
        }

        // ジャンプ
        if (input_.isJumpPressed) {
            if (state_.isOnGround_) {
                state_.velocity_.y = params_.jumpPower_;
                // 足場に乗っている（または猶予期間中）場合は慣性を加算 (セレステ風)
                if (state_.platformInertiaTimer_ > 0.0f) {
                    state_.externalVelocityX_ = state_.recentPlatformVelocity_.x;
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
                // 壁張り付き状態（Control入力がある場合）は真上ジャンプを優先
                bool isPressingCling = input_.isClingHeld;
                if (state_.isWallClinging_ || isPressingCling) {
                    // 壁張り付き中は真上ジャンプ
                    state_.velocity_.x = 0.0f;
                    state_.velocity_.y = params_.jumpPower_;
                    
                    // 慣性を加算
                    if (state_.platformInertiaTimer_ > 0.0f) {
                        state_.externalVelocityX_ = state_.recentPlatformVelocity_.x;
                        state_.velocity_.x += state_.externalVelocityX_;
                        if (state_.recentPlatformVelocity_.y > 0.0f) {
                            state_.velocity_.y += state_.recentPlatformVelocity_.y;
                        }
                        state_.platformInertiaTimer_ = 0.0f;
                    }
                } else {
                    // 右壁キック（左へ跳ね返る）
                    state_.velocity_.x = -params_.wallJumpPower_.x;
                    state_.velocity_.y = params_.wallJumpPower_.y;
                    state_.wallJumpTimer_ = params_.wallJumpDuration_;
                    state_.externalVelocityX_ = 0.0f; // 壁ジャンプ時に慣性をリセット
                    visuals_.SpawnJumpDust({state_.position_.x + params_.halfWidth_, state_.position_.y, 0.0f}, -1.0f);
                    
                    state_.wallJumpDirLockTimer_ = params_.wallJumpDirLockDuration_;
                    state_.lockedDirectionX_ = 1.0f; // 右方向への入力をロック
                }
                state_.isTouchingWallRight_ = false;
                state_.isWallSliding_ = false;
                state_.isWallClinging_ = false;
            } else if (state_.isTouchingWallLeft_) {
                // 壁張り付き状態（Control入力がある場合）は真上ジャンプを優先
                bool isPressingCling = input_.isClingHeld;
                if (state_.isWallClinging_ || isPressingCling) {
                    // 壁張り付き中は真上ジャンプ
                    state_.velocity_.x = 0.0f;
                    state_.velocity_.y = params_.jumpPower_;
                    
                    // 慣性を加算
                    if (state_.platformInertiaTimer_ > 0.0f) {
                        state_.externalVelocityX_ = state_.recentPlatformVelocity_.x;
                        state_.velocity_.x += state_.externalVelocityX_;
                        if (state_.recentPlatformVelocity_.y > 0.0f) {
                            state_.velocity_.y += state_.recentPlatformVelocity_.y;
                        }
                        state_.platformInertiaTimer_ = 0.0f;
                    }
                } else {
                    // 左壁キック（右へ跳ね返る）
                    state_.velocity_.x = params_.wallJumpPower_.x;
                    state_.velocity_.y = params_.wallJumpPower_.y;
                    state_.wallJumpTimer_ = params_.wallJumpDuration_;
                    state_.externalVelocityX_ = 0.0f; // 壁ジャンプ時に慣性をリセット
                    visuals_.SpawnJumpDust({state_.position_.x - params_.halfWidth_, state_.position_.y, 0.0f}, 1.0f);
                    
                    state_.wallJumpDirLockTimer_ = params_.wallJumpDirLockDuration_;
                    state_.lockedDirectionX_ = -1.0f; // 左方向への入力をロック
                }
                state_.isTouchingWallLeft_ = false;
                state_.isWallSliding_ = false;
                state_.isWallClinging_ = false;
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
    if (state_.canDash_ && !state_.isDashing_ && input_.isDashPressed) {
        // 入力方向の取得
        Vector3 inputDir = {0.0f, 0.0f, 0.0f};
        if (input_.moveX < 0.0f) inputDir.x -= 1.0f;
        if (input_.moveX > 0.0f) inputDir.x += 1.0f;
        if (input_.moveY > 0.5f) inputDir.y += 1.0f;
        if (input_.moveY < -0.5f) inputDir.y -= 1.0f;

        // 入力が無い場合は向いている方向にするなどの処理が必要だが、とりあえず右とする
        if (inputDir.x == 0.0f && inputDir.y == 0.0f) {
            inputDir.x = 1.0f;
        } else {
            inputDir = TransformFunctions::Normalize(inputDir);
        }

        state_.dashVelocity_ = { inputDir.x * params_.dashSpeed_, inputDir.y * params_.dashSpeed_, 0.0f };
        state_.velocity_ = state_.dashVelocity_;
        state_.isDashing_ = true;
        state_.canDash_ = false;
        state_.dashTimer_ = 0.0f;
        
        // ダッシュ波紋を発生
        visuals_.SpawnDashRing(state_.position_, inputDir);
    }
}

void PlayerPhysics::ApplyGravity(PlayerState& state_, const PlayerParams& params_, float deltaTime) {
    if (state_.isDashing_) return; // ダッシュ中は重力を無視

    if (state_.isWallClinging_) {
        KeyboardInput* keyboard = KeyboardInput::GetInstance();
        float moveY = 0.0f;
        if (keyboard->IsKeyDown(DIK_W) || keyboard->IsKeyDown(DIK_UP)) moveY += 1.0f;
        if (keyboard->IsKeyDown(DIK_S) || keyboard->IsKeyDown(DIK_DOWN)) moveY -= 1.0f;
        state_.velocity_.y = moveY * params_.wallClimbSpeed_; // Wで上、Sで下へ移動
        return;
    }

    state_.velocity_.y += params_.gravity_ * deltaTime;

    // 最大落下速度を制限
    float currentMaxFallSpeed = params_.maxFallSpeed_;
    if (state_.isWallSliding_ && state_.velocity_.y < 0.0f) {
        currentMaxFallSpeed = params_.wallSlideSpeed_; // ずり落ち中はゆっくり落下
    }

    if (state_.velocity_.y < currentMaxFallSpeed) {
        state_.velocity_.y = currentMaxFallSpeed;
    }
}

void PlayerPhysics::ResolveCollisionY(PlayerState& state_, const PlayerParams& params_) {
    state_.isOnGround_ = false;
    state_.isOnMovingPlatform_ = false;
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
    if (state_.velocity_.y <= 0.0f) {
        // 下方向：足元チェック
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
                
                state_.position_.y = blockAABB.top + params_.halfHeight_;
                state_.velocity_.y = 0.0f;
                state_.isOnGround_ = true;
                state_.canDash_ = true;
                state_.wallJumpDirLockTimer_ = 0.0f;
                state_.lockedDirectionX_ = 0.0f;
                
                if (collider->GetUserData()) {
                    BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                    block->OnPlayerStand();
                }
                break;
            }
        }
    } else {
        // 上方向：頭上チェック
        queryAABB.top += 0.1f;
        auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
        for (auto* collider : colliders) {
            if (collider->IsMoving()) continue;
            if (!collider->IsSolid()) continue;

            AABB2D blockAABB = collider->GetAABB();
            if (aabb.right > blockAABB.left && aabb.left < blockAABB.right &&
                aabb.top > blockAABB.bottom && aabb.bottom < blockAABB.top) {
                state_.position_.y = blockAABB.bottom - params_.halfHeight_;
                state_.velocity_.y = 0.0f;
                break;
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
            if (state_.velocity_.y <= 0.0f && aabb.bottom >= blockAABB.top - 0.5f) { // 上から乗った
                state_.position_.y = blockAABB.top + params_.halfHeight_;
                state_.velocity_.y = 0.0f;
                state_.isOnGround_ = true;
                state_.canDash_ = true;
                state_.wallJumpDirLockTimer_ = 0.0f;
                state_.lockedDirectionX_ = 0.0f;
                if (collider->GetUserData()) {
                    BaseBlock* block = static_cast<BaseBlock*>(collider->GetUserData());
                    block->OnPlayerStand();
                }
                state_.isOnMovingPlatform_ = true;
                state_.platformVelocity_ = collider->GetVelocity();
            } else if (state_.velocity_.y > 0.0f && aabb.top <= blockAABB.bottom + 0.5f) { // 下からぶつかった
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
    
    if (state_.velocity_.x > 0.0f) {
        queryAABB.right += 0.1f;
        auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
        for (auto* collider : colliders) {
            if (collider->IsMoving() || !collider->IsSolid()) continue;
            
            AABB2D blockAABB = collider->GetAABB();
            if (aabb.right > blockAABB.left && aabb.left < blockAABB.right &&
                queryAABB.top > blockAABB.bottom && queryAABB.bottom < blockAABB.top) {
                state_.position_.x = blockAABB.left - params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.isTouchingWallRight_ = true;
                break;
            }
        }
    } else if (state_.velocity_.x < 0.0f) {
        queryAABB.left -= 0.1f;
        auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
        for (auto* collider : colliders) {
            if (collider->IsMoving() || !collider->IsSolid()) continue;
            
            AABB2D blockAABB = collider->GetAABB();
            if (aabb.right > blockAABB.left && aabb.left < blockAABB.right &&
                queryAABB.top > blockAABB.bottom && queryAABB.bottom < blockAABB.top) {
                state_.position_.x = blockAABB.right + params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.isTouchingWallLeft_ = true;
                break;
            }
        }
    }
}

void PlayerPhysics::ResolveDynamicCollisionX(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb) {
    state_.wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f }; // 毎フレームリセット
    
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
            float blockCenterX = (blockAABB.left + blockAABB.right) * 0.5f;
            if (state_.position_.x < blockCenterX) {
                state_.position_.x = blockAABB.left - params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.isTouchingWallRight_ = true;
                state_.wallPlatformVelocity_ = collider->GetVelocity();
            } else {
                state_.position_.x = blockAABB.right + params_.halfWidth_;
                state_.velocity_.x = 0.0f;
                state_.isTouchingWallLeft_ = true;
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

