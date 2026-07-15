import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.cpp'

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# t0 がある descriptorRange を探す。
# descriptorRange[0].BaseShaderRegister = 0; (t0)
# その後に t1, t2 を追加するか、別の DescriptorTable としてパラメータを追加する。
# 一番簡単なのは、Object3DのRootSignatureの pParameters を増やすこと。

# D3D12_ROOT_PARAMETER rootParameters[3] = {}; (b0, b1, t0) を探す
# これを [5] に増やし、t1, t2 用の DescriptorTable を追加する。
old_params = r'''
    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    // b0 (Transform用)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    // b1 (Material用)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 1; // ※PS側は b1
    // t0 (TextureCube用)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
'''

new_params = r'''
    // Palette用とInfluence用のDescriptorRange
    D3D12_DESCRIPTOR_RANGE descriptorRangePalette[1] = {};
    descriptorRangePalette[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangePalette[0].NumDescriptors = 1;
    descriptorRangePalette[0].BaseShaderRegister = 1; // t1
    descriptorRangePalette[0].RegisterSpace = 0;
    descriptorRangePalette[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRangeInfluence[1] = {};
    descriptorRangeInfluence[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeInfluence[0].NumDescriptors = 1;
    descriptorRangeInfluence[0].BaseShaderRegister = 2; // t2
    descriptorRangeInfluence[0].RegisterSpace = 0;
    descriptorRangeInfluence[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[5] = {};
    // b0 (Transform用)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    // b1 (Material用)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    // t0 (TextureCube用)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    
    // t1 (MatrixPalette)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangePalette;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    
    // t2 (VertexInfluence)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[4].DescriptorTable.pDescriptorRanges = descriptorRangeInfluence;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
'''

if 'D3D12_ROOT_PARAMETER rootParameters[3] = {};' in content:
    content = content.replace(old_params.strip(), new_params.strip())
    with open(cpp_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Updated RootSignature for Skinning")
else:
    print("Could not find the target code in DirectXCommon.cpp")
