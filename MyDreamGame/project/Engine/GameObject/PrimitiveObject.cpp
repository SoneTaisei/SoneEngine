#include "PrimitiveObject.h"
#include "Renderer/Renderer.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "GameObject/Object3D.h"
#include "../externals/imgui/imgui.h"
#include "Renderer/Renderer.h"
#include "Editor/Replay/ReplayManager.h"
#include <algorithm>

D3D12_GPU_DESCRIPTOR_HANDLE PrimitiveObject::sDefaultTextureHandle_ = {};

void PrimitiveObject::Initialize(ID3D12Device* device, Primitive* primitive) {
    primitive_ = primitive;
    transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    material_.color = {1.0f, 1.0f, 1.0f, 1.0f};
    material_.lightingType = 1;
    material_.enableBlinnPhong = 0;
    material_.uvTransform = TransformFunctions::MakeIdentity4x4();
    material_.shininess = 50.0f;
    material_.enableEnvironmentMap = 1;
    material_.environmentCoefficient = 0.1f;
    material_.alphaReference = 0.0f;
    material_.dissolveThreshold = 0.0f;

    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTransform_));

    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial_));

    // Write initial state to mapped memory immediately to prevent all-zeroes on first frame
    *mappedMaterial_ = material_;
    Matrix4x4 identity = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->World = identity;
    mappedTransform_->WVP = identity;
    mappedTransform_->WorldInverseTranspose = identity;

    // ゴースト描画用リソースの初期化
    uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
    uint32_t materialSize = (sizeof(Material) + 255) & ~255u;
    
    ghostTransformResource_ = CreateBufferResource(device, transformSize * kMaxGhosts);
    ghostTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGhostTransform_));
    
    ghostMaterialResource_ = CreateBufferResource(device, materialSize * kMaxGhosts);
    ghostMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedGhostMaterial_));
}

void PrimitiveObject::Update() {
    *mappedMaterial_ = material_;

    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();
    if (isBillboard_) {
        // カメラのワールド行列(ビュー行列の逆行列)を取得
        Matrix4x4 cameraMatrix = TransformFunctions::Inverse(viewMatrix);
        // 回転成分のみを抽出してビルボード行列とする
        billboardMatrix = cameraMatrix;
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(transform_.scale);
    
    // 回転行列の作成 (XYZの順で合成)
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(transform_.rotate.x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(transform_.rotate.y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(transform_.rotate.z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);

    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(transform_.translate);

    // ビルボードが有効な場合、ビルボード回転を適用した後にローカル回転を適用する
    // (Z回転などで表示を傾けられるようにするため)
    Matrix4x4 localMatrix;
    if (isBillboard_) {
        localMatrix = TransformFunctions::Multiply(rotateMatrix, billboardMatrix);
    } else {
        localMatrix = rotateMatrix;
    }
    
    if (overrideMatrix_) {
        worldMatrix_ = overriddenWorldMatrix_;
    } else {
        worldMatrix_ = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMatrix);
        
        if (parent_) {
            worldMatrix_ = TransformFunctions::Multiply(worldMatrix_, parent_->GetWorldMatrix());
        }
    }
    
    mappedTransform_->World = worldMatrix_;
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix_, viewMatrix), projectionMatrix);
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix_, viewMatrix), projectionMatrix);
    mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix_));
}

void PrimitiveObject::Draw() {
    Renderer::GetInstance()->DrawPrimitiveObject(this);
}

void PrimitiveObject::DrawGhost(const EulerTransform& transform, const Material& material) {
    Renderer::GetInstance()->DrawPrimitiveGhost(this, transform, material);
}

void PrimitiveObject::DisplayImGui(const std::string& label) {
#ifdef USE_IMGUI
    if (ImGui::TreeNode(label.c_str())) {
        ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f);
        ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f);
        ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);

        if (ImGui::TreeNode("Material")) {
            ImGui::ColorEdit4("Color", &material_.color.x);
            ImGui::DragFloat("Shininess", &material_.shininess, 0.1f, 0.1f, 100.0f);
            ImGui::SliderFloat("Environment Coefficient", &material_.environmentCoefficient, 0.0f, 1.0f);
            ImGui::Checkbox("Enable Environment Map", (bool*)&material_.enableEnvironmentMap);
            ImGui::Checkbox("Lighting Enable", (bool*)&material_.lightingType);
            ImGui::DragFloat("Alpha Reference", &material_.alphaReference, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Trail (霆瑚ｷ｡繝ｻ谿句ワ)")) {
            ImGui::Checkbox("Show Trail", &showTrail_);
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}

