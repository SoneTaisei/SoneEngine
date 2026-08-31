#include "MeshRendererComponent.h"
#include "GameObject/GameObject.h"
#include "TransformComponent.h"
#include "Renderer/Renderer.h"
#include "Core/Utility/UtilityFunctions.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

MeshRendererComponent::MeshRendererComponent() {
}

MeshRendererComponent::~MeshRendererComponent() {
}

void MeshRendererComponent::Initialize(ID3D12Device* device, Model* model) {
    model_ = model;
    
    // マテリアル用バッファ生成
    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial_));

    // デフォルトマテリアル設定
    material_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    material_.lightingType = 1; // Meshは通常ライティング有効
    material_.enableBlinnPhong = 1;
    material_.enableEnvironmentMap = 0;
    material_.alphaReference = 0.0f;
    material_.uvTransform = TransformFunctions::MakeIdentity4x4();
    material_.shininess = 50.0f;
    material_.environmentCoefficient = 1.0f;
    material_.dissolveThreshold = 0.0f;
    material_.enableBoxMapping = 0.0f;
    *mappedMaterial_ = material_;

    // Transform用バッファ生成
    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransform_));
}

void MeshRendererComponent::Initialize() {
    // 引数なし版
}

void MeshRendererComponent::Update() {
    *mappedMaterial_ = material_;
}

void MeshRendererComponent::Draw() {
    Renderer::GetInstance()->AddMeshComponent(this);
}

void MeshRendererComponent::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Mesh Renderer")) {
        if (ImGui::TreeNode("Material")) {
            ImGui::ColorEdit4("Color", &material_.color.x);
            ImGui::DragFloat("Shininess", &material_.shininess, 0.1f, 0.1f, 100.0f);
            ImGui::SliderFloat("Environment Coefficient", &material_.environmentCoefficient, 0.0f, 1.0f);
            ImGui::Checkbox("Enable Environment Map", (bool*)&material_.enableEnvironmentMap);
            ImGui::Checkbox("Lighting Enable", (bool*)&material_.lightingType);
            ImGui::DragFloat("Alpha Reference", &material_.alphaReference, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}
