#include "PlayerCollisionSolver.h"
#include "../Player2D.h"
#include "Game2D/Blocks/BaseBlock.h"
#include "Collision/CollisionManager.h"
#include <algorithm>
#include <cmath>
#include <vector>

std::array<PlayerCollisionSolver::HitPart, 2> PlayerCollisionSolver::BuildHitParts(const PlayerState& state, const PlayerParams& params, bool usePreviousWallTouch) const {
    const bool isWallPose = state.isWallClinging_ || state.isWallSliding_;

    HitPart body;
    body.offsetX = 0.0f;
    body.offsetY = params.bodyOffsetY_;
    body.halfWidth = params.bodyHalfWidth_;
    body.halfHeight = params.bodyHalfHeight_;
    if (isWallPose && params.bodyWallBottomShrink_ > 0.0f) {
        float origTop = params.bodyOffsetY_ + params.bodyHalfHeight_;
        float newHalfH = (std::max)(0.05f, params.bodyHalfHeight_ - params.bodyWallBottomShrink_ * 0.5f);
        body.halfHeight = newHalfH;
        body.offsetY = origTop - newHalfH;
    }
    body.allowWallTouch = true;
    body.allowOneWayStand = true;

    HitPart head;
    head.offsetX = params.headOffsetX_ * state.facingDirectionX_;
    if (isWallPose) {
        bool touchRight = usePreviousWallTouch ? state.wasTouchingWallRight_ : (state.isTouchingWallRight_ || state.wasTouchingWallRight_);
        bool touchLeft = usePreviousWallTouch ? state.wasTouchingWallLeft_ : (state.isTouchingWallLeft_ || state.wasTouchingWallLeft_);
        if (touchRight) {
            head.offsetX -= params.headWallOffsetX_;
        } else if (touchLeft) {
            head.offsetX += params.headWallOffsetX_;
        }
    }
    head.offsetY = params.headOffsetY_;
    head.halfWidth = params.headHalfWidth_;
    head.halfHeight = params.headHalfHeight_;
    head.allowWallTouch = false;
    head.allowOneWayStand = false;

    return { body, head };
}

AABB2D PlayerCollisionSolver::MakePartAABB(const Vector3& position, const HitPart& part) {
    float cx = position.x + part.offsetX;
    float cy = position.y + part.offsetY;
    return {
        cx - part.halfWidth,
        cy + part.halfHeight,
        cx + part.halfWidth,
        cy - part.halfHeight
    };
}

void PlayerCollisionSolver::ApplyLanding(PlayerState& state, const PlayerParams& params, const HitPart& part, const AABB2D& blockAABB, ColliderComponent* collider) {
    state.position_.y = blockAABB.top + part.halfHeight - part.offsetY;
    state.velocity_.y = 0.0f;
    state.isOnGround_ = true;
    state.isJumping_ = false;
    state.minJumpTimer_ = 0.0f;
    state.canDash_ = true;
    state.wallJumpDirLockTimer_ = 0.0f;
    state.lockedDirectionX_ = 0.0f;

    if (state.climbingUpTimer_ > 0.0f) {
        state.climbLandingTimer_ = params.climbLandingDuration_;
        state.climbingUpTimer_ = 0.0f;
        state.velocity_.x = 0.0f;
        state.externalVelocityX_ = 0.0f;
        state.externalVelocityTimer_ = 0.0f;
    }

    if (collider && collider->GetUserData()) {
        BaseMapObject* block = static_cast<BaseMapObject*>(collider->GetUserData());
        block->OnPlayerStand();
    }
}

void PlayerCollisionSolver::ApplyCeilingHit(PlayerState& state, const HitPart& part, const AABB2D& blockAABB) {
    state.position_.y = blockAABB.bottom - part.halfHeight - part.offsetY;
    state.velocity_.y = 0.0f;
    state.isJumping_ = false;
    state.minJumpTimer_ = 0.0f;
}

void PlayerCollisionSolver::ResetHorizontalOnWallHit(PlayerState& state, bool clearDashInertia) {
    state.velocity_.x = 0.0f;
    state.externalVelocityX_ = 0.0f;
    state.externalVelocityTimer_ = 0.0f;
    if (clearDashInertia) {
        state.hasDashInertia_ = false;
        state.dashJumpWindowTimer_ = 0.0f;
        state.lastDashDirX_ = 0.0f;
    }
}

void PlayerCollisionSolver::ResolveCollisionY(PlayerState& state, const PlayerParams& params, float prevPosY, float moveDeltaY) {
    state.isOnGround_ = false;
    state.isOnMovingPlatform_ = false;
    state.standingPlatformCollider_ = nullptr;
    state.platformVelocity_ = { 0.0f, 0.0f, 0.0f };

    const bool isWallPose = state.isWallClinging_ || state.isWallSliding_;
    float bodyPrevBottomOffset = 0.0f;
    if (!isWallPose && state.wasWallPose_ && params.bodyWallBottomShrink_ > 0.0f) {
        float shrunkHalfH = (std::max)(0.05f, params.bodyHalfHeight_ - params.bodyWallBottomShrink_ * 0.5f);
        bodyPrevBottomOffset = (params.bodyHalfHeight_ - shrunkHalfH) * 2.0f;
    }

    auto parts = BuildHitParts(state, params);

    // 1. まず胴体パーツで床・天井を判定
    ResolveStaticCollisionY(state, params, parts[0], prevPosY, moveDeltaY, bodyPrevBottomOffset);
    ResolveDynamicCollisionY(state, params, parts[0], prevPosY, moveDeltaY, bodyPrevBottomOffset);

    // 壁掴み中に床へ着地した場合は立ち姿勢の足元高さに戻してから頭判定を行う
    if (isWallPose && state.isOnGround_ && params.bodyWallBottomShrink_ > 0.0f) {
        float actualShrink = (params.bodyHalfHeight_ - parts[0].halfHeight) * 2.0f;
        state.position_.y += actualShrink;
        state.isWallClinging_ = false;
        state.isWallSliding_ = false;
        parts = BuildHitParts(state, params);
    }

    // 2. 頭パーツで天井・床を判定
    ResolveStaticCollisionY(state, params, parts[1], prevPosY, moveDeltaY, 0.0f);
    ResolveDynamicCollisionY(state, params, parts[1], prevPosY, moveDeltaY, 0.0f);
}

void PlayerCollisionSolver::ResolveStaticCollisionY(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset) {
    const float kSkinX = (std::min)(0.02f, part.halfWidth * 0.5f);
    AABB2D aabb = MakePartAABB(state.position_, part);
    float partPosY = state.position_.y + part.offsetY;
    float partPrevPosY = prevPosY + part.offsetY;

    AABB2D queryAABB;
    queryAABB.left = aabb.left + kSkinX;
    queryAABB.right = aabb.right - kSkinX;
    queryAABB.top = (std::max)(partPrevPosY, partPosY) + part.halfHeight + 0.1f;
    queryAABB.bottom = (std::min)(partPrevPosY, partPosY) - part.halfHeight - 0.1f;

    float prevBottom = partPrevPosY - part.halfHeight + prevBottomOffset;
    float prevTop = partPrevPosY + part.halfHeight;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (collider->IsMoving()) continue;
        if (!collider->IsSolid() && !collider->IsOneWay()) continue;

        AABB2D blockAABB = collider->GetAABB();

        if (aabb.right - kSkinX <= blockAABB.left || aabb.left + kSkinX >= blockAABB.right) {
            continue;
        }

        if (collider->IsOneWay()) {
            if (!part.allowOneWayStand || prevBottom < blockAABB.top - 0.08f || moveDeltaY > 0.0001f) {
                continue;
            }
        }

        bool wasAbove = (prevBottom >= blockAABB.top - 0.08f);
        bool wasBelow = (prevTop <= blockAABB.bottom + 0.08f);

        // 1. 移動前にブロックの上面より上にいた場合
        if (wasAbove && moveDeltaY <= 0.0001f && aabb.bottom <= blockAABB.top && prevBottom >= blockAABB.bottom) {
            ApplyLanding(state, params, part, blockAABB, collider);
            aabb = MakePartAABB(state.position_, part);
            continue;
        }

        // 2. 移動前にブロックの下面より下にいた場合
        if (wasBelow && moveDeltaY >= -0.0001f && aabb.top >= blockAABB.bottom && prevTop <= blockAABB.top) {
            if (collider->IsOneWay()) continue;
            ApplyCeilingHit(state, part, blockAABB);
            aabb = MakePartAABB(state.position_, part);
            continue;
        }

        // 3. 移動前から既にY範囲が重なっていた場合
        if (aabb.Intersects(blockAABB)) {
            float overlapX = (std::min)(aabb.right, blockAABB.right) - (std::max)(aabb.left, blockAABB.left);
            float overlapY = (std::min)(aabb.top, blockAABB.top) - (std::max)(aabb.bottom, blockAABB.bottom);
            if (overlapY > overlapX) continue;

            float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
            partPosY = state.position_.y + part.offsetY;
            if (partPosY > blockCenterY) {
                ApplyLanding(state, params, part, blockAABB, collider);
            } else {
                if (collider->IsOneWay()) continue;
                ApplyCeilingHit(state, part, blockAABB);
            }
            aabb = MakePartAABB(state.position_, part);
        }
    }
}

void PlayerCollisionSolver::ResolveDynamicCollisionY(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset) {
    (void)moveDeltaY;
    AABB2D aabb = MakePartAABB(state.position_, part);
    AABB2D shrunkAABBY = aabb;
    const float skinX = (std::min)(0.02f, part.halfWidth * 0.5f);
    shrunkAABBY.left += skinX;
    shrunkAABBY.right -= skinX;

    if (state.isWallClinging_ || state.isWallSliding_) {
        if (state.isTouchingWallRight_) shrunkAABBY.right -= 0.15f;
        if (state.isTouchingWallLeft_) shrunkAABBY.left += 0.15f;
    }

    float partPrevPosY = prevPosY + part.offsetY;
    float prevBottom = partPrevPosY - part.halfHeight + prevBottomOffset;
    float prevTop = partPrevPosY + part.halfHeight;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(shrunkAABBY, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider->IsSolid() || !collider->IsMoving()) continue;

        AABB2D blockAABB = collider->GetAABB();
        if (!shrunkAABBY.IntersectsInclusive(blockAABB)) continue;

        bool wasAbove = (prevBottom >= blockAABB.top - 0.15f);
        bool wasBelow = (prevTop <= blockAABB.bottom + 0.15f);

        if (!wasAbove && !wasBelow) {
            float overlapX = (std::min)(shrunkAABBY.right, blockAABB.right) - (std::max)(shrunkAABBY.left, blockAABB.left);
            float overlapY = (std::min)(shrunkAABBY.top, blockAABB.top) - (std::max)(shrunkAABBY.bottom, blockAABB.bottom);
            if (overlapY > overlapX) continue;
        }

        float blockCenterY = (blockAABB.top + blockAABB.bottom) * 0.5f;
        float partPosY = state.position_.y + part.offsetY;
        if (wasAbove || (!wasBelow && partPosY > blockCenterY)) {
            ApplyLanding(state, params, part, blockAABB, collider);
            state.isOnMovingPlatform_ = true;
            state.standingPlatformCollider_ = collider;
            state.platformVelocity_ = collider->GetVelocity();
        } else {
            ApplyCeilingHit(state, part, blockAABB);
        }
        aabb = MakePartAABB(state.position_, part);
        shrunkAABBY.top = aabb.top;
        shrunkAABBY.bottom = aabb.bottom;
    }
}

void PlayerCollisionSolver::ResolveCollisionX(PlayerState& state, const PlayerParams& params, float prevPosX, float moveDeltaX, float deltaTime) {
    state.wasTouchingWallLeft_ = state.isTouchingWallLeft_;
    state.wasTouchingWallRight_ = state.isTouchingWallRight_;
    auto parts = BuildHitParts(state, params, true);

    state.isTouchingWallLeft_ = false;
    state.isTouchingWallRight_ = false;
    state.wallPlatformVelocity_ = { 0.0f, 0.0f, 0.0f };
    state.wallPlatformCollider_ = nullptr;

    for (const auto& part : parts) {
        ResolveStaticCollisionX(state, params, part, prevPosX, moveDeltaX);
    }
    for (const auto& part : parts) {
        ResolveDynamicCollisionX(state, params, part, prevPosX, moveDeltaX, deltaTime);
    }
}

void PlayerCollisionSolver::ResolveStaticCollisionX(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosX, float moveDeltaX) {
    (void)params;
    float partCenterX = state.position_.x + part.offsetX;
    float partCenterY = state.position_.y + part.offsetY;
    float partPrevPosX = prevPosX + part.offsetX;

    float minX = (std::min)(partPrevPosX, partCenterX) - part.halfWidth - 0.05f;
    float maxX = (std::max)(partPrevPosX, partCenterX) + part.halfWidth + 0.05f;

    const float skinY = (std::min)(0.05f, part.halfHeight * 0.5f);
    AABB2D queryAABB;
    queryAABB.left = minX;
    queryAABB.right = maxX;
    queryAABB.top = partCenterY + part.halfHeight - skinY;
    queryAABB.bottom = partCenterY - part.halfHeight + skinY;

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);

    std::vector<AABB2D> hitBlocks;
    hitBlocks.reserve(colliders.size());
    for (auto* collider : colliders) {
        if (!collider || collider->IsMoving() || !collider->IsSolid()) continue;
        AABB2D blockAABB = collider->GetAABB();
        if (queryAABB.top > blockAABB.bottom && queryAABB.bottom < blockAABB.top) {
            hitBlocks.push_back(blockAABB);
        }
    }

    if (hitBlocks.empty()) return;

    // 1. 右向きへの移動
    if (moveDeltaX > 0.0001f || state.velocity_.x > 0.0001f) {
        float earliestPartX = partCenterX;
        bool collided = false;
        for (const auto& blockAABB : hitBlocks) {
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
            state.position_.x = earliestPartX - part.offsetX;
            ResetHorizontalOnWallHit(state, true);
            if (part.allowWallTouch) {
                state.isTouchingWallRight_ = true;
            }
        }
    }
    // 2. 左向きへの移動
    else if (moveDeltaX < -0.0001f || state.velocity_.x < -0.0001f) {
        float earliestPartX = partCenterX;
        bool collided = false;
        for (const auto& blockAABB : hitBlocks) {
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
            state.position_.x = earliestPartX - part.offsetX;
            ResetHorizontalOnWallHit(state, true);
            if (part.allowWallTouch) {
                state.isTouchingWallLeft_ = true;
            }
        }
    }
    // 3. 残りのめり込み解決
    {
        AABB2D currentAABB = MakePartAABB(state.position_, part);
        for (const auto& blockAABB : hitBlocks) {
            if (currentAABB.right > blockAABB.left && currentAABB.left < blockAABB.right) {
                float overlapLeft = currentAABB.right - blockAABB.left;
                float overlapRight = blockAABB.right - currentAABB.left;
                if (overlapLeft < overlapRight) {
                    state.position_.x = blockAABB.left - part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state.isTouchingWallRight_ = true;
                    }
                } else {
                    state.position_.x = blockAABB.right + part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state.isTouchingWallLeft_ = true;
                    }
                }
                ResetHorizontalOnWallHit(state, true);
                currentAABB = MakePartAABB(state.position_, part);
            }
        }
    }

    // 壁接触フラグの補正
    if (part.allowWallTouch) {
        const float kContactMargin = 0.02f;
        AABB2D finalAABB = MakePartAABB(state.position_, part);
        for (const auto& blockAABB : hitBlocks) {
            if (std::abs(finalAABB.right - blockAABB.left) <= kContactMargin) {
                state.isTouchingWallRight_ = true;
            }
            if (std::abs(finalAABB.left - blockAABB.right) <= kContactMargin) {
                state.isTouchingWallLeft_ = true;
            }
        }
    }
}

void PlayerCollisionSolver::ResolveDynamicCollisionX(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosX, float moveDeltaX, float deltaTime) {
    (void)params;
    float partCenterX = state.position_.x + part.offsetX;
    float partCenterY = state.position_.y + part.offsetY;
    float partPrevPosX = prevPosX + part.offsetX;

    float minX = (std::min)(partPrevPosX, partCenterX) - part.halfWidth - 0.05f;
    float maxX = (std::max)(partPrevPosX, partCenterX) + part.halfWidth + 0.05f;

    const float skinY = (std::min)(0.05f, part.halfHeight * 0.5f);
    AABB2D queryAABB;
    queryAABB.left = minX;
    queryAABB.right = maxX;
    queryAABB.top = partCenterY + part.halfHeight - skinY;
    queryAABB.bottom = partCenterY - part.halfHeight + skinY;

    if (state.isWallClinging_ || state.isWallSliding_) {
        if (state.isTouchingWallRight_ || state.wasTouchingWallRight_) queryAABB.right += 0.2f;
        if (state.isTouchingWallLeft_ || state.wasTouchingWallLeft_) queryAABB.left -= 0.2f;
    }

    auto colliders = CollisionManager::GetInstance()->GetCollidersInAABB(queryAABB, kLayerBlock);
    for (auto* collider : colliders) {
        if (!collider || !collider->IsSolid() || !collider->IsMoving()) continue;

        AABB2D blockAABB = collider->GetAABB();
        if (queryAABB.top <= blockAABB.bottom || queryAABB.bottom >= blockAABB.top) continue;

        Vector3 platVel = collider->GetVelocity();
        float relMoveDeltaX = moveDeltaX - platVel.x * deltaTime;

        AABB2D currentAABB = MakePartAABB(state.position_, part);
        partCenterX = state.position_.x + part.offsetX;
        if (currentAABB.IntersectsInclusive(blockAABB) ||
            (partCenterX + part.halfWidth > blockAABB.left && partCenterX - part.halfWidth < blockAABB.right)) {

            if ((relMoveDeltaX > 0.0001f && partPrevPosX + part.halfWidth <= blockAABB.left + 0.05f) || (partPrevPosX + part.halfWidth <= blockAABB.left)) {
                state.position_.x = blockAABB.left - part.halfWidth - part.offsetX;
                ResetHorizontalOnWallHit(state, true);
                if (part.allowWallTouch) {
                    state.isTouchingWallRight_ = true;
                    state.wallPlatformCollider_ = collider;
                    state.wallPlatformVelocity_ = platVel;
                }
            } else if ((relMoveDeltaX < -0.0001f && partPrevPosX - part.halfWidth >= blockAABB.right - 0.05f) || (partPrevPosX - part.halfWidth >= blockAABB.right)) {
                state.position_.x = blockAABB.right + part.halfWidth - part.offsetX;
                ResetHorizontalOnWallHit(state, true);
                if (part.allowWallTouch) {
                    state.isTouchingWallLeft_ = true;
                    state.wallPlatformCollider_ = collider;
                    state.wallPlatformVelocity_ = platVel;
                }
            } else {
                float overlapLeft = currentAABB.right - blockAABB.left;
                float overlapRight = blockAABB.right - currentAABB.left;
                if (overlapLeft < overlapRight) {
                    state.position_.x = blockAABB.left - part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state.isTouchingWallRight_ = true;
                    }
                } else {
                    state.position_.x = blockAABB.right + part.halfWidth - part.offsetX;
                    if (part.allowWallTouch) {
                        state.isTouchingWallLeft_ = true;
                    }
                }
                ResetHorizontalOnWallHit(state, false);
                if (part.allowWallTouch) {
                    state.wallPlatformCollider_ = collider;
                    state.wallPlatformVelocity_ = platVel;
                }
            }

            if (part.allowWallTouch && collider->GetUserData()) {
                BaseMapObject* block = static_cast<BaseMapObject*>(collider->GetUserData());
                block->OnPlayerTouch();
            }
        }
    }
}

void PlayerCollisionSolver::SimulateCollisions(PlayerState& state, const PlayerParams& params, Player2D* player) {
    AABB2D bodyAABB = GetBodyAABB(state, params);
    AABB2D headAABB = GetHeadAABB(state, params);

    const float normalMargin = 0.02f;
    AABB2D queryAABB = {
        (std::min)(bodyAABB.left, headAABB.left) - normalMargin,
        (std::max)(bodyAABB.top, headAABB.top) + normalMargin,
        (std::max)(bodyAABB.right, headAABB.right) + normalMargin,
        (std::min)(bodyAABB.bottom, headAABB.bottom) - normalMargin
    };

    auto checkPartHit = [](const AABB2D& partAABB, const AABB2D& blockAABB, float margin, bool ignoreCornerGraze) -> bool {
        AABB2D expandedPart = {
            partAABB.left - margin,
            partAABB.top + margin,
            partAABB.right + margin,
            partAABB.bottom - margin
        };
        if (expandedPart.Intersects(blockAABB)) {
            if (ignoreCornerGraze) {
                float overlapX = (std::min)(expandedPart.right, blockAABB.right) - (std::max)(expandedPart.left, blockAABB.left);
                float overlapY = (std::min)(expandedPart.top, blockAABB.top) - (std::max)(expandedPart.bottom, blockAABB.bottom);
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
        BaseMapObject* block = static_cast<BaseMapObject*>(collider->GetUserData());
        AABB2D blockAABB = collider->GetAABB();

        const bool isHazard = block->IsHazard();
        const float margin = isHazard ? 0.005f : normalMargin;
        if (checkPartHit(bodyAABB, blockAABB, margin, isHazard) ||
            checkPartHit(headAABB, blockAABB, margin, isHazard)) {
            block->OnCollision(player);
        }
    }
}

AABB2D PlayerCollisionSolver::GetAABB(const PlayerState& state, const PlayerParams& params) const {
    AABB2D body = GetBodyAABB(state, params);
    AABB2D head = GetHeadAABB(state, params);
    return {
        (std::min)(body.left, head.left),
        (std::max)(body.top, head.top),
        (std::max)(body.right, head.right),
        (std::min)(body.bottom, head.bottom)
    };
}

AABB2D PlayerCollisionSolver::GetBodyAABB(const PlayerState& state, const PlayerParams& params) const {
    auto parts = BuildHitParts(state, params);
    return MakePartAABB(state.position_, parts[0]);
}

AABB2D PlayerCollisionSolver::GetHeadAABB(const PlayerState& state, const PlayerParams& params) const {
    auto parts = BuildHitParts(state, params);
    return MakePartAABB(state.position_, parts[1]);
}
