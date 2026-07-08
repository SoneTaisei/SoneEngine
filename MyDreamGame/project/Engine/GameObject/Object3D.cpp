#include "Object3D.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Graphics/CameraManager.h"
#include <DirectXMath.h>
#include "../externals/imgui/imgui.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Editor/ReplayManager.h"
#include <algorithm>

D3D12_GPU_DESCRIPTOR_HANDLE Object3D::sEnvironmentMapHandle = {};

void Object3D::Initialize(ID3D12Device *device, Model *model) {
    model_ = model; // 共有されているモデルをセット
    transform_ = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    // マテリアルが透明にならないように初期値を設定する
    material_.color = {1.0f, 1.0f, 1.0f, 1.0f};                    // 白色で不透明 (RGBA)
    material_.lightingType = 1;                                    // ライティング有効
    material_.uvTransform = TransformFunctions::MakeIdentity4x4(); // 以前作った単位行列を返す関数
    material_.shininess = 50.0f;
    material_.enableEnvironmentMap = 1;
    material_.environmentCoefficient = 0.1f;

    // マテリアルと座標変換リソースの作成（自分の分だけ）
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

    // 軌跡用履歴の保存
    trailHistory_.push_front(transform_);
    if (trailHistory_.size() > kMaxHistory) {
        trailHistory_.pop_back();
    }
}

void Object3D::Draw() {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    // --- 追加: エディタでの変更を即時反映させるため、Draw直前にもマテリアルとワールド行列を更新 ---
    *mappedMaterial_ = material_;

    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 nodeMatrix = model_->GetModelData().rootNode.localMatrix;
    Matrix4x4 finalWorldMatrix = nodeMatrix * worldMatrix;

    mappedTransform_->World = finalWorldMatrix;
    mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(finalWorldMatrix));
    // -------------------------------------------------------------------------------------------------

    // 描画直前に最新のカメラ行列でWVPを再計算してGPUに送る（停止中のデバッグカメラ追従のため）
    CameraManager *cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();
    mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(mappedTransform_->World, viewMatrix), projectionMatrix);

    auto dxCommon = DirectXCommon::GetInstance();

    // Ensure common model root signature is bound
    commandList->SetGraphicsRootSignature(dxCommon->GetRootSignature());

    // ==============================================================
    // ★ 0. トレイル (残像) の描画 (設定されている場合のみ)
    // ==============================================================
    if (showTrail_ && model_) {
        if (!trailHistory_.empty() && trailLength_ > 0 && trailStep_ > 0) {
            
            // トレイル用のブレンドモード設定
            if (trailBlendMode_ == BlendMode::kBlendModeAdd) {
                commandList->SetPipelineState(isDoubleSided_ ? dxCommon->GetGraphicsPipelineStateNoCullAdditive() : dxCommon->GetGraphicsPipelineStateAdditive());
            } else {
                commandList->SetPipelineState(isDoubleSided_ ? dxCommon->GetGraphicsPipelineStateNoCull() : dxCommon->GetGraphicsPipelineState());
            }

            int trailCount = 0;
            uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
            uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

            Matrix4x4 nodeMatrix = model_->GetModelData().rootNode.localMatrix;

            size_t maxTrails = (std::min)(trailHistory_.size(), (size_t)trailLength_);
            for (size_t i = 0; i < maxTrails; i += trailStep_) {
                if (trailCount >= kMaxTrails) break;
                
                Vector3 pastPos = trailHistory_[i].translate;
                Vector3 pastRot = trailHistory_[i].rotate;
                Vector3 pastScale = trailHistory_[i].scale;
                
                // --- WorldMatrix 計算 ---
                Matrix4x4 tWorldMatrix = TransformFunctions::MakeAffineMatrix(pastScale, pastRot, pastPos);
                Matrix4x4 finalWorldMatrix = nodeMatrix * tWorldMatrix;

                // --- CBV に書き込み ---
                TransformMatrix* tMat = reinterpret_cast<TransformMatrix*>(mappedTrailTransform_ + transformSize * trailCount);
                tMat->World = finalWorldMatrix;
                tMat->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(finalWorldMatrix));
                tMat->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(finalWorldMatrix, viewMatrix), projectionMatrix);

                Material* mMat = reinterpret_cast<Material*>(mappedTrailMaterial_ + materialSize * trailCount);
                *mMat = material_;
                // 過去に行くほどアルファ値を下げるか
                float alphaFactor = 1.0f;
                if (trailFadeOut_) {
                    alphaFactor = 1.0f - (static_cast<float>(i) / static_cast<float>(maxTrails));
                }
                mMat->color.w = material_.color.w * trailStartAlpha_ * alphaFactor;

                // --- 描画 ---
                commandList->SetGraphicsRootConstantBufferView(1, trailTransformResource_->GetGPUVirtualAddress() + transformSize * trailCount);
                commandList->SetGraphicsRootConstantBufferView(0, trailMaterialResource_->GetGPUVirtualAddress() + materialSize * trailCount);
                commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
                if (sEnvironmentMapHandle.ptr != 0) {
                    commandList->SetGraphicsRootDescriptorTable(7, sEnvironmentMapHandle);
                }
                model_->Draw();

                trailCount++;
            }
        }
    }

    // ==============================================================
    // ★ 1. 先に Main drawing pass (本体のメッシュ) を描画する！
    // ==============================================================
    if (blendMode_ == BlendMode::kBlendModeAdd) {
        if (isDoubleSided_) {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCullAdditive());
        } else {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateAdditive());
        }
    } else {
        // 通常合成 (kBlendModeNormal)
        // アルファ値が1.0未満、または意図的に半透明として扱う場合はデプス書き込みなしのパイプラインを使う
        if (material_.color.w < 1.0f) {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateTransparent());
        } else {
            if (isDoubleSided_) {
                commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCull());
            } else {
                commandList->SetPipelineState(dxCommon->GetGraphicsPipelineState());
            }
        }
    }

    // 💡 スロット1が行列、スロット0がマテリアルが正解です！
    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress()); // 行列 (スロット1)
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());  // マテリアル (スロット0)
    // カメラの定数バッファをセット (スロット3)
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());

    // 環境マップをスロット7にセット
    if (sEnvironmentMapHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, sEnvironmentMapHandle);
    }

    // 本体の描画
    model_->Draw();

    // ==============================================================
    // ★ 2. 後から Outline drawing pass (ワイヤーフレーム) を重ねる！
    // ==============================================================
    if (dxCommon->IsOutlineEnabled() && material_.color.w >= 1.0f && blendMode_ != BlendMode::kBlendModeAdd) {
        commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateOutline());
        commandList->SetGraphicsRootConstantBufferView(8, dxCommon->GetOutlineParamsGPUAddress());
        
        // ※ルートパラメータ（行列やマテリアル）は本体描画時にセット済みなのでそのまま使えます
        
        // ワイヤーフレームの描画
        model_->Draw();
    }
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
        if (ImGui::TreeNode("Trail (軌跡・残像)")) {
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