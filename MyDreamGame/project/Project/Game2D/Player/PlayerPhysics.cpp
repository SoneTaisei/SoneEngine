#include "PlayerPhysics.h"
#include "Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/BaseBlock.h"
#include <algorithm>
#include <cmath>

void PlayerPhysics::Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime, Player2D* player, MapChip2D* mapChip) {
    if (state_.isDead_ || state_.isGoal_) return;

    // 1. 蟾ｦ蜿ｳ蜈･蜉・
    HandleMovement(state_, params_, input_, deltaTime, player);

    // 2. 驥榊鴨
    ApplyGravity(state_, params_, deltaTime);

    // 3. X霆ｸ遘ｻ蜍輔→螢∵款縺玲綾縺・
    state_.position_.x += (state_.velocity_.x + state_.platformVelocity_.x) * deltaTime;
    ResolveCollisionX(state_, params_, mapChip, player);
    if (state_.isDead_ || state_.isGoal_) return;

    // 4. Y霆ｸ遘ｻ蜍輔→蠎・螟ｩ莠墓款縺玲綾縺・
    state_.position_.y += state_.velocity_.y * deltaTime;
    ResolveCollisionY(state_, params_, mapChip, player);
    if (state_.isDead_ || state_.isGoal_) return;

    // 5. 襍ｰ陦御ｸｭ縺ｮ雜ｳ蜈・ゅ⊂縺薙ｊ繧ｨ繝輔ぉ繧ｯ繝・
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

    // 6. 迚ｹ谿翫ヶ繝ｭ繝・け蛻､螳夲ｼ医ョ繧ｹ繝ｻ繧ｴ繝ｼ繝ｫ遲会ｼ・
    CheckBlockInteractions(state_, params_, player, mapChip);
}

void PlayerPhysics::HandleMovement(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime, Player2D* player) {
    (void)deltaTime;
    // 蟾ｦ蜿ｳ遘ｻ蜍・
    state_.velocity_.x = input_.moveX * params_.moveSpeed_;

    // 繧ｸ繝｣繝ｳ繝・
    if (state_.isOnGround_ && input_.isJumpPressed) {
        // 骼悶・謨ｰ縺・蛟九ｒ雜・∴縺溷・縺縺代ず繝｣繝ｳ繝怜鴨繧剃ｽ惹ｸ九＆縺帙ｋ
        int extraChains = (std::max)(0, state_.chainLength_ - 3);
        float actualJumpPower = params_.jumpPower_ - (extraChains * params_.chainJumpPenalty_);
        if (actualJumpPower < 0.0f) actualJumpPower = 0.0f; // 譛菴弱〒繧・莉･荳・

        state_.velocity_.y = actualJumpPower;
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

void PlayerPhysics::ResolveCollisionX(PlayerState& state_, const PlayerParams& params_, MapChip2D* mapChip, Player2D* player) {
    state_.isTouchingWallLeft_ = false;
    state_.isTouchingWallRight_ = false;
    if (!mapChip) return;

    // 蠎翫・螟ｩ莠輔・隗偵→縺ｮ蟷ｲ貂峨ｒ驕ｿ縺代ｋ縺溘ａ縲〆譁ｹ蜷代・蛻､螳壹し繧､繧ｺ繧剃ｸ贋ｸ・.05f蟆上＆縺上☆繧・
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
            auto* block = mapChip->GetBlock(cx, cy);
            if (block && block->IsSolid() && !block->IsDestroyed() && !block->IsMoving()) {
                float blockLeft = mapChip->ChipToWorldX(cx);
                float blockRight = blockLeft + mapChip->GetChipSize();
                float blockBottom = mapChip->ChipToWorldY(cy);
                float blockTop = blockBottom + mapChip->GetChipSize();

                // Y譁ｹ蜷代・驥崎､・メ繧ｧ繝・け
                if (maxY <= blockBottom || minY >= blockTop) {
                    continue;
                }

                if (state_.velocity_.x > 0.0f && maxX > blockLeft && minX < blockLeft) {
                    state_.position_.x = blockLeft - params_.halfWidth_;
                    state_.velocity_.x = 0.0f;
                    state_.isTouchingWallRight_ = true;
                    minX = state_.position_.x - params_.halfWidth_;
                    maxX = state_.position_.x + params_.halfWidth_;
                    if (player) {
                        block->OnCollision(player);
                        block->OnPlayerTouch(player);
                    }
                    if (state_.isDead_) return;
                } else if (state_.velocity_.x < 0.0f && minX < blockRight && maxX > blockRight) {
                    state_.position_.x = blockRight + params_.halfWidth_;
                    state_.velocity_.x = 0.0f;
                    state_.isTouchingWallLeft_ = true;
                    minX = state_.position_.x - params_.halfWidth_;
                    maxX = state_.position_.x + params_.halfWidth_;
                    if (player) {
                        block->OnCollision(player);
                        block->OnPlayerTouch(player);
                    }
                    if (state_.isDead_) return;
                }
            }
        }
    }

    // 蜍輔￥蠎翫・迚ｹ蛻･蛻､螳・(繧ｰ繝ｪ繝・ラ縺ｧ縺ｯ縺ｪ縺丞ｮ滄圀縺ｮ繝ｯ繝ｼ繝ｫ繝牙ｺｧ讓・AABB)繝吶・繧ｹ縺ｧ蛻､螳・
    for (const auto& blockPtr : mapChip->GetUpdateBlocks()) {
        if (!blockPtr || blockPtr->IsDestroyed() || !blockPtr->IsMoving() || !blockPtr->IsSolid()) continue;
        
        AABB2D blockAABB = blockPtr->GetAABB();
        float blockLeft = blockAABB.left;
        float blockRight = blockAABB.right;
        float blockTop = blockAABB.top;
        float blockBottom = blockAABB.bottom;
        
        // Y譁ｹ蜷代・驥崎､・メ繧ｧ繝・け
        if (maxY <= blockBottom || minY >= blockTop) {
            continue;
        }

        if (state_.velocity_.x > 0.0f && maxX > blockLeft && minX < blockLeft) {
            state_.position_.x = blockLeft - params_.halfWidth_;
            state_.velocity_.x = 0.0f;
            state_.isTouchingWallRight_ = true;
            minX = state_.position_.x - params_.halfWidth_;
            maxX = state_.position_.x + params_.halfWidth_;
            if (player) {
                blockPtr->OnCollision(player);
                blockPtr->OnPlayerTouch(player);
            }
            if (state_.isDead_) return;
        } else if (state_.velocity_.x < 0.0f && minX < blockRight && maxX > blockRight) {
            state_.position_.x = blockRight + params_.halfWidth_;
            state_.velocity_.x = 0.0f;
            state_.isTouchingWallLeft_ = true;
            minX = state_.position_.x - params_.halfWidth_;
            maxX = state_.position_.x + params_.halfWidth_;
            if (player) {
                blockPtr->OnCollision(player);
                blockPtr->OnPlayerTouch(player);
            }
            if (state_.isDead_) return;
        }
    }
}

void PlayerPhysics::ResolveCollisionY(PlayerState& state_, const PlayerParams& params_, MapChip2D* mapChip, Player2D* player) {
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

    // 關ｽ荳倶ｸｭ縺ｾ縺溘・蛛懈ｭ｢荳ｭ縺ｮ蠎雁愛螳・
    if (state_.velocity_.y <= 0.0f) {
        for (int cy = startChipY; cy <= endChipY; ++cy) {
            for (int cx = startChipX; cx <= endChipX; ++cx) {
                auto* block = mapChip->GetBlock(cx, cy);
                if (block && !block->IsDestroyed() && !block->IsMoving()) {
                    float blockLeft = mapChip->ChipToWorldX(cx);
                    float blockRight = blockLeft + mapChip->GetChipSize();
                    float blockBottom = mapChip->ChipToWorldY(cy);
                    float blockTop = blockBottom + mapChip->GetChipSize();

                    // X譁ｹ蜷代・驥崎､・メ繧ｧ繝・け
                    if (maxX <= blockLeft || minX >= blockRight) {
                        continue;
                    }

                    if (block->IsSolid()) {
                        if (minY <= blockTop && (minY >= blockBottom - 0.1f || maxY > blockTop)) {
                            state_.position_.y = blockTop + params_.halfHeight_;
                            state_.velocity_.y = 0.0f;
                            groundedThisFrame = true;
                            state_.platformVelocity_ = block->GetVelocity();
                            minY = state_.position_.y - params_.halfHeight_;
                            maxY = state_.position_.y + params_.halfHeight_;
                            if (player) {
                                block->OnCollision(player);
                                block->OnPlayerStand(player);
                            }
                            if (state_.isDead_) return;
                        }
                    } else if (block->IsOneWay()) {
                        if (minY <= blockTop && minY >= blockTop - 0.25f) {
                            state_.position_.y = blockTop + params_.halfHeight_;
                            state_.velocity_.y = 0.0f;
                            groundedThisFrame = true;
                            state_.platformVelocity_ = block->GetVelocity();
                            minY = state_.position_.y - params_.halfHeight_;
                            maxY = state_.position_.y + params_.halfHeight_;
                            if (player) {
                                block->OnCollision(player);
                                block->OnPlayerStand(player);
                            }
                            if (state_.isDead_) return;
                        }
                    }
                }
            }
        }
        
        // 蜍輔￥蠎翫・迚ｹ蛻･蛻､螳・(繧ｰ繝ｪ繝・ラ縺ｧ縺ｯ縺ｪ縺丞ｮ滄圀縺ｮ繝ｯ繝ｼ繝ｫ繝牙ｺｧ讓・AABB)繝吶・繧ｹ縺ｧ蛻､螳・
        for (const auto& blockPtr : mapChip->GetUpdateBlocks()) {
            if (!blockPtr || blockPtr->IsDestroyed() || !blockPtr->IsMoving()) continue;
            
            AABB2D blockAABB = blockPtr->GetAABB();
            float blockLeft = blockAABB.left;
            float blockRight = blockAABB.right;
            float blockTop = blockAABB.top;
            float blockBottom = blockAABB.bottom;
            
            // X譁ｹ蜷代・驥崎､・メ繧ｧ繝・け
            if (maxX <= blockLeft || minX >= blockRight) {
                continue;
            }

            if (blockPtr->IsSolid()) {
                if (minY <= blockTop && (minY >= blockBottom - 0.1f || maxY > blockTop)) {
                    state_.position_.y = blockTop + params_.halfHeight_;
                    state_.velocity_.y = 0.0f;
                    groundedThisFrame = true;
                    state_.platformVelocity_ = blockPtr->GetVelocity();
                    minY = state_.position_.y - params_.halfHeight_;
                    maxY = state_.position_.y + params_.halfHeight_;
                    if (player) {
                        blockPtr->OnCollision(player);
                        blockPtr->OnPlayerStand(player);
                    }
                    if (state_.isDead_) return;
                }
            } else if (blockPtr->IsOneWay()) {
                if (minY <= blockTop && minY >= blockTop - 0.25f) {
                    state_.position_.y = blockTop + params_.halfHeight_;
                    state_.velocity_.y = 0.0f;
                    groundedThisFrame = true;
                    state_.platformVelocity_ = blockPtr->GetVelocity();
                    minY = state_.position_.y - params_.halfHeight_;
                    maxY = state_.position_.y + params_.halfHeight_;
                    if (player) {
                        blockPtr->OnCollision(player);
                        blockPtr->OnPlayerStand(player);
                    }
                    if (state_.isDead_) return;
                }
            }
        }
    }
    // 荳頑・荳ｭ縺ｮ螟ｩ莠募愛螳・
    else if (state_.velocity_.y > 0.0f) {
        for (int cy = startChipY; cy <= endChipY; ++cy) {
            for (int cx = startChipX; cx <= endChipX; ++cx) {
                auto* block = mapChip->GetBlock(cx, cy);
                if (block && block->IsSolid() && !block->IsDestroyed() && !block->IsMoving()) {
                    float blockLeft = mapChip->ChipToWorldX(cx);
                    float blockRight = blockLeft + mapChip->GetChipSize();
                    float blockBottom = mapChip->ChipToWorldY(cy);
                    float blockTop = blockBottom + mapChip->GetChipSize();

                    // X譁ｹ蜷代・驥阪↑繧翫メ繧ｧ繝・け
                    if (maxX <= blockLeft || minX >= blockRight) {
                        continue;
                    }

                    if (maxY >= blockBottom && minY < blockBottom) {
                        state_.position_.y = blockBottom - params_.halfHeight_;
                        state_.velocity_.y = 0.0f;
                        minY = state_.position_.y - params_.halfHeight_;
                        maxY = state_.position_.y + params_.halfHeight_;
                        if (player) {
                            block->OnCollision(player);
                            block->OnPlayerTouch(player);
                        }
                        if (state_.isDead_) return;
                    }
                }
            }
        }

        // 蜍輔￥蠎翫・迚ｹ蛻･蛻､螳・(繧ｰ繝ｪ繝・ラ縺ｧ縺ｯ縺ｪ縺丞ｮ滄圀縺ｮ繝ｯ繝ｼ繝ｫ繝牙ｺｧ讓・AABB)繝吶・繧ｹ縺ｧ蛻､螳・
        for (const auto& blockPtr : mapChip->GetUpdateBlocks()) {
            if (!blockPtr || blockPtr->IsDestroyed() || !blockPtr->IsMoving() || !blockPtr->IsSolid()) continue;
            
            AABB2D blockAABB = blockPtr->GetAABB();
            float blockLeft = blockAABB.left;
            float blockRight = blockAABB.right;
            float blockTop = blockAABB.top;
            float blockBottom = blockAABB.bottom;
            
            // X譁ｹ蜷代・驥阪↑繧翫メ繧ｧ繝・け
            if (maxX <= blockLeft || minX >= blockRight) {
                continue;
            }

            if (maxY >= blockBottom && minY < blockBottom) {
                state_.position_.y = blockBottom - params_.halfHeight_;
                state_.velocity_.y = 0.0f;
                minY = state_.position_.y - params_.halfHeight_;
                maxY = state_.position_.y + params_.halfHeight_;
                if (player) {
                    blockPtr->OnCollision(player);
                    blockPtr->OnPlayerTouch(player);
                }
                if (state_.isDead_) return;
            }
        }
    }

    state_.isOnGround_ = groundedThisFrame;
    if (!groundedThisFrame) {
        state_.platformVelocity_ = { 0.0f, 0.0f, 0.0f };
    }
}

void PlayerPhysics::CheckBlockInteractions(PlayerState& state_, const PlayerParams& params_, Player2D* player, MapChip2D* mapChip) {
    if (!player || state_.isDead_) return;

    // 逕ｻ髱｢荳玖誠荳区ｭｻ
    if (state_.position_.y < -10.0f) {
        player->Kill(true);
        return;
    }

    if (!mapChip) return;

    // 謚ｼ縺玲綾縺怜ｾ後・蠅・阜邱壻ｸ奇ｼ亥ｺ翫ｄ螢√↓謗･縺励※縺・ｋ迥ｶ諷具ｼ峨〒繧ら｢ｺ螳溘↓蛻､螳壹〒縺阪ｋ繧医≧縺ｫ蛻､螳夐伜沺縺ｫ繝槭・繧ｸ繝ｳ繧呈戟縺溘○繧・
    float margin = 0.05f;
    float pLeft = state_.position_.x - params_.halfWidth_ - margin;
    float pRight = state_.position_.x + params_.halfWidth_ + margin;
    float pBottom = state_.position_.y - params_.halfHeight_ - margin;
    float pTop = state_.position_.y + params_.halfHeight_ + margin;

    int startChipX = mapChip->WorldToChipX(pLeft);
    int endChipX   = mapChip->WorldToChipX(pRight);
    int startChipY = mapChip->WorldToChipY(pBottom);
    int endChipY   = mapChip->WorldToChipY(pTop);

    for (int cy = startChipY; cy <= endChipY; ++cy) {
        for (int cx = startChipX; cx <= endChipX; ++cx) {
            float blockLeft = mapChip->ChipToWorldX(cx);
            float blockRight = blockLeft + mapChip->GetChipSize();
            float blockBottom = mapChip->ChipToWorldY(cy);
            float blockTop = blockBottom + mapChip->GetChipSize();

            // AABB驥崎､・愛螳・
            if (pRight <= blockLeft || pLeft >= blockRight ||
                pTop <= blockBottom || pBottom >= blockTop) {
                continue;
            }

            MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
            if (type == MapChip2D::ChipType::kDeathBlock) {
                player->Kill();
                return;
            } else if (type == MapChip2D::ChipType::kGoal) {
                player->ReachGoal();
                return;
            }

            if (auto* block = mapChip->GetBlock(cx, cy)) {
                if (block->IsMoving()) continue; // 動く床は別ループで処理
                block->OnCollision(player);
                if (state_.isDead_ || state_.isGoal_) {
                    return;
                }
            }
        }
    }

    // 動く床の特別判定 (実際のAABBベース)
    for (const auto& blockPtr : mapChip->GetUpdateBlocks()) {
        if (!blockPtr || blockPtr->IsDestroyed() || !blockPtr->IsMoving()) continue;
        
        AABB2D blockAABB = blockPtr->GetAABB();
        if (pRight <= blockAABB.left || pLeft >= blockAABB.right ||
            pTop <= blockAABB.bottom || pBottom >= blockAABB.top) {
            continue;
        }

        blockPtr->OnCollision(player);
        if (state_.isDead_ || state_.isGoal_) {
            return;
        }
    }
}

