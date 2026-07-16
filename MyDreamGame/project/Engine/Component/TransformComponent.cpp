#include "TransformComponent.h"
#include "Core/Utility/TransformFunctions.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

void TransformComponent::Initialize() {
    UpdateMatrix();
}

void TransformComponent::Update() {
    if (isDirty_) {
        UpdateMatrix();
    }
}

void TransformComponent::UpdateMatrix() {
    worldMatrix_ = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    if (parent_) {
        worldMatrix_ = TransformFunctions::Multiply(worldMatrix_, parent_->GetWorldMatrix());
    }
    isDirty_ = false;
}

void TransformComponent::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("TransformComponent")) {
        if (ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f)) isDirty_ = true;
        if (ImGui::DragFloat3("Rotation", &transform_.rotate.x, 0.01f)) isDirty_ = true;
        if (ImGui::DragFloat3("Scale", &transform_.scale.x, 0.1f)) isDirty_ = true;
        ImGui::TreePop();
    }
#endif
}
