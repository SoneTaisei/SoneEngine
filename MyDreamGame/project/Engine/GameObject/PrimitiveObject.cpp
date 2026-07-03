#include "PrimitiveObject.h"
#include "Renderer/Renderer.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "GameObject/Object3D.h"
#include "../externals/imgui/imgui.h"
#include "Renderer/Renderer.h"
#include "Editor/ReplayManager.h"
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

    // 繧ｴ繝ｼ繧ｹ繝域緒逕ｻ逕ｨ繝ｪ繧ｽ繝ｼ繧ｹ縺ｮ蛻晄悄蛹・
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
        // 繧ｫ繝｡繝ｩ縺ｮ繝ｯ繝ｼ繝ｫ繝芽｡悟・・医ン繝･繝ｼ陦悟・縺ｮ騾・｡悟・・峨ｒ蜿門ｾ・
        Matrix4x4 cameraMatrix = TransformFunctions::Inverse(viewMatrix);
        // 蝗櫁ｻ｢謌仙・縺ｮ縺ｿ繧呈歓蜃ｺ縺励※繝薙Ν繝懊・繝芽｡悟・縺ｨ縺吶ｋ
        billboardMatrix = cameraMatrix;
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(transform_.scale);
    
    // 蝗櫁ｻ｢陦悟・縺ｮ菴懈・ (XYZ縺ｮ鬆・〒蜷域・)
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(transform_.rotate.x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(transform_.rotate.y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(transform_.rotate.z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);

    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(transform_.translate);

    // 繝薙Ν繝懊・繝峨′譛牙柑縺ｪ蝣ｴ蜷医・縲√ン繝ｫ繝懊・繝牙屓霆｢繧帝←逕ｨ縺励◆蠕後↓繝ｭ繝ｼ繧ｫ繝ｫ蝗櫁ｻ｢繧帝←逕ｨ縺吶ｋ
    // (Z蝗櫁ｻ｢縺ｪ縺ｩ縺ｧ陦ｨ遉ｺ繧貞だ縺代ｉ繧後ｋ繧医≧縺ｫ縺吶ｋ縺溘ａ)
    Matrix4x4 localMatrix;
    if (isBillboard_) {
        localMatrix = TransformFunctions::Multiply(rotateMatrix, billboardMatrix);
    } else {
        localMatrix = rotateMatrix;
    }
    
    worldMatrix_ = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMatrix);
    
    if (parent_) {
        worldMatrix_ = TransformFunctions::Multiply(worldMatrix_, parent_->GetWorldMatrix());
    }
    
    mappedTransform_->World = worldMatrix_;
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix_, viewMatrix), projectionMatrix);
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix_, viewMatrix), projectionMatrix);
    mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix_));
}

void PrimitiveObject::Draw() {
    Renderer::GetInstance()->DrawPrimitiveObject(this);
}

void PrimitiveObject::DrawGhost(const Transform& transform, const Material& material) {
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

