import os
import re

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/Renderer.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace DrawMeshRendererComponent rendering logic
# We need to find the section where SetGraphicsRootSignature is called, up to comp->GetModel()->Draw();
old_logic_start = 'commandList->SetGraphicsRootSignature(dxCommon_->GetRootSignature());'
old_logic_end = 'comp->GetModel()->Draw();'

new_logic = '''
    AnimatorComponent* animator = comp->GetGameObject()->GetComponent<AnimatorComponent>();
    bool useSkinning = (animator != nullptr && animator->HasSkeleton());
    
    if (useSkinning) {
        commandList->SetGraphicsRootSignature(dxCommon_->GetSkinningRootSignature());
        commandList->SetPipelineState(dxCommon_->GetSkinningPipelineState());
        
        // SRV Setup
        commandList->SetGraphicsRootConstantBufferView(1, comp->GetTransformResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(0, comp->GetMaterialResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(3, CameraManager::GetInstance()->GetCameraGPUAddress());
        
        // SrvManager を使って SRV が作られていないので、今回は Structs.h の SkinClusterData から StructuredBuffer を作って
        // ShaderResourceView を作成する処理が必要だが、実は CreateSkinCluster で paletteSrvHandle は SrvManager経由で作成されていない。
        // ここでエラーが起きる可能性があるため、対処が必要。
        // （そもそも SRV の作成処理が UtilityFunctions.cpp の CreateSkinCluster でコメントアウトされている）
'''

# SRV作成の対応が抜けていたので、一度 UtilityFunctions.cpp の CreateSkinCluster も直す必要がある。
print("Stop and fix UtilityFunctions.cpp CreateSkinCluster first.")
