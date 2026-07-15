import os
import re

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/Renderer.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Make sure AnimatorComponent is included
if '#include "Component/AnimatorComponent.h"' not in content:
    content = content.replace(
        '#include "Renderer.h"',
        '#include "Renderer.h"\n#include "Component/AnimatorComponent.h"'
    )

# The logic replacement
old_draw_logic = '''    commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

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

    if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
    }

    comp->GetModel()->Draw();

    if (dxCommon_->IsOutlineEnabled() && comp->GetMaterial().color.w >= 1.0f && comp->GetBlendMode() != BlendMode::kBlendModeAdd) {
        commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateOutline());
        commandList->SetGraphicsRootConstantBufferView(8, dxCommon_->GetOutlineParamsGPUAddress());
        comp->GetModel()->Draw();
    }'''

new_draw_logic = '''    AnimatorComponent* animator = comp->GetGameObject()->GetComponent<AnimatorComponent>();
    bool useSkinning = (animator != nullptr && animator->HasSkeleton());
    
    if (useSkinning) {
        commandList->SetGraphicsRootSignature(dxCommon_->GetSkinningRootSignature());
        commandList->SetPipelineState(dxCommon_->GetSkinningPipelineState());
        
        commandList->SetGraphicsRootConstantBufferView(0, comp->GetTransformResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, comp->GetMaterialResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
        
        // Skinning Palette setup
        const SkinCluster& skinCluster = animator->GetSkinCluster();
        commandList->SetGraphicsRootDescriptorTable(6, skinCluster.paletteSrvHandle.second);
        
        if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
            // Note: Skinning shader might not use environment map in exactly the same way, but keeping it if needed
            // Currently SkinningRootSignature doesn't have it mapped to 7, but let's ignore it for skinning or map it correctly later
        }

        comp->GetModel()->Draw(&skinCluster.influenceBufferView);
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

        if (Object3D::GetEnvironmentMapHandle().ptr != 0) {
            commandList->SetGraphicsRootDescriptorTable(7, Object3D::GetEnvironmentMapHandle());
        }

        comp->GetModel()->Draw();

        if (dxCommon_->IsOutlineEnabled() && comp->GetMaterial().color.w >= 1.0f && comp->GetBlendMode() != BlendMode::kBlendModeAdd) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateOutline());
            commandList->SetGraphicsRootConstantBufferView(8, dxCommon_->GetOutlineParamsGPUAddress());
            comp->GetModel()->Draw();
        }
    }'''

content = content.replace(old_draw_logic, new_draw_logic)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Added Skinning support to Renderer::DrawMeshRendererComponent")
