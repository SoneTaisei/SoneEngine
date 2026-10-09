#include "SphereColliderComponent.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

SphereColliderComponent::SphereColliderComponent() {
    type_ = ColliderType::kSphere;
    sphereRadius_ = 1.0f;
    sphereOffset_ = { 0.0f, 0.0f, 0.0f };
}

void SphereColliderComponent::DisplayImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Sphere Collider Component");
    ImGui::DragFloat("Sphere Radius", &sphereRadius_, 0.05f, 0.01f, 100.0f);
    ImGui::DragFloat3("Sphere Offset", &sphereOffset_.x, 0.05f, -100.0f, 100.0f);

    bool solid = isSolid_;
    if (ImGui::Checkbox("Is Solid", &solid)) isSolid_ = solid;
    bool oneway = isOneWay_;
    if (ImGui::Checkbox("Is OneWay", &oneway)) isOneWay_ = oneway;
#endif
}

AABB2D SphereColliderComponent::GetAABB() const {
    if (hasCustomAABB_) {
        return customAABB_;
    }

    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;

    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            pos = tc->GetPosition();
            scale = tc->GetScale().x; // 球体はXスケールを基準にする
        }
    }

    pos.x += sphereOffset_.x;
    pos.y += sphereOffset_.y;

    float r = sphereRadius_ * scale;

    return {
        pos.x - r,
        pos.y + r,
        pos.x + r,
        pos.y - r
    };
}
