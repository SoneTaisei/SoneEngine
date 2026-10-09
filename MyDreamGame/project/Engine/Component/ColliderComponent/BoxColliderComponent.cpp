#include "BoxColliderComponent.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

BoxColliderComponent::BoxColliderComponent() {
    type_ = ColliderType::kAABB;
    boxSize_ = { 1.0f, 1.0f, 1.0f };
    boxOffset_ = { 0.0f, 0.0f, 0.0f };
}

void BoxColliderComponent::DisplayImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Box Collider Component");
    ImGui::DragFloat3("Box Size", &boxSize_.x, 0.05f, 0.01f, 100.0f);
    ImGui::DragFloat3("Box Offset", &boxOffset_.x, 0.05f, -100.0f, 100.0f);

    bool solid = isSolid_;
    if (ImGui::Checkbox("Is Solid", &solid)) isSolid_ = solid;
    bool oneway = isOneWay_;
    if (ImGui::Checkbox("Is OneWay", &oneway)) isOneWay_ = oneway;
#endif
}

AABB2D BoxColliderComponent::GetAABB() const {
    if (hasCustomAABB_) {
        return customAABB_;
    }

    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 scale = boxSize_;

    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            pos = tc->GetPosition();
            scale.x *= tc->GetScale().x;
            scale.y *= tc->GetScale().y;
        }
    }

    pos.x += boxOffset_.x;
    pos.y += boxOffset_.y;

    return {
        pos.x - scale.x * 0.5f,
        pos.y + scale.y * 0.5f,
        pos.x + scale.x * 0.5f,
        pos.y - scale.y * 0.5f
    };
}
