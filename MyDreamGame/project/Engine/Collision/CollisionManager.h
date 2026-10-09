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

    // Debug Wireframe Draw
    bool IsDebugDrawEnabled() const { return isDebugDrawEnabled_; }
    void SetDebugDrawEnabled(bool enabled) { isDebugDrawEnabled_ = enabled; }

    bool IsDepthTestEnabled() const { return isDepthTestEnabled_; }
    void SetDepthTestEnabled(bool enabled) { isDepthTestEnabled_ = enabled; }

    // DirectX 12 3D ライン描画（深度テスト対応でブロックに遮蔽される）
    void Draw3D(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix);

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    std::vector<ColliderComponent*> colliders_;
    bool isDebugDrawEnabled_ = false;
    bool isDepthTestEnabled_ = true;
};
