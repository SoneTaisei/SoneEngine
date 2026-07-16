#pragma once
#include <vector>
#include "Core/Utility/Structs.h"

class ColliderComponent;

class CollisionManager {
public:
    static CollisionManager* GetInstance();

    void RegisterCollider(ColliderComponent* collider);
    void UnregisterCollider(ColliderComponent* collider);

    // Queries
    std::vector<ColliderComponent*> GetCollidersInAABB(const AABB2D& aabb, uint32_t layerMask = 0xFFFFFFFF) const;

    // Trigger checks
    void Update();

    // Clear all colliders (on scene change)
    void Clear();

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    std::vector<ColliderComponent*> colliders_;
};
