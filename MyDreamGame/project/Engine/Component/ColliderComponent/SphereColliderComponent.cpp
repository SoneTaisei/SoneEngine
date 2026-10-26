#include "SphereColliderComponent.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

SphereColliderComponent::SphereColliderComponent() {
    SetColliderType(ColliderType::kSphere);
    SetSphereRadius(1.0f);
    SetSphereOffset({0.0f, 0.0f, 0.0f});
}

void SphereColliderComponent::DisplayImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Sphere Collider Component");
    float sphereRadius = GetSphereRadius();
    ImGui::DragFloat("Sphere Radius", &sphereRadius, 0.05f, 0.01f, 100.0f);
    Vector3 sphereOffset = GetSphereOffset();
    ImGui::DragFloat3("Sphere Offset", &sphereOffset.x, 0.05f, -100.0f, 100.0f);

    bool solid = IsSolid();
    ImGui::Checkbox("Is Solid", &solid);
    bool oneway = IsOneWay();
    ImGui::Checkbox("Is OneWay", &oneway);
#endif
}

AABB2D SphereColliderComponent::GetAABB() const {
    if (HasCustomAABB()) {
        return GetCustomAABB();
    }

    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;

    if (GetGameObject()) {
        if (auto *tc = GetGameObject()->GetComponent<TransformComponent>()) {
            pos = tc->GetPosition();
            scale = tc->GetScale().x; // 球体はXスケールを基準にする
        }
    }

    pos.x += GetSphereOffset().x;
    pos.y += GetSphereOffset().y;

    float r = GetSphereRadius() * scale;

    return {
        pos.x - r,
        pos.y + r,
        pos.x + r,
        pos.y - r
    };
}
