#include "Object3D.h"
#include "Renderer/Renderer.h"
#include "Graphics/CameraManager.h"
#include <DirectXMath.h>
#include "../externals/imgui/imgui.h"
#include "Renderer/Renderer.h"
#include "Editor/ReplayManager.h"
#include <algorithm>

D3D12_GPU_DESCRIPTOR_HANDLE Object3D::sEnvironmentMapHandle = {};

void Object3D::Initialize(ID3D12Device *device, Model *model) {
    model_ = model; // 共有されているモデルをセット
    transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    // マテリアルが透明にならないよう初期値を設定する
    material_.color = {1.0f, 1.0f, 1.0f, 1.0f};                    // 白色で不透明 (RGBA)
    material_.lightingType = 1;                                    // ライティング有効
    material_.uvTransform = TransformFunctions::MakeIdentity4x4(); // 以前作った単位行列を返す関数
    material_.shininess = 50.0f;
    material_.enableEnvironmentMap = 1;
    material_.environmentCoefficient = 0.1f;

    // マテリアルと座標変換リソースの作成（1つ分だけ！）
    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));

    // Write initial state to mapped memory immediately to prevent all-zeroes on first frame
    *mappedMaterial_ = material_;
    Matrix4x4 identity = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->World = identity;
    mappedTransform_->WVP = identity;

    // Trail resources
    uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
    uint32_t materialSize = (sizeof(Material) + 255) & ~255u;
    
    trailTransformResource_ = CreateBufferResource(device, transformSize * kMaxTrails);
    trailTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTrailTransform_));
    
    trailMaterialResource_ = CreateBufferResource(device, materialSize * kMaxTrails);
    trailMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTrailMaterial_));
}

void Object3D::Update() {
    *mappedMaterial_ = material_;

    // マネージャから最新のカメラ情報をゲット
    // ※マネージャから最新のカメラ情報をゲット！！
    CameraManager *cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    // 自身のワールド行列作成
    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // モデル側のローカル行列を除外（スキニング対応のため）
    Matrix4x4 nodeMatrix = model_->GetModelData().rootNode.localMatrix;
    Matrix4x4 finalWorldMatrix = TransformFunctions::Multiply(nodeMatrix, worldMatrix);

    mappedTransform_->World = finalWorldMatrix;
    mappedTransform_->WVP = finalWorldMatrix * viewMatrix * projectionMatrix;

    // ※ここを修正：順は World * View * Projection
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(finalWorldMatrix, viewMatrix), projectionMatrix);

    // ※追加：法線用行列の計算（これがないとライティングが真っ黒になります！）
    mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(finalWorldMatrix));

    // 軌跡用履歴の保存
    trailHistory_.push_front(transform_);
    if (trailHistory_.size() > kMaxHistory) {
        trailHistory_.pop_back();
    }
}

void Object3D::Draw() {
    Renderer::GetInstance()->DrawObject3D(this);
}

void Object3D::DisplayImGui(const std::string &label) {
#ifdef USE_IMGUI
    if (ImGui::TreeNode(label.c_str())) {
        ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f);
        ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f);
        ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Material")) {
            ImGui::ColorEdit4("Color", &material_.color.x);
            ImGui::DragFloat("Shininess", &material_.shininess, 0.1f, 0.1f, 100.0f);
            ImGui::SliderFloat("Environment Coefficient", &material_.environmentCoefficient, 0.0f, 1.0f);
            ImGui::Checkbox("Enable Environment Map", (bool*)&material_.enableEnvironmentMap);
            ImGui::Checkbox("Lighting Enable", (bool*)&material_.lightingType);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Trail (霆瑚ｷ｡繝ｻ谿句ワ)")) {
            ImGui::Checkbox("Show Trail", &showTrail_);
            if (showTrail_) {
                ImGui::Checkbox("Fade Out", &trailFadeOut_);
                ImGui::DragInt("Trail Step", &trailStep_, 1, 1, 60);
                ImGui::DragInt("Trail Length", &trailLength_, 1, 1, kMaxHistory);
                ImGui::DragFloat("Start Alpha", &trailStartAlpha_, 0.01f, 0.0f, 1.0f);
                int blendModeIdx = static_cast<int>(trailBlendMode_);
                if (ImGui::Combo("Blend Mode", &blendModeIdx, "Normal\0Add\0Subtract\0Multiply\0Screen\0")) {
                    trailBlendMode_ = static_cast<BlendMode>(blendModeIdx);
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}
