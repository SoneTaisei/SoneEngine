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

void CollisionManager::Draw3D(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjectionMatrix) {
    if (!isDebugDrawEnabled_ || colliders_.empty() || !commandList) return;

    LineRenderer* lineRenderer = LineRenderer::GetInstance();

    for (auto* col : colliders_) {
        if (!col || !col->IsEnabled()) continue;

        ColliderType type = col->GetColliderType();

        // 形状タイプごとの色分け
        // AABB: 緑 (0.0f, 1.0f, 0.0f, 1.0f)
        // OBB: オレンジ (1.0f, 0.65f, 0.0f, 1.0f)
        // Sphere: 水色 (0.0f, 0.86f, 1.0f, 1.0f)
        Vector4 wireColor = { 0.0f, 1.0f, 0.0f, 1.0f };
        if (type == ColliderType::kOBB) {
            wireColor = { 1.0f, 0.65f, 0.0f, 1.0f };
        } else if (type == ColliderType::kSphere) {
            wireColor = { 0.0f, 0.86f, 1.0f, 1.0f };
        }

        Vector3 basePos = { 0.0f, 0.0f, 0.0f };
        Vector3 baseRotate = { 0.0f, 0.0f, 0.0f };
        Vector3 baseScale = { 1.0f, 1.0f, 1.0f };
        if (auto* go = col->GetGameObject()) {
            if (auto* tc = go->GetComponent<TransformComponent>()) {
                basePos = tc->GetPosition();
                baseRotate = tc->GetRotation();
                baseScale = tc->GetScale();
            }
        }

        if (type == ColliderType::kAABB) {
            AABB2D aabb = col->GetAABB();
            float depth = col->GetBoxSize().z * baseScale.z;
            float halfDepth = (depth > 0.01f) ? (depth * 0.5f) : 0.5f;
            Vector3 minP = { aabb.left, aabb.bottom, basePos.z - halfDepth };
            Vector3 maxP = { aabb.right, aabb.top, basePos.z + halfDepth };
            lineRenderer->AddAABB(minP, maxP, wireColor);
        }
        else if (type == ColliderType::kOBB) {
            Vector3 size = col->GetBoxSize();
            size.x *= baseScale.x;
            size.y *= baseScale.y;
            size.z = (size.z > 0.01f) ? (size.z * baseScale.z) : 1.0f;
            Vector3 offset = col->GetBoxOffset();
            offset.x *= baseScale.x;
            offset.y *= baseScale.y;
            offset.z *= baseScale.z;

            Matrix4x4 rotMat = TransformFunctions::MakeAffineMatrix({1.0f, 1.0f, 1.0f}, baseRotate, {0.0f, 0.0f, 0.0f});
            Vector3 rotOffset = rotMat * offset;
            Vector3 center = { basePos.x + rotOffset.x, basePos.y + rotOffset.y, basePos.z + rotOffset.z };
            lineRenderer->AddOBB(center, size, baseRotate, wireColor);
        }
        else if (type == ColliderType::kSphere) {
            float maxScale = baseScale.x;
            if (baseScale.y > maxScale) maxScale = baseScale.y;
            if (baseScale.z > maxScale) maxScale = baseScale.z;
            float radius = col->GetSphereRadius() * maxScale;
            Vector3 offset = col->GetSphereOffset();
            Vector3 center = { basePos.x + offset.x, basePos.y + offset.y, basePos.z + offset.z };
            lineRenderer->AddSphere(center, radius, wireColor);
        }
    }

    lineRenderer->Render(commandList, viewProjectionMatrix, isDepthTestEnabled_);
}
