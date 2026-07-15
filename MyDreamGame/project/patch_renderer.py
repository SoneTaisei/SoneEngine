import os
import re

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/Renderer.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Includes
if '#include "Component/AnimatorComponent.h"' not in content:
    content = content.replace(
        '#include "Renderer.h"',
        '#include "Renderer.h"\n#include "Component/AnimatorComponent.h"'
    )

# DrawMeshRendererComponent
draw_mesh_start = 'void Renderer::DrawMeshRendererComponent(MeshRendererComponent* comp) {'
# Find the start of rendering block
search_str = r'''    if (comp->GetBlendMode() == BlendMode::kBlendModeNone) {
        if (comp->GetMaterial().color.w < 1.0f) {
            commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateTransparent());
        } else {
            if (comp->IsDoubleSided()) {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineStateNoCull());
            } else {
                commandList->SetPipelineState(dxCommon_->GetGraphicsPipelineState());
            }
        }
    }'''

# 現状のコードを見ると BlendMode 関連のコードがもう少し前にあるかもしれない。
# さきほどの Select-String 結果を見ると、BlendMode の分岐があることがわかる。
# 少し荒い置換になるため、正規表現で置き換える。
# Renderer.cpp 内の oid Renderer::DrawMeshRendererComponent を見つけて修正する。
