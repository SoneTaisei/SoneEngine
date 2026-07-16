#include "CollisionManager.h"
#include "Component/ColliderComponent.h"
#include <algorithm>

CollisionManager* CollisionManager::GetInstance() {
    static CollisionManager instance;
    return &instance;
}

void CollisionManager::RegisterCollider(ColliderComponent* collider) {
    if (!collider) return;
    auto it = std::find(colliders_.begin(), colliders_.end(), collider);
    if (it == colliders_.end()) {
        colliders_.push_back(collider);
    }
}

void CollisionManager::UnregisterCollider(ColliderComponent* collider) {
    if (!collider) return;
    auto it = std::find(colliders_.begin(), colliders_.end(), collider);
    if (it != colliders_.end()) {
        colliders_.erase(it);
    }
}

std::vector<ColliderComponent*> CollisionManager::GetCollidersInAABB(const AABB2D& aabb, uint32_t layerMask) const {
    std::vector<ColliderComponent*> result;
    for (auto* collider : colliders_) {
        if ((collider->GetLayerMask() & layerMask) == 0) continue;
        
        AABB2D cAABB = collider->GetAABB();
        if (aabb.right > cAABB.left && aabb.left < cAABB.right &&
            aabb.top > cAABB.bottom && aabb.bottom < cAABB.top) {
            result.push_back(collider);
        }
    }
    return result;
}

void CollisionManager::Update() {
    // If we need to process triggers (like coin touching player)
    // we could do O(N^2) checks here or just let the PlayerPhysics do it.
    // For now, PlayerPhysics will query GetCollidersInAABB, so we leave this empty.
}

void CollisionManager::Clear() {
    colliders_.clear();
}
