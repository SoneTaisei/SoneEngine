#include "Object3D.h"
#include "Graphics/CameraManager.h"
#include <DirectXMath.h>
#include "../externals/imgui/imgui.h"

D3D12_GPU_DESCRIPTOR_HANDLE Object3D::sEnvironmentMapHandle = {};

void Object3D::Initialize(ID3D12Device *device, Model *model) {
    model_ = model; // 共有されているモデルをセット
    transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    // マテリアルが透明にならないように初期値を設定する
    material_.color = {1.0f, 1.0f, 1.0f, 1.0f};                    // 白色で不透明 (RGBA)
    material_.lightingType = 1;                                    // ライティング有効
    material_.uvTransform = TransformFunctions::MakeIdentity4x4(); // 以前作った単位行列を返す関数
    material_.shininess = 50.0f;
    material_.environmentCoefficient = 0.1f;

    // マテリアルと座標変換リソースの作成（自分の分だけ）
    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    materialResource_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
}

void Object3D::Update() {
    *mappedMaterial_ = material_;

    // ★ マネージャから最新のカメラ情報をゲット！
    CameraManager *cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    // 自身のワールド行列作成
    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // モデル側のデータを使って最終的な行列を計算
    Matrix4x4 nodeMatrix = model_->GetModelData().rootNode.localMatrix;
    Matrix4x4 finalWorldMatrix = nodeMatrix * worldMatrix;

    mappedTransform_->World = finalWorldMatrix;
    mappedTransform_->WVP = finalWorldMatrix * viewMatrix * projectionMatrix;

    // ★ ここを修正！ 順序は World * View * Projection
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(finalWorldMatrix, viewMatrix), projectionMatrix);

    // ★ 追加：法線用行列の計算（これがないとライティングが真っ黒になります）
    mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(finalWorldMatrix));
}

void Object3D::Draw(ID3D12GraphicsCommandList *commandList) {
    // 💡 スロット1が行列、スロット0がマテリアルが正解です！
    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress()); // 行列 (スロット1)
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());  // マテリアル (スロット0)

    // 環境マップをスロット7にセット
    if (sEnvironmentMapHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, sEnvironmentMapHandle);
    }

    // 実際の描画
    model_->Draw();
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
            ImGui::Checkbox("Lighting Enable", (bool*)&material_.lightingType);
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}