#include "PrimitiveRendererComponent.h"
#include "GameObject/GameObject.h"
#include "TransformComponent.h"
#include "Renderer/Renderer.h"
#include "Core/Utility/UtilityFunctions.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

PrimitiveRendererComponent::PrimitiveRendererComponent() {
}

PrimitiveRendererComponent::~PrimitiveRendererComponent() {
}

void PrimitiveRendererComponent::Initialize(ID3D12Device* device, Primitive* primitive) {
    primitive_ = primitive;
    
    // マテリアル用バッファ生成
    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial_));

    // デフォルトマテリアル設定
    material_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    material_.lightingType = 0;
    material_.enableBlinnPhong = 0;
    material_.enableEnvironmentMap = 0;
    material_.alphaReference = 0.0f;
    material_.uvTransform = TransformFunctions::MakeIdentity4x4();
    material_.shininess = 50.0f;
    material_.environmentCoefficient = 1.0f;
    material_.dissolveThreshold = 0.0f;
    material_.padding2 = 0.0f;
    *mappedMaterial_ = material_;

    // Transform用バッファ生成
    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransform_));

    // 残像用バッファの生成
    uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
    uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

    ghostTransformResource_ = CreateBufferResource(device, transformSize * kMaxGhosts);
    ghostTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGhostTransform_));

    ghostMaterialResource_ = CreateBufferResource(device, materialSize * kMaxGhosts);
    ghostMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGhostMaterial_));
}

void PrimitiveRendererComponent::Initialize() {
    // 引数なし版はとりあえず何もしない。Initialize(device, primitive)を呼ぶ想定。
}

void PrimitiveRendererComponent::Update() {
    *mappedMaterial_ = material_;

    if (showTrail_ && gameObject_) {
        if (auto transformComp = gameObject_->GetComponent<TransformComponent>()) {
            UpdateGhost(transformComp->GetTransform());
        }
    }
}

void PrimitiveRendererComponent::UpdateGhost(const Transform& currentTransform) {
    if (currentGhostIndex_ < kMaxGhosts) {
        uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
        uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

        // 現在のTransformとMaterialをGPUに書き込む
        TransformMatrix* mappedTrans = reinterpret_cast<TransformMatrix*>(mappedGhostTransform_ + (currentGhostIndex_ * transformSize));
        if (gameObject_) {
            if (auto tc = gameObject_->GetComponent<TransformComponent>()) {
                mappedTrans->WVP = tc->GetWorldMatrix();
                mappedTrans->World = tc->GetWorldMatrix();
            }
        }
        
        Material* mappedMat = reinterpret_cast<Material*>(mappedGhostMaterial_ + (currentGhostIndex_ * materialSize));
        *mappedMat = material_;

        currentGhostIndex_++;
    }
}

void PrimitiveRendererComponent::Draw() {
    Renderer::GetInstance()->AddPrimitiveComponent(this);
}

void PrimitiveRendererComponent::DrawGhost(ID3D12GraphicsCommandList* commandList, const Transform& transform, const Material& material) {
    // 外部からのGhost描画用
    // TODO: 実際の描画ロジック（VertexBuffer, IndexBufferのセットなど）を実装
}

void PrimitiveRendererComponent::DisplayImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Primitive Renderer")) {
        if (ImGui::TreeNode("Material")) {
            ImGui::ColorEdit4("Color", &material_.color.x);
            ImGui::DragFloat("Shininess", &material_.shininess, 0.1f, 0.1f, 100.0f);
            ImGui::SliderFloat("Environment Coefficient", &material_.environmentCoefficient, 0.0f, 1.0f);
            ImGui::Checkbox("Enable Environment Map", (bool*)&material_.enableEnvironmentMap);
            ImGui::Checkbox("Lighting Enable", (bool*)&material_.lightingType);
            ImGui::DragFloat("Alpha Reference", &material_.alphaReference, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Trail")) {
            ImGui::Checkbox("Show Trail", &showTrail_);
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}
