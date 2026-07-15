import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

srv_code = '''
    // SRVの生成
    uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
    skinCluster.paletteSrvHandle.first = SrvManager::GetInstance()->GetCpuHandle(srvIndex);
    skinCluster.paletteSrvHandle.second = SrvManager::GetInstance()->GetGpuHandle(srvIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
    srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &srvDesc, skinCluster.paletteSrvHandle.first);
'''

# Replace the comment part with actual code
if 'uint32_t srvIndex = SrvManager::GetInstance()->Allocate();' not in content:
    content = content.replace(
        '// SrvManagerを使って場合によっては引数でもらうか、GetInstance()を呼ぶ',
        srv_code
    )
    # include SrvManager
    if '#include "Renderer/SrvManager.h"' not in content:
        content = '#include "Renderer/SrvManager.h"\n' + content
        
    with open(cpp_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added SRV creation to CreateSkinCluster")
else:
    print("SRV creation already exists.")
