#include "CollisionManager.h"
#include "Component/ColliderComponent.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#include "Core/Utility/TransformFunctions.h"
#include "Renderer/LineRenderer.h"
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
        if (aabb.Intersects(cAABB)) {
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

void CollisionManager::Draw3D(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix) {
    if (!isDebugDrawEnabled_ || colliders_.empty() || !commandList) return;

    LineRenderer* lineRenderer = LineRenderer::GetInstance();

    for (auto* col : colliders_) {
        if (!col || !col->IsEnabled()) continue;

        bool isPlayerCollider = (col->GetLayerMask() & kLayerPlayer) != 0;
        bool isPlayerHeadCollider = (col->GetLayerMask() & kLayerPlayerHead) != 0;
        ColliderType type = col->GetColliderType();

        // 形状タイプ・対象ごとの色分け
        // Player Body: 黄 (1.0f, 1.0f, 0.0f, 1.0f)
        // Player Head: マゼンタ (1.0f, 0.3f, 0.8f, 1.0f)
        // AABB: 緑 (0.0f, 1.0f, 0.0f, 1.0f)
        // OBB: オレンジ (1.0f, 0.65f, 0.0f, 1.0f)
        // Sphere: 水色 (0.0f, 0.86f, 1.0f, 1.0f)
        Vector4 wireColor = { 0.0f, 1.0f, 0.0f, 1.0f };
        if (isPlayerHeadCollider) {
            wireColor = { 1.0f, 0.3f, 0.8f, 1.0f };
        } else if (isPlayerCollider) {
            wireColor = { 1.0f, 1.0f, 0.0f, 1.0f };
        } else if (type == ColliderType::kOBB) {
            wireColor = { 1.0f, 0.65f, 0.0f, 1.0f };
        } else if (type == ColliderType::kSphere) {
            wireColor = { 0.0f, 0.86f, 1.0f, 1.0f };
        }

        col->DrawDebug(lineRenderer, wireColor);
    }

    lineRenderer->Render(commandList, viewProjectionMatrix, isDepthTestEnabled_);
}
