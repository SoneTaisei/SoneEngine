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
    bool solid = isSolid_;
    if (ImGui::Checkbox("Is Solid", &solid)) isSolid_ = solid;
    bool oneway = isOneWay_;
    if (ImGui::Checkbox("Is OneWay", &oneway)) isOneWay_ = oneway;
#endif
}

AABB2D ColliderComponent::GetAABB() const {
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
