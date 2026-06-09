#include "PrimitiveObject.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "GameObject/Object3D.h"
#include "../externals/imgui/imgui.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
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
        // カメラのワールド行列（ビュー行列の逆行列）を取得
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

    // ビルボードが有効な場合は、ビルボード回転を適用した後にローカル回転を適用する
    // (Z回転などで表示を傾けられるようにするため)
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

void PrimitiveObject::Draw(ID3D12GraphicsCommandList* commandList) {
    // 描画直前に最新のパラメータでマテリアルを更新
    *mappedMaterial_ = material_;

    // 描画直前に最新のカメラ行列でWVPとワールド行列を再計算してGPUに送る（停止中のデバッグカメラ・ビルボード追従、およびエディタでの即時反映のため）
    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();
    if (isBillboard_) {
        Matrix4x4 cameraMatrix = TransformFunctions::Inverse(viewMatrix);
        billboardMatrix = cameraMatrix;
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(transform_.scale);
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(transform_.rotate.x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(transform_.rotate.y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(transform_.rotate.z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(transform_.translate);

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
    mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix_));
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix_, viewMatrix), projectionMatrix);

    auto dxCommon = DirectXCommon::GetInstance();
    D3D12_GPU_DESCRIPTOR_HANDLE activeTexture = textureHandle_;
    if (activeTexture.ptr == 0) {
        activeTexture = sDefaultTextureHandle_;
    }

    // Ensure common model root signature is bound
    commandList->SetGraphicsRootSignature(dxCommon->GetRootSignature());

    // ==============================================================
    // ★ 1. 先に Main drawing pass (本体のプリミティブ) を描画する！
    // ==============================================================
    if (blendMode_ == BlendMode::kBlendModeAdd) {
        if (isDoubleSided_) {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCullAdditive());
        } else {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateAdditive());
        }
    } else {
        // 通常合成 (または未対応のモード)
        if (material_.color.w < 1.0f) {
            // 半透明の場合はデプス書き込みなしのTransparentパイプラインを使用する
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateTransparent());
        } else {
            if (isDoubleSided_) {
                commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCull());
            } else {
                commandList->SetPipelineState(dxCommon->GetGraphicsPipelineState());
            }
        }
    }

    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // カメラの定数バッファをセット (スロット3)
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
    
    if (activeTexture.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
    }

    // ★ 環境マップをセット (Object3D.PS.hlsl が register(t1) = スロット7 を使うため)
    if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
    }

    if (primitive_) {
        primitive_->Draw(commandList);
    }

    // ==============================================================
    // ★ 2. 後から Outline drawing pass (ワイヤーフレーム) を重ねる！
    // ==============================================================
    if (dxCommon->IsOutlineEnabled() && material_.color.w >= 1.0f && blendMode_ != BlendMode::kBlendModeAdd) {
        commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateOutline());
        commandList->SetGraphicsRootConstantBufferView(8, dxCommon->GetOutlineParamsGPUAddress());
        
        // ※ルートパラメータ（行列やマテリアル、テクスチャ）は本体描画時にセット済みなのでそのまま使えます
        
        if (primitive_) {
            primitive_->Draw(commandList);
        }
    }
}

void PrimitiveObject::DrawGhost(ID3D12GraphicsCommandList* commandList, const Transform& transform, const Material& material) {
    if (currentGhostIndex_ >= kMaxGhosts || !primitive_) return;

    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();
    if (isBillboard_) {
        Matrix4x4 cameraMatrix = TransformFunctions::Inverse(viewMatrix);
        billboardMatrix = cameraMatrix;
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(transform.scale);
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(transform.rotate.x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(transform.rotate.y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(transform.rotate.z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(transform.translate);

    Matrix4x4 localMatrix;
    if (isBillboard_) {
        localMatrix = TransformFunctions::Multiply(rotateMatrix, billboardMatrix);
    } else {
        localMatrix = rotateMatrix;
    }
    
    Matrix4x4 worldMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMatrix);
    if (parent_) {
        worldMatrix = TransformFunctions::Multiply(worldMatrix, parent_->GetWorldMatrix());
    }

    uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
    uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

    TransformMatrix* tMat = reinterpret_cast<TransformMatrix*>(mappedGhostTransform_ + transformSize * currentGhostIndex_);
    tMat->World = worldMatrix;
    tMat->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix));
    tMat->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix, viewMatrix), projectionMatrix);

    Material* mMat = reinterpret_cast<Material*>(mappedGhostMaterial_ + materialSize * currentGhostIndex_);
    *mMat = material;

    auto dxCommon = DirectXCommon::GetInstance();
    commandList->SetGraphicsRootSignature(dxCommon->GetRootSignature());

    // ブレンドモードの設定
    if (blendMode_ == BlendMode::kBlendModeAdd) {
        if (isDoubleSided_) commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCullAdditive());
        else commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateAdditive());
    } else {
        if (material.color.w < 1.0f) {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateTransparent());
        } else {
            if (isDoubleSided_) commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCull());
            else commandList->SetPipelineState(dxCommon->GetGraphicsPipelineState());
        }
    }

    commandList->SetGraphicsRootConstantBufferView(1, ghostTransformResource_->GetGPUVirtualAddress() + transformSize * currentGhostIndex_);
    commandList->SetGraphicsRootConstantBufferView(0, ghostMaterialResource_->GetGPUVirtualAddress() + materialSize * currentGhostIndex_);
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
    
    D3D12_GPU_DESCRIPTOR_HANDLE activeTexture = textureHandle_;
    if (activeTexture.ptr == 0) activeTexture = sDefaultTextureHandle_;
    if (activeTexture.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
    }

    if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
    }

    primitive_->Draw(commandList);

    currentGhostIndex_++;
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
        if (ImGui::TreeNode("Trail (軌跡・残像)")) {
            ImGui::Checkbox("Show Trail", &showTrail_);
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}
