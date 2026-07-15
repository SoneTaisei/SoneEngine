import os

dx_h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.h'
with open(dx_h_path, 'r', encoding='utf-8') as f:
    content = f.read()

if 'ID3D12PipelineState* GetSkinningPipelineState() const' not in content:
    content = content.replace(
        'ID3D12PipelineState* GetGraphicsPipelineState() const { return graphicsPipelineState_.Get(); }',
        'ID3D12PipelineState* GetGraphicsPipelineState() const { return graphicsPipelineState_.Get(); }\n    ID3D12RootSignature* GetSkinningRootSignature() const { return skinningRootSignature_.Get(); }\n    ID3D12PipelineState* GetSkinningPipelineState() const { return skinningPipelineState_.Get(); }'
    )
    with open(dx_h_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added GetSkinning getters to DirectXCommon.h")
