#include "PlayerPhysics.h"
#include "Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/BaseBlock.h"
#include <algorithm>
#include <cmath>

void PlayerPhysics::Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime, Player2D* player, MapChip2D* mapChip) {
    if (state_.isDead_ || state_.isGoal_) return;

    // 1. 左右入力
    HandleMovement(state_, params_, input_, deltaTime, player);

    // 2. 重力
    ApplyGravity(state_, params_, deltaTime);

    // 3. X軸移動と壁押し戻し
    state_.position_.x += state_.velocity_.x * deltaTime;
    ResolveCollisionX(state_, params_, mapChip);

    // 4. Y軸移動と床/天井押し戻し
    state_.position_.y += state_.velocity_.y * deltaTime;
    ResolveCollisionY(state_, params_, mapChip);

    // 5. 走行中の足元砂ぼこりエフェクト
    if (state_.isOnGround_ && std::abs(state_.velocity_.x) > 0.1f && player) {
        state_.runDustTimer_ += deltaTime;
        if (state_.runDustTimer_ >= params_.runDustInterval_) {
            state_.runDustTimer_ = 0.0f;
            float dirX = (state_.velocity_.x > 0.0f) ? 1.0f : -1.0f;
            player->GetVisuals().SpawnRunDust({ state_.position_.x, state_.position_.y - params_.halfHeight_, 0.0f }, dirX);
        }
    } else {
        state_.runDustTimer_ = 0.0f;
    }

    // 6. 特殊ブロック判定（デス・ゴール等）
    CheckBlockInteractions(state_, params_, player, mapChip);
}

void PlayerPhysics::HandleMovement(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime, Player2D* player) {
    (void)deltaTime;
    // 左右移動
    state_.velocity_.x = input_.moveX * params_.moveSpeed_;

    // ジャンプ
    if (state_.isOnGround_ && input_.isJumpPressed) {
        state_.velocity_.y = params_.jumpPower_;
        state_.isOnGround_ = false;
        if (player) {
            player->GetVisuals().SpawnJumpDust({ state_.position_.x, state_.position_.y - params_.halfHeight_, 0.0f }, 0.0f);
        }
    }
}

void PlayerPhysics::ApplyGravity(PlayerState& state_, const PlayerParams& params_, float deltaTime) {
    if (!state_.isOnGround_) {
        state_.velocity_.y += params_.gravity_ * deltaTime;
        if (state_.velocity_.y < params_.maxFallSpeed_) {
            state_.velocity_.y = params_.maxFallSpeed_;
        }
    }
}

AABB2D PlayerPhysics::GetAABB(const PlayerState& state_, const PlayerParams& params_) const {
    return {
        state_.position_.x - params_.halfWidth_,
        state_.position_.y + params_.halfHeight_,
        state_.position_.x + params_.halfWidth_,
        state_.position_.y - params_.halfHeight_
    };
}

bool PlayerPhysics::CheckAABBCollision(const AABB2D& a, const AABB2D& b) const {
    return (a.right > b.left && a.left < b.right &&
            a.top > b.bottom && a.bottom < b.top);
}

void PlayerPhysics::ResolveCollisionX(PlayerState& state_, const PlayerParams& params_, MapChip2D* mapChip) {
    state_.isTouchingWallLeft_ = false;
    state_.isTouchingWallRight_ = false;
    if (!mapChip) return;

    // 床・天井の角との干渉を避けるため、Y方向の判定サイズを上下0.05f小さくする
    float minX = state_.position_.x - params_.halfWidth_;
    float maxX = state_.position_.x + params_.halfWidth_;
    float minY = state_.position_.y - params_.halfHeight_ + 0.05f;
    float maxY = state_.position_.y + params_.halfHeight_ - 0.05f;

    if (minY >= maxY) return;

    int startChipX = mapChip->WorldToChipX(minX);
    int endChipX   = mapChip->WorldToChipX(maxX);
    int startChipY = mapChip->WorldToChipY(minY);
    int endChipY   = mapChip->WorldToChipY(maxY);

    for (int cy = startChipY; cy <= endChipY; ++cy) {
        for (int cx = startChipX; cx <= endChipX; ++cx) {
            MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
            if (type == MapChip2D::ChipType::kBlock) {
                float blockLeft = mapChip->ChipToWorldX(cx);
                float blockRight = blockLeft + mapChip->GetChipSize();
                float blockBottom = mapChip->ChipToWorldY(cy);
                float blockTop = blockBottom + mapChip->GetChipSize();

                if (maxY > blockBottom && minY < blockTop) {
                    if (state_.velocity_.x > 0.0f && maxX > blockLeft && minX < blockLeft) {
                        state_.position_.x = blockLeft - params_.halfWidth_;
                        state_.velocity_.x = 0.0f;
                        state_.isTouchingWallRight_ = true;
                        minX = state_.position_.x - params_.halfWidth_;
                        maxX = state_.position_.x + params_.halfWidth_;
                    } else if (state_.velocity_.x < 0.0f && minX < blockRight && maxX > blockRight) {
                        state_.position_.x = blockRight + params_.halfWidth_;
                        state_.velocity_.x = 0.0f;
                        state_.isTouchingWallLeft_ = true;
                        minX = state_.position_.x - params_.halfWidth_;
                        maxX = state_.position_.x + params_.halfWidth_;
                    }
                }
            }
        }
    }
}

void PlayerPhysics::ResolveCollisionY(PlayerState& state_, const PlayerParams& params_, MapChip2D* mapChip) {
    if (!mapChip) return;

    float minX = state_.position_.x - params_.halfWidth_ + 0.02f;
    float maxX = state_.position_.x + params_.halfWidth_ - 0.02f;
    float minY = state_.position_.y - params_.halfHeight_;
    float maxY = state_.position_.y + params_.halfHeight_;

    int startChipX = mapChip->WorldToChipX(minX);
    int endChipX   = mapChip->WorldToChipX(maxX);
    int startChipY = mapChip->WorldToChipY(minY - 0.05f);
    int endChipY   = mapChip->WorldToChipY(maxY + 0.05f);

    bool groundedThisFrame = false;

    // 落下中または停止中の床判定
    if (state_.velocity_.y <= 0.0f) {
        for (int cy = startChipY; cy <= endChipY; ++cy) {
            for (int cx = startChipX; cx <= endChipX; ++cx) {
                MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
                if (type == MapChip2D::ChipType::kBlock) {
                    float blockBottom = mapChip->ChipToWorldY(cy);
                    float blockTop = blockBottom + mapChip->GetChipSize();

                    if (minY <= blockTop && (minY >= blockBottom - 0.1f || maxY > blockTop)) {
                        state_.position_.y = blockTop + params_.halfHeight_;
                        state_.velocity_.y = 0.0f;
                        groundedThisFrame = true;
                        minY = state_.position_.y - params_.halfHeight_;
                        maxY = state_.position_.y + params_.halfHeight_;
                    }
                } else if (type == MapChip2D::ChipType::kOneWayBlock) {
                    float blockTop = mapChip->ChipToWorldY(cy) + mapChip->GetChipSize();
                    if (minY <= blockTop && minY >= blockTop - 0.25f) {
                        state_.position_.y = blockTop + params_.halfHeight_;
                        state_.velocity_.y = 0.0f;
                        groundedThisFrame = true;
                        minY = state_.position_.y - params_.halfHeight_;
                        maxY = state_.position_.y + params_.halfHeight_;
                    }
                }
            }
        }
    }
    // 上昇中の天井判定
    else if (state_.velocity_.y > 0.0f) {
        for (int cy = startChipY; cy <= endChipY; ++cy) {
            for (int cx = startChipX; cx <= endChipX; ++cx) {
                MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
                if (type == MapChip2D::ChipType::kBlock) {
                    float blockBottom = mapChip->ChipToWorldY(cy);

                    if (maxY >= blockBottom && minY < blockBottom) {
                        state_.position_.y = blockBottom - params_.halfHeight_;
                        state_.velocity_.y = 0.0f;
                        minY = state_.position_.y - params_.halfHeight_;
                        maxY = state_.position_.y + params_.halfHeight_;
                    }
                }
            }
        }
    }

    state_.isOnGround_ = groundedThisFrame;
}

void PlayerPhysics::CheckBlockInteractions(PlayerState& state_, const PlayerParams& params_, Player2D* player, MapChip2D* mapChip) {
    if (!player) return;

    // 画面下落下死
    if (state_.position_.y < -10.0f) {
        player->Kill(true);
        return;
    }

    if (!mapChip) return;

    float minX = state_.position_.x - params_.halfWidth_;
    float maxX = state_.position_.x + params_.halfWidth_;
    float minY = state_.position_.y - params_.halfHeight_;
    float maxY = state_.position_.y + params_.halfHeight_;

    int startChipX = mapChip->WorldToChipX(minX);
    int endChipX   = mapChip->WorldToChipX(maxX);
    int startChipY = mapChip->WorldToChipY(minY);
    int endChipY   = mapChip->WorldToChipY(maxY);

    for (int cy = startChipY; cy <= endChipY; ++cy) {
        for (int cx = startChipX; cx <= endChipX; ++cx) {
            MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
            if (type == MapChip2D::ChipType::kDeathBlock) {
                player->Kill();
                return;
            } else if (type == MapChip2D::ChipType::kGoal) {
                player->ReachGoal();
                return;
            }
        }
    }
}
