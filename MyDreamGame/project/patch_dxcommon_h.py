import os

dx_h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.h'
with open(dx_h_path, 'r', encoding='utf-8') as f:
    content = f.read()

# スキンメッシュ用のメンバを追加
if 'Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;' not in content:
    content = content.replace(
        'Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;',
        'Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;\n    Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;\n    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPipelineState_;'
    )
    with open(dx_h_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added skinning members to DirectXCommon.h")
else:
    print("Skinning members already exist.")
