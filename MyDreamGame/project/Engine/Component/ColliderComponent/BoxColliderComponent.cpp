#include "BoxColliderComponent.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

BoxColliderComponent::BoxColliderComponent() {
    SetColliderType(ColliderType::kAABB);
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

    if (gameObject_) {
        if (auto *tc = gameObject_->GetComponent<TransformComponent>()) {
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