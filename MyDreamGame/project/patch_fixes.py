import os
import re

# 1. DirectXCommon.cpp
cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Fix DescriptorTable init (it seems CD3DX12 is not fully used or mixed)
# Just use raw initialization
fix_root_param = '''    skinningRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    skinningRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    skinningRootParameters[6].DescriptorTable.pDescriptorRanges = &paletteRange;
    skinningRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;'''
content = re.sub(r'skinningRootParameters\[6\]\.InitAsDescriptorTable.*?;\s*', fix_root_param + '\n', content)

# Fix RootSignatureDesc init
fix_desc = '''    D3D12_ROOT_SIGNATURE_DESC skinningRootSignatureDesc{};
    skinningRootSignatureDesc.pParameters = skinningRootParameters;
    skinningRootSignatureDesc.NumParameters = _countof(skinningRootParameters);
    skinningRootSignatureDesc.pStaticSamplers = staticSamplers;
    skinningRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
    skinningRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;'''
content = re.sub(r'D3D12_ROOT_SIGNATURE_DESC skinningRootSignatureDesc\{\};\s*skinningRootSignatureDesc\.Init\([\s\S]*?\);', fix_desc, content)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 2. AnimatorComponent.cpp (::Update instead of Update)
cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()
content = content.replace('Update(skinCluster_, skeleton_);', '::Update(skinCluster_, skeleton_);')
with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 3. AnimatorComponent.h (HasSkeleton)
h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.h'
with open(h_path, 'r', encoding='utf-8') as f:
    content = f.read()
if 'bool HasSkeleton() const' not in content:
    content = content.replace('bool hasSkeleton_ = false;', 'bool hasSkeleton_ = false;\npublic:\n    bool HasSkeleton() const { return hasSkeleton_; }')
with open(h_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 4. DirectXCommon.h (Getters)
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

print("Fixed build errors.")
