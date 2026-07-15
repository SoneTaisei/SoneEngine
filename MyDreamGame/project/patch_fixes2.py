import os

# 1. DirectXCommon.h
h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.h'
with open(h_path, 'r', encoding='utf-8') as f:
    content = f.read()

if 'GetSkinningRootSignature' not in content:
    content = content.replace(
        'ID3D12PipelineState* GetGraphicsPipelineState() const { return graphicsPipelineState_.Get(); }',
        'ID3D12PipelineState* GetGraphicsPipelineState() const { return graphicsPipelineState_.Get(); }\n    ID3D12RootSignature* GetSkinningRootSignature() const { return skinningRootSignature_.Get(); }\n    ID3D12PipelineState* GetSkinningPipelineState() const { return skinningPipelineState_.Get(); }'
    )
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write(content)

# 2. DirectXCommon.cpp
cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Fix the InitAsDescriptorTable error
content = content.replace(
    'skinningRootParameters[6].InitAsDescriptorTable(1, &paletteRange, D3D12_SHADER_VISIBILITY_VERTEX); // t1 (Palette)',
    '''skinningRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    skinningRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    skinningRootParameters[6].DescriptorTable.pDescriptorRanges = &paletteRange;
    skinningRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;'''
)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Fixed remaining build errors.")
