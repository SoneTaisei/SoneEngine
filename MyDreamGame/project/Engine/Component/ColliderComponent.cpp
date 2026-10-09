#include "ColliderComponent.h"
#include "Collision/CollisionManager.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

ColliderComponent::ColliderComponent() {
}

ColliderComponent::~ColliderComponent() {
    CollisionManager::GetInstance()->UnregisterCollider(this);
}

void ColliderComponent::Initialize() {
    CollisionManager::GetInstance()->RegisterCollider(this);
}

void ColliderComponent::Update() {
}

void ColliderComponent::DisplayImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Collider Component");
    const char* typeNames[] = { "AABB (Axis-Aligned)", "OBB (Oriented)", "Sphere" };
    int currentType = static_cast<int>(type_);
    if (ImGui::Combo("Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
        type_ = static_cast<ColliderType>(currentType);
    }

    if (type_ == ColliderType::kAABB || type_ == ColliderType::kOBB) {
        ImGui::DragFloat3("Box Size", &boxSize_.x, 0.05f, 0.01f, 100.0f);
        ImGui::DragFloat3("Box Offset", &boxOffset_.x, 0.05f, -100.0f, 100.0f);
    } else if (type_ == ColliderType::kSphere) {
        ImGui::DragFloat("Sphere Radius", &sphereRadius_, 0.05f, 0.01f, 100.0f);
        ImGui::DragFloat3("Sphere Offset", &sphereOffset_.x, 0.05f, -100.0f, 100.0f);
    }

    bool solid = isSolid_;
    if (ImGui::Checkbox("Is Solid", &solid)) isSolid_ = solid;
    bool oneway = isOneWay_;
    if (ImGui::Checkbox("Is OneWay", &oneway)) isOneWay_ = oneway;
#endif
}

AABB2D ColliderComponent::GetAABB() const {
    if (hasCustomAABB_) {
        return customAABB_;
    }

    Vector3 pos = {0.0f, 0.0f, 0.0f};
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
