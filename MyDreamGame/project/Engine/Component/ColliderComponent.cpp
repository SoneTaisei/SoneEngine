#include "ColliderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void ColliderComponent::Update() {
    // Collision detection is usually handled by a PhysicsSystem iterating over all colliders.
    // For now, this is just a data container.
}

void ColliderComponent::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Collider Component")) {
        int typeInt = static_cast<int>(type_);
        if (ImGui::Combo("Type", &typeInt, "Box\0Sphere\0")) {
            type_ = static_cast<ColliderType>(typeInt);
        }

        if (type_ == ColliderType::kBox) {
            ImGui::DragFloat3("Box Size", &boxSize_.x, 0.1f);
            ImGui::DragFloat3("Box Offset", &boxOffset_.x, 0.1f);
        } else {
            ImGui::DragFloat("Sphere Radius", &sphereRadius_, 0.1f);
            ImGui::DragFloat3("Sphere Offset", &sphereOffset_.x, 0.1f);
        }
        ImGui::TreePop();
    }
#endif
}
