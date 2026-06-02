#include "PrimitiveObject.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "GameObject/Object3D.h"
#include "../externals/imgui/imgui.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Game2D/ReplayManager.h"
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

    // Trail resources
    uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
    uint32_t materialSize = (sizeof(Material) + 255) & ~255u;
    
    trailTransformResource_ = CreateBufferResource(device, transformSize * kMaxTrails);
    trailTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTrailTransform_));
    
    trailMaterialResource_ = CreateBufferResource(device, materialSize * kMaxTrails);
    trailMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedTrailMaterial_));
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
    // ★ 0. トレイル (残像) の描画 (設定されている場合のみ)
    // ==============================================================
    if (showReplayTrail_ && primitive_) {
        auto replayMgr = ReplayManager::GetInstance();
        const auto& replay = replayMgr->GetCurrentReplay();
        if (replay.totalFrames > 0 && trailLength_ > 0 && trailStep_ > 0) {
            
            // トレイル用のブレンドモード設定
            if (trailBlendMode_ == BlendMode::kBlendModeAdd) {
                commandList->SetPipelineState(isDoubleSided_ ? dxCommon->GetGraphicsPipelineStateNoCullAdditive() : dxCommon->GetGraphicsPipelineStateAdditive());
            } else {
                commandList->SetPipelineState(isDoubleSided_ ? dxCommon->GetGraphicsPipelineStateNoCull() : dxCommon->GetGraphicsPipelineState());
            }

            int curFrame = replayMgr->GetCurrentFrame();
            int startFrame = (std::max)(0, curFrame - trailLength_);
            
            int trailCount = 0;
            uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
            uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

            for (int f = curFrame - trailStep_; f >= startFrame; f -= trailStep_) {
                if (trailCount >= kMaxTrails) break;
                
                Vector3 pastPos = replay.frames[f].position;
                
                // --- WorldMatrix 計算 ---
                Matrix4x4 translateMat = TransformFunctions::MakeTranslateMatrix(pastPos);
                Matrix4x4 tWorldMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMat);
                if (parent_) {
                    tWorldMatrix = TransformFunctions::Multiply(tWorldMatrix, parent_->GetWorldMatrix());
                }

                // --- CBV に書き込み ---
                TransformMatrix* tMat = reinterpret_cast<TransformMatrix*>(mappedTrailTransform_ + transformSize * trailCount);
                tMat->World = tWorldMatrix;
                tMat->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(tWorldMatrix));
                tMat->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(tWorldMatrix, viewMatrix), projectionMatrix);

                Material* mMat = reinterpret_cast<Material*>(mappedTrailMaterial_ + materialSize * trailCount);
                *mMat = material_;
                // 過去に行くほどアルファ値を下げる
                float alphaFactor = 1.0f;
                if (curFrame > startFrame) {
                    alphaFactor = static_cast<float>(f - startFrame) / static_cast<float>(curFrame - startFrame);
                }
                mMat->color.w = material_.color.w * trailStartAlpha_ * alphaFactor;

                // --- 描画 ---
                commandList->SetGraphicsRootConstantBufferView(1, trailTransformResource_->GetGPUVirtualAddress() + transformSize * trailCount);
                commandList->SetGraphicsRootConstantBufferView(0, trailMaterialResource_->GetGPUVirtualAddress() + materialSize * trailCount);
                commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
                if (activeTexture.ptr != 0) {
                    commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
                }
                if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
                    commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
                }
                primitive_->Draw(commandList);

                trailCount++;
            }
        }
    }

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
        if (isDoubleSided_) {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineStateNoCull());
        } else {
            commandList->SetPipelineState(dxCommon->GetGraphicsPipelineState());
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
        if (ImGui::TreeNode("Replay Trail (残像)")) {
            ImGui::Checkbox("Show Trail", &showReplayTrail_);
            if (showReplayTrail_) {
                ImGui::DragInt("Trail Step", &trailStep_, 1, 1, 60);
                ImGui::DragInt("Trail Length", &trailLength_, 1, 1, 1000);
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
