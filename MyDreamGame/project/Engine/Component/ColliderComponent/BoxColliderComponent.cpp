#include "BoxColliderComponent.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#include "Renderer/LineRenderer.h"             
#include "Core/Utility/TransformFunctions.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

BoxColliderComponent::BoxColliderComponent() {
    SetBoxSize({1.0f, 1.0f, 1.0f});
    SetBoxOffset({0.0f, 0.0f, 0.0f});
}

void BoxColliderComponent::DisplayImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Box Collider Component");
    // Box Size
    Vector3 size = GetBoxSize();
    if (ImGui::DragFloat3("Box Size", &size.x, 0.05f, 0.01f, 100.0f)) {
        SetBoxSize(size);
    }
    // Box Offset
    Vector3 offset = GetBoxOffset();
    if (ImGui::DragFloat3("Box Offset", &offset.x, 0.05f, -100.0f, 100.0f)) {
        SetBoxOffset(offset);
    }
    // Is Solid
    bool solid = IsSolid();
    if (ImGui::Checkbox("Is Solid", &solid)) {
        SetIsSolid(solid);
    }
    // Is OneWay
    bool oneway = IsOneWay();
    if (ImGui::Checkbox("Is OneWay", &oneway)) {
        SetIsOneWay(oneway);
    }
#endif
}

AABB2D BoxColliderComponent::GetAABB() const {
    if (HasCustomAABB()) {
        return GetCustomAABB();
    }

    Vector3 pos = {0.0f, 0.0f, 0.0f};
    Vector3 scale = GetBoxSize(); // ← boxSize_ から変更

    if (GetGameObject()) {
        if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
            pos = tc->GetPosition();
            scale.x *= tc->GetScale().x;
            scale.y *= tc->GetScale().y;
        }
    }

    pos.x += GetBoxOffset().x; // ← boxOffset_.x から変更
    pos.y += GetBoxOffset().y; // ← boxOffset_.y から変更

    return {
        pos.x - scale.x * 0.5f,
        pos.y + scale.y * 0.5f,
        pos.x + scale.x * 0.5f,
        pos.y - scale.y * 0.5f};
}

void BoxColliderComponent::DrawDebug(LineRenderer *lineRenderer, const Vector4 &color) const {
    if (!lineRenderer)
        return;

    // 1. 自身の Transform を取得する（元の CollisionManager.cpp 83〜92行目）
    Vector3 basePos = {0.0f, 0.0f, 0.0f};
    Vector3 baseRotate = {0.0f, 0.0f, 0.0f};
    Vector3 baseScale = {1.0f, 1.0f, 1.0f};
    if (auto *go = GetGameObject()) {
        if (auto *tc = go->GetComponent<TransformComponent>()) {
            basePos = tc->GetPosition();
            baseRotate = tc->GetRotation();
            baseScale = tc->GetScale();
        }
    }

    // 2. プレイヤーかどうかの判定（元の CollisionManager.cpp 62〜63行目）
    bool isPlayer = (GetLayerMask() & (kLayerPlayer | kLayerPlayerHead)) != 0;

    // 3. AABB / OBB の描画（元の CollisionManager.cpp 94〜122行目）
    if (GetColliderType() == ColliderType::kAABB) {
        AABB2D aabb = GetAABB();
        if (isPlayer) {
            Vector3 minP = {aabb.left, aabb.bottom, basePos.z};
            Vector3 maxP = {aabb.right, aabb.top, basePos.z};
            lineRenderer->AddAABB(minP, maxP, color);
        } else {
            float depth = GetBoxSize().z * baseScale.z;
            float halfDepth = (depth > 0.01f) ? (depth * 0.5f) : 0.5f;
            Vector3 minP = {aabb.left, aabb.bottom, basePos.z - halfDepth};
            Vector3 maxP = {aabb.right, aabb.top, basePos.z + halfDepth};
            lineRenderer->AddAABB(minP, maxP, color);
        }
    } else if (GetColliderType() == ColliderType::kOBB) {
        Vector3 size = GetBoxSize();
        size.x *= baseScale.x;
        size.y *= baseScale.y;
        size.z = (size.z > 0.01f) ? (size.z * baseScale.z) : 1.0f;
        Vector3 offset = GetBoxOffset();
        offset.x *= baseScale.x;
        offset.y *= baseScale.y;
        offset.z *= baseScale.z;

        Matrix4x4 rotMat = TransformFunctions::MakeAffineMatrix({1.0f, 1.0f, 1.0f}, baseRotate, {0.0f, 0.0f, 0.0f});
        Vector3 rotOffset = rotMat * offset;
        Vector3 center = {basePos.x + rotOffset.x, basePos.y + rotOffset.y, basePos.z + rotOffset.z};
        lineRenderer->AddOBB(center, size, baseRotate, color);
    }
}
