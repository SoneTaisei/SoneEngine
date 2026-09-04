#include "Renderer.h"
#include "Component/AnimatorComponent.h"
#include "DirectXCommon/DirectXCommon.h"
#include "Resource/Primitive/Primitive.h"
#include "Renderer.h"
#include "Component/AnimatorComponent.h"
#include "DirectXCommon/DirectXCommon.h"
#include "Resource/Primitive/Primitive.h"
#include "Resource/Sprite/Sprite.h"
#include "Resource/Model/Model.h"
#include "Effect/ParticleManager.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "GameObject/Object3D.h"
#include "Component/MeshRendererComponent.h"
#include "Component/PrimitiveRendererComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject/GameObject.h"
#include "Graphics/CameraManager.h"
#include "Core/Utility/TransformFunctions.h"
#include <algorithm>

Renderer* Renderer::GetInstance() {
    static Renderer instance;
    return &instance;
}

void Renderer::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
}

void Renderer::PreDraw() {
    if (dxCommon_) {
        dxCommon_->PreDraw();
    }
}

void Renderer::PostDraw() {
    if (dxCommon_) {
        dxCommon_->PostDraw();
    }
}

void Renderer::BeginShadowPass(const Matrix4x4& lightViewProj) {
    if (!dxCommon_) return;
    auto commandList = dxCommon_->GetCommandList();
    if (!commandList) return;

    isShadowPass_ = true;

    // ライトビュー射影行列を定数バッファに書き込む
    dxCommon_->SetShadowLightViewProjection(lightViewProj);

    // リソースバリア: シャドウマップリソースを PIXEL_SHADER_RESOURCE から DEPTH_WRITE へ遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dxCommon_->GetShadowMapResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    commandList->ResourceBarrier(1, &barrier);

    // レンダーターゲットを解除し、DSVにシャドウマップを設定
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetShadowMapDsvCPUHandle();
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

    // 深度バッファをクリア (1.0f)
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // ビューポートとシザー矩形をシャドウマップサイズ (2048x2048) に設定
    const D3D12_VIEWPORT& vp = dxCommon_->GetShadowViewport();
    const D3D12_RECT& sc = dxCommon_->GetShadowScissorRect();
    commandList->RSSetViewports(1, &vp);
    commandList->RSSetScissorRects(1, &sc);

    // シャドウ用RootSignatureとトポロジの設定
    commandList->SetGraphicsRootSignature(dxCommon_->GetShadowMapRootSignature());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // シャドウ用グローバル定数バッファ (LightViewProj) をルートパラメータ1にバインド
    commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
}

void Renderer::EndShadowPass() {
    if (!dxCommon_) return;
    auto commandList = dxCommon_->GetCommandList();
    if (!commandList) return;

    // リソースバリア: シャドウマップリソースを DEPTH_WRITE から PIXEL_SHADER_RESOURCE へ遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dxCommon_->GetShadowMapResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    isShadowPass_ = false;

    // メインの描画先（RenderTextureとメインDSV、ビューポート、シザー矩形）に復帰
    dxCommon_->RestoreMainRenderTarget();
}

void Renderer::DrawPrimitive(Primitive* primitive) {
    if (primitive && dxCommon_) {
        primitive->Draw();
    }
}

#include "Resource/Sprite/SpriteCommon.h"
#include "Graphics/TextureManager.h"
#include "Effect/ParticleCommon.h"

void Renderer::DrawModel(Model* model) {
    if (model && dxCommon_) {
        model->Draw();
    }
}

void Renderer::DrawSprite(Sprite* sprite) {
    if (!sprite || !dxCommon_ || !sprite->spriteCommon_) return;

    Matrix4x4 uvTransform = TransformFunctions::MakeIdentity4x4();
    if(sprite->isCutMode_) {
        uvTransform = TransformFunctions::MakeAffineMatrix(
            { sprite->texSize_.x / sprite->texBaseSize_.x, sprite->texSize_.y / sprite->texBaseSize_.y, 1.0f },
            { 0.0f, 0.0f, 0.0f },
            { sprite->texPos_.x / sprite->texBaseSize_.x, sprite->texPos_.y / sprite->texBaseSize_.y, 0.0f }
        );
    }
    sprite->materialData_->uvTransform = uvTransform;

    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(
        sprite->transform_.scale, sprite->transform_.rotate, sprite->transform_.translate
    );
    Matrix4x4 viewMatrix = sprite->spriteCommon_->GetViewMatrix();
    Matrix4x4 projectionMatrix = sprite->spriteCommon_->GetProjectionMatrix();

    TransformMatrix transformMatrixData;
    transformMatrixData.WVP = TransformFunctions::Multiply(worldMatrix, TransformFunctions::Multiply(viewMatrix, projectionMatrix));
    transformMatrixData.World = worldMatrix;

    if (sprite->mappedTransform_) {
        *sprite->mappedTransform_ = transformMatrixData;
    }

    auto commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootConstantBufferView(0, sprite->materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, sprite->transformResource_->GetGPUVirtualAddress());
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetGpuHandle(sprite->textureIndex_);
    commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

    commandList->DrawIndexedInstanced(sprite->spriteCommon_->GetIndexCount(), 1, 0, 0, 0);
}

void Renderer::DrawParticle(ParticleManager* particleManager, const Matrix4x4& viewProjection) {
    if (!particleManager || !dxCommon_ || !particleManager->particleCommon_) return;

    particleManager->TransferToGPU(viewProjection);
    particleManager->particleCommon_->SetBlendMode(kBlendModeAdd);

    auto commandList = dxCommon_->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &particleManager->particleCommon_->GetVertexBufferView());
    commandList->SetGraphicsRootConstantBufferView(0, particleManager->materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, particleManager->instancingSrvHandleGPU_);
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGpuHandle(particleManager->textureIndex_));

    if(particleManager->numActiveParticles_ > 0) {
        commandList->DrawInstanced(particleManager->particleCommon_->GetVertexCount(), particleManager->numActiveParticles_, 0, 0);
    }
}

void Renderer::DrawObject3D(Object3D* obj) {
    if (!obj || !obj->model_ || !dxCommon_) return;

    auto commandList = dxCommon_->GetCommandList();
    *obj->mappedMaterial_ = obj->material_;

    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(obj->transform_.scale, obj->transform_.rotate, obj->transform_.translate);
    Matrix4x4 finalWorldMatrix = worldMatrix;

    obj->mappedTransform_->World = finalWorldMatrix;
    obj->mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(finalWorldMatrix));

    // シャドウパス時は深度専用パイプラインで高速描画
    if (isShadowPass_) {
        if (obj->material_.color.w <= 0.0f) return;

        AnimatorComponent* animator = obj->GetAnimator();
        bool useSkinning = (animator != nullptr && animator->HasSkeleton());

        if (useSkinning) {
            commandList->SetPipelineState(dxCommon_->GetShadowMapSkinningPipelineState());
            commandList->SetGraphicsRootConstantBufferView(0, obj->transformResource_->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
            const SkinCluster& skinCluster = animator->GetSkinCluster();
            commandList->SetGraphicsRootDescriptorTable(2, skinCluster.paletteSrvHandle.second);
            obj->model_->Draw(&skinCluster.influenceBufferView, obj->GetTextureHandle());
        } else {
            commandList->SetPipelineState(dxCommon_->GetShadowMapPipelineState());
            commandList->SetGraphicsRootConstantBufferView(0, obj->transformResource_->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
            obj->model_->Draw(nullptr, obj->GetTextureHandle());
        }
        return;
    }

    CameraManager *cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();
    obj->mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(obj->mappedTransform_->World, viewMatrix), projectionMatrix);

    // Removed static RootSignature set to allow skinning signature to be bound dynamically below.

    if (obj->showTrail_ && obj->model_) {
        if (!obj->trailHistory_.empty() && obj->trailLength_ > 0 && obj->trailStep_ > 0) {
            if (obj->trailBlendMode_ == BlendMode::kBlendModeAdd) {
                commandList->SetPipelineState(obj->isDoubleSided_ ? dxCommon_->GetGraphicsPipelineStateNoCullAdditive() : dxCommon_->GetGraphicsPipelineStateAdditive());
            } else {
                commandList->SetPipelineState(obj->isDoubleSided_ ? dxCommon_->GetGraphicsPipelineStateNoCull() : dxCommon_->GetGraphicsPipelineState());
            }

            int trailCount = 0;
            uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
            uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

            size_t maxTrails = (std::min)(obj->trailHistory_.size(), (size_t)obj->trailLength_);
            for (size_t i = 0; i < maxTrails; i += obj->trailStep_) {
                if (trailCount >= Object3D::kMaxTrails) break;
                
                Vector3 pastPos = obj->trailHistory_[i].translate;
                Vector3 pastRot = obj->trailHistory_[i].rotate;
                Vector3 pastScale = obj->trailHistory_[i].scale;
                
                Matrix4x4 tWorldMatrix = TransformFunctions::MakeAffineMatrix(pastScale, pastRot, pastPos);
                Matrix4x4 tFinalWorldMatrix = tWorldMatrix;

                TransformMatrix* tMat = reinterpret_cast<TransformMatrix*>(obj->mappedTrailTransform_ + transformSize * trailCount);
                tMat->World = tFinalWorldMatrix;
                tMat->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(tFinalWorldMatrix));
                tMat->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(tFinalWorldMatrix, viewMatrix), projectionMatrix);

                Material* mMat = reinterpret_cast<Material*>(obj->mappedTrailMaterial_ + materialSize * trailCount);
                *mMat = obj->material_;
                float alphaFactor = 1.0f;
                if (obj->trailFadeOut_) {
                    alphaFactor = 1.0f - (static_cast<float>(i) / static_cast<float>(maxTrails));
                }
                mMat->color.w = obj->material_.color.w * obj->trailStartAlpha_ * alphaFactor;

                commandList->SetGraphicsRootConstantBufferView(1, obj->trailTransformResource_->GetGPUVirtualAddress() + transformSize * trailCount);
                commandList->SetGraphicsRootConstantBufferView(0, obj->trailMaterialResource_->GetGPUVirtualAddress() + materialSize * trailCount);
                commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
                if (Object3D::sEnvironmentMapHandle.ptr != 0) {
                    commandList->SetGraphicsRootDescriptorTable(7, Object3D::sEnvironmentMapHandle);
                }
                obj->model_->Draw(nullptr, obj->GetTextureHandle());

                trailCount++;
            }
        }
    }

    AnimatorComponent* animator = obj->GetAnimator();
    bool useSkinning = (animator != nullptr && animator->HasSkeleton());

    if (useSkinning) {
        commandList->SetGraphicsRootSignature(dxCommon_->GetSkinningRootSignature());
        commandList->SetPipelineState(dxCommon_->GetSkinningPipelineState());
    } else {
        commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());
        if (obj->blendMode_ == BlendMode::kBlendModeAdd) {
            if (obj->isDoubleSided_) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCullAdditive());
            } else {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateAdditive());
            }
        } else {
            if (obj->material_.color.w < 1.0f) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateTransparent());
            } else {
                if (obj->isDoubleSided_) {
                    commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCull());
                } else {
                    commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineState());
                }
            }
        }
    }

    commandList->SetGraphicsRootConstantBufferView(1, obj->transformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(0, obj->materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());

    if (ModelCommon* mc = obj->model_->GetModelCommon()) {
        if (auto addr = mc->GetDirectionalLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(4, addr);
        if (auto addr = mc->GetPointLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(5, addr);
        if (auto addr = mc->GetSpotLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(6, addr);
    }

    if (Object3D::sEnvironmentMapHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::sEnvironmentMapHandle);
    }

    // 9: ShadowMap SRV
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv = dxCommon_->GetShadowMapSrvHandleGPU();
    if (shadowSrv.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(9, shadowSrv);
    }

    if (useSkinning) {
        const SkinCluster& skinCluster = animator->GetSkinCluster();
        commandList->SetGraphicsRootDescriptorTable(10, skinCluster.paletteSrvHandle.second);
        obj->model_->Draw(&skinCluster.influenceBufferView, obj->GetTextureHandle());
    } else {
        obj->model_->Draw(nullptr, obj->GetTextureHandle());

        if (dxCommon_->IsOutlineEnabled() && obj->material_.color.w >= 1.0f && obj->blendMode_ != BlendMode::kBlendModeAdd) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateOutline());
            commandList->SetGraphicsRootConstantBufferView(8, dxCommon_->GetOutlineParamsGPUAddress());
            obj->model_->Draw(nullptr, obj->GetTextureHandle());
        }
    }
}

void Renderer::DrawPrimitiveObject(PrimitiveObject* obj) {
    if (!obj || !dxCommon_) return;

    auto commandList = dxCommon_->GetCommandList();
    *obj->mappedMaterial_ = obj->material_;

    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();
    if (obj->isBillboard_) {
        Matrix4x4 cameraMatrix = TransformFunctions::Inverse(viewMatrix);
        billboardMatrix = cameraMatrix;
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(obj->transform_.scale);
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(obj->transform_.rotate.x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(obj->transform_.rotate.y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(obj->transform_.rotate.z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(obj->transform_.translate);

    Matrix4x4 localMatrix;
    if (obj->isBillboard_) {
        localMatrix = TransformFunctions::Multiply(rotateMatrix, billboardMatrix);
    } else {
        localMatrix = rotateMatrix;
    }
    
    obj->worldMatrix_ = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMatrix);
    if (obj->parent_) {
        obj->worldMatrix_ = TransformFunctions::Multiply(obj->worldMatrix_, obj->parent_->GetWorldMatrix());
    }
    
    obj->mappedTransform_->World = obj->worldMatrix_;
    obj->mappedTransform_->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(obj->worldMatrix_));

    // シャドウパス時は深度専用パイプラインで描画
    if (isShadowPass_) {
        if (obj->material_.color.w <= 0.0f) return;
        commandList->SetPipelineState(dxCommon_->GetShadowMapPipelineState());
        commandList->SetGraphicsRootConstantBufferView(0, obj->transformResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
        if (obj->primitive_) {
            obj->primitive_->Draw();
        }
        return;
    }

    obj->mappedTransform_->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(obj->worldMatrix_, viewMatrix), projectionMatrix);

    D3D12_GPU_DESCRIPTOR_HANDLE activeTexture = obj->textureHandle_;
    if (activeTexture.ptr == 0) {
        activeTexture = PrimitiveObject::sDefaultTextureHandle_;
    }

    commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

    if (obj->blendMode_ == BlendMode::kBlendModeAdd) {
        if (obj->isDoubleSided_) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCullAdditive());
        } else {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateAdditive());
        }
    } else {
        if (obj->material_.color.w < 1.0f) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateTransparent());
        } else {
            if (obj->isDoubleSided_) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCull());
            } else {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineState());
            }
        }
    }

    commandList->SetGraphicsRootConstantBufferView(1, obj->transformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(0, obj->materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
    
    if (activeTexture.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
    }

    if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
    }

    // 9: ShadowMap SRV
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv = dxCommon_->GetShadowMapSrvHandleGPU();
    if (shadowSrv.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(9, shadowSrv);
    }

    if (obj->primitive_) {
        obj->primitive_->Draw();
    }

    if (dxCommon_->IsOutlineEnabled() && obj->material_.color.w >= 1.0f && obj->blendMode_ != BlendMode::kBlendModeAdd) {
        commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateOutline());
        commandList->SetGraphicsRootConstantBufferView(8, dxCommon_->GetOutlineParamsGPUAddress());
        
        if (obj->primitive_) {
            obj->primitive_->Draw();
        }
    }
}

void Renderer::DrawPrimitiveGhost(PrimitiveObject* obj, const EulerTransform& transform, const Material& material) {
    if (!obj || !dxCommon_ || !obj->primitive_) return;
    if (obj->currentGhostIndex_ >= PrimitiveObject::kMaxGhosts) return;

    auto commandList = dxCommon_->GetCommandList();
    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();
    if (obj->isBillboard_) {
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
    if (obj->isBillboard_) {
        localMatrix = TransformFunctions::Multiply(rotateMatrix, billboardMatrix);
    } else {
        localMatrix = rotateMatrix;
    }
    
    Matrix4x4 worldMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMatrix);
    if (obj->parent_) {
        worldMatrix = TransformFunctions::Multiply(worldMatrix, obj->parent_->GetWorldMatrix());
    }

    uint32_t transformSize = (sizeof(TransformMatrix) + 255) & ~255u;
    uint32_t materialSize = (sizeof(Material) + 255) & ~255u;

    TransformMatrix* tMat = reinterpret_cast<TransformMatrix*>(obj->mappedGhostTransform_ + transformSize * obj->currentGhostIndex_);
    tMat->World = worldMatrix;
    tMat->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix));
    tMat->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix, viewMatrix), projectionMatrix);

    Material* mMat = reinterpret_cast<Material*>(obj->mappedGhostMaterial_ + materialSize * obj->currentGhostIndex_);
    *mMat = material;

    commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

    if (obj->blendMode_ == BlendMode::kBlendModeAdd) {
        if (obj->isDoubleSided_) commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCullAdditive());
        else commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateAdditive());
    } else {
        if (material.color.w < 1.0f) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateTransparent());
        } else {
            if (obj->isDoubleSided_) commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCull());
            else commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineState());
        }
    }

    commandList->SetGraphicsRootConstantBufferView(1, obj->ghostTransformResource_->GetGPUVirtualAddress() + transformSize * obj->currentGhostIndex_);
    commandList->SetGraphicsRootConstantBufferView(0, obj->ghostMaterialResource_->GetGPUVirtualAddress() + materialSize * obj->currentGhostIndex_);
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
    
    D3D12_GPU_DESCRIPTOR_HANDLE activeTexture = obj->textureHandle_;
    if (activeTexture.ptr == 0) activeTexture = PrimitiveObject::sDefaultTextureHandle_;
    if (activeTexture.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
    }

    if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
    }

    obj->primitive_->Draw();

    obj->currentGhostIndex_++;
}

void Renderer::AddMeshComponent(MeshRendererComponent* comp) {
    if (comp) meshComponents_.push_back(comp);
}

void Renderer::AddPrimitiveComponent(PrimitiveRendererComponent* comp) {
    if (comp) primitiveComponents_.push_back(comp);
}

void Renderer::RenderComponents() {
    for (auto* comp : meshComponents_) {
        DrawMeshRendererComponent(comp);
    }
    meshComponents_.clear();

    for (auto* comp : primitiveComponents_) {
        DrawPrimitiveRendererComponent(comp);
    }
    primitiveComponents_.clear();
}

void Renderer::DrawMeshRendererComponent(MeshRendererComponent* comp) {
    if (!comp || !dxCommon_ || !comp->GetModel()) return;
    
    auto commandList = dxCommon_->GetCommandList();
    TransformComponent* tc = comp->GetGameObject()->GetComponent<TransformComponent>();
    if (!tc) return;

    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(tc->GetScale());
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(tc->GetRotation().x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(tc->GetRotation().y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(tc->GetRotation().z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(tc->GetPosition());

    Matrix4x4 worldMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    // TODO: Parent logic if needed

    TransformMatrix* mappedTransform = comp->GetMappedTransform();
    mappedTransform->World = worldMatrix;
    mappedTransform->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix));

    // シャドウパス時は深度専用パイプラインで描画
    if (isShadowPass_) {
        if (comp->GetMaterial().color.w <= 0.0f) return;
        AnimatorComponent* animator = comp->GetGameObject()->GetComponent<AnimatorComponent>();
        bool useSkinning = (animator != nullptr && animator->HasSkeleton());
        if (useSkinning) {
            commandList->SetPipelineState(dxCommon_->GetShadowMapSkinningPipelineState());
            commandList->SetGraphicsRootConstantBufferView(0, comp->GetTransformResource()->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
            const SkinCluster& skinCluster = animator->GetSkinCluster();
            commandList->SetGraphicsRootDescriptorTable(2, skinCluster.paletteSrvHandle.second);
            comp->GetModel()->Draw(&skinCluster.influenceBufferView, comp->GetTextureHandle());
        } else {
            commandList->SetPipelineState(dxCommon_->GetShadowMapPipelineState());
            commandList->SetGraphicsRootConstantBufferView(0, comp->GetTransformResource()->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
            comp->GetModel()->Draw(nullptr, comp->GetTextureHandle());
        }
        return;
    }

    mappedTransform->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix, viewMatrix), projectionMatrix);

    AnimatorComponent* animator = comp->GetGameObject()->GetComponent<AnimatorComponent>();
    bool useSkinning = (animator != nullptr && animator->HasSkeleton());
    
    if (useSkinning) {
        commandList->SetGraphicsRootSignature(dxCommon_->GetSkinningRootSignature());
        commandList->SetPipelineState(dxCommon_->GetSkinningPipelineState());
        
        commandList->SetGraphicsRootConstantBufferView(1, comp->GetTransformResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(0, comp->GetMaterialResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
        
        if (ModelCommon* mc = comp->GetModel()->GetModelCommon()) {
            if (auto addr = mc->GetDirectionalLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(4, addr);
            if (auto addr = mc->GetPointLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(5, addr);
            if (auto addr = mc->GetSpotLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(6, addr);
        }        
        if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
            commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
        }

        // 9: ShadowMap SRV
        D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv = dxCommon_->GetShadowMapSrvHandleGPU();
        if (shadowSrv.ptr != 0) {
            commandList->SetGraphicsRootDescriptorTable(9, shadowSrv);
        }
        
        // Skinning Palette setup at index 10
        const SkinCluster& skinCluster = animator->GetSkinCluster();
        commandList->SetGraphicsRootDescriptorTable(10, skinCluster.paletteSrvHandle.second);

        comp->GetModel()->Draw(&skinCluster.influenceBufferView, comp->GetTextureHandle());
    } else {
        commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

        if (comp->GetBlendMode() == BlendMode::kBlendModeAdd) {
            if (comp->IsDoubleSided()) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCullAdditive());
            } else {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateAdditive());
            }
        } else {
            if (comp->GetMaterial().color.w < 1.0f) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateTransparent());
            } else {
                if (comp->IsDoubleSided()) {
                    commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCull());
                } else {
                    commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineState());
                }
            }
        }

        commandList->SetGraphicsRootConstantBufferView(1, comp->GetTransformResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(0, comp->GetMaterialResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());

        if (ModelCommon* mc = comp->GetModel()->GetModelCommon()) {
            if (auto addr = mc->GetDirectionalLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(4, addr);
            if (auto addr = mc->GetPointLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(5, addr);
            if (auto addr = mc->GetSpotLightGPUAddress()) commandList->SetGraphicsRootConstantBufferView(6, addr);
        }

        if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
            commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
        }

        // 9: ShadowMap SRV
        D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv = dxCommon_->GetShadowMapSrvHandleGPU();
        if (shadowSrv.ptr != 0) {
            commandList->SetGraphicsRootDescriptorTable(9, shadowSrv);
        }

        comp->GetModel()->Draw(nullptr, comp->GetTextureHandle());

        if (dxCommon_->IsOutlineEnabled() && comp->GetMaterial().color.w >= 1.0f && comp->GetBlendMode() != BlendMode::kBlendModeAdd) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateOutline());
            commandList->SetGraphicsRootConstantBufferView(8, dxCommon_->GetOutlineParamsGPUAddress());
            comp->GetModel()->Draw(nullptr, comp->GetTextureHandle());
        }
    }
}

void Renderer::DrawPrimitiveRendererComponent(PrimitiveRendererComponent* comp) {
    if (!comp || !dxCommon_ || !comp->GetPrimitive()) return;

    auto commandList = dxCommon_->GetCommandList();
    TransformComponent* tc = comp->GetGameObject()->GetComponent<TransformComponent>();
    if (!tc) return;

    CameraManager* cameraMgr = CameraManager::GetInstance();
    Matrix4x4 viewMatrix = cameraMgr->GetViewMatrix();
    Matrix4x4 projectionMatrix = cameraMgr->GetProjectionMatrix();

    Matrix4x4 billboardMatrix = TransformFunctions::MakeIdentity4x4();
    if (comp->IsBillboard()) {
        Matrix4x4 cameraMatrix = TransformFunctions::Inverse(viewMatrix);
        billboardMatrix = cameraMatrix;
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    }

    Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(tc->GetScale());
    Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(tc->GetRotation().x);
    Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(tc->GetRotation().y);
    Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(tc->GetRotation().z);
    Matrix4x4 rotateMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
    Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(tc->GetPosition());

    Matrix4x4 localMatrix;
    if (comp->IsBillboard()) {
        localMatrix = TransformFunctions::Multiply(rotateMatrix, billboardMatrix);
    } else {
        localMatrix = rotateMatrix;
    }
    
    Matrix4x4 worldMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, localMatrix), translateMatrix);
    
    TransformMatrix* mappedTransform = comp->GetMappedTransform();
    mappedTransform->World = worldMatrix;
    mappedTransform->WorldInverseTranspose = TransformFunctions::Transpose(TransformFunctions::Inverse(worldMatrix));

    // シャドウパス時は深度専用パイプラインで描画
    if (isShadowPass_) {
        if (comp->GetMaterial().color.w <= 0.0f) return;
        commandList->SetPipelineState(dxCommon_->GetShadowMapPipelineState());
        commandList->SetGraphicsRootConstantBufferView(0, comp->GetTransformResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, dxCommon_->GetShadowGlobalGPUAddress());
        comp->GetPrimitive()->Draw();
        return;
    }

    mappedTransform->WVP = TransformFunctions::Multiply(TransformFunctions::Multiply(worldMatrix, viewMatrix), projectionMatrix);

    D3D12_GPU_DESCRIPTOR_HANDLE activeTexture = comp->GetTextureHandle();
    if (activeTexture.ptr == 0) {
        activeTexture = PrimitiveObject::sDefaultTextureHandle_;
    }

    commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

    if (comp->GetBlendMode() == BlendMode::kBlendModeAdd) {
        if (comp->IsDoubleSided()) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCullAdditive());
        } else {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateAdditive());
        }
    } else {
        if (comp->GetMaterial().color.w < 1.0f) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateTransparent());
        } else {
            if (comp->IsDoubleSided()) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCull());
            } else {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineState());
            }
        }
    }

    commandList->SetGraphicsRootConstantBufferView(1, comp->GetTransformResource()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(0, comp->GetMaterialResource()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
    
    if (activeTexture.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
    }

    if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
    }

    // 9: ShadowMap SRV
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv = dxCommon_->GetShadowMapSrvHandleGPU();
    if (shadowSrv.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(9, shadowSrv);
    }

    comp->GetPrimitive()->Draw();

    if (dxCommon_->IsOutlineEnabled() && comp->GetMaterial().color.w >= 1.0f && comp->GetBlendMode() != BlendMode::kBlendModeAdd) {
        commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateOutline());
        commandList->SetGraphicsRootConstantBufferView(8, dxCommon_->GetOutlineParamsGPUAddress());
        comp->GetPrimitive()->Draw();
    }
}
