import os

dx_cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/DirectXCommon/DirectXCommon.cpp'
with open(dx_cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 挿入位置を探す（graphicsPipelineState_作成の後あたり）
insert_pos_str = 'device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));'

skinning_code = r'''
    // =====================================================================================
    // スキンメッシュ用のパイプラインステート (skinningPipelineState_)
    // =====================================================================================

    // 1. RootSignature の拡張 (Palette用 t1 の追加)
    CD3DX12_DESCRIPTOR_RANGE paletteRange{};
    paletteRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

    CD3DX12_ROOT_PARAMETER skinningRootParameters[7] = {};
    skinningRootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);   // b0 (WVP)
    skinningRootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);    // b1 (Material)
    skinningRootParameters[2].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL); // t0 (Texture)
    skinningRootParameters[3].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);    // b2 (CameraForGPU)
    skinningRootParameters[4].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_PIXEL);    // b3 (DirectionalLight)
    skinningRootParameters[5].InitAsConstantBufferView(4, 0, D3D12_SHADER_VISIBILITY_PIXEL);    // b4 (PointLight)
    skinningRootParameters[6].InitAsDescriptorTable(1, &paletteRange, D3D12_SHADER_VISIBILITY_VERTEX); // t1 (Palette)

    D3D12_ROOT_SIGNATURE_DESC skinningRootSignatureDesc{};
    skinningRootSignatureDesc.Init(
        _countof(skinningRootParameters), skinningRootParameters,
        _countof(staticSamplers), staticSamplers,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> skinningSignatureBlob = nullptr;
    D3D12SerializeRootSignature(&skinningRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &skinningSignatureBlob, &errorBlob);
    device_->CreateRootSignature(0, skinningSignatureBlob->GetBufferPointer(), skinningSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&skinningRootSignature_));

    // 2. InputLayout の拡張 (WEIGHT, BONEINDICES の追加)
    D3D12_INPUT_ELEMENT_DESC skinningInputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_INPUT_LAYOUT_DESC skinningInputLayoutDesc{};
    skinningInputLayoutDesc.pInputElementDescs = skinningInputElementDescs;
    skinningInputLayoutDesc.NumElements = _countof(skinningInputElementDescs);

    // 3. Shader のコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> skinningVertexShaderBlob = CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
    // ピクセルシェーダは既存の Object3d.PS.hlsl を流用

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinningPipelineStateDesc = graphicsPipelineStateDesc;
    skinningPipelineStateDesc.pRootSignature = skinningRootSignature_.Get();
    skinningPipelineStateDesc.InputLayout = skinningInputLayoutDesc;
    skinningPipelineStateDesc.VS = { skinningVertexShaderBlob->GetBufferPointer(), skinningVertexShaderBlob->GetBufferSize() };

    device_->CreateGraphicsPipelineState(&skinningPipelineStateDesc, IID_PPV_ARGS(&skinningPipelineState_));
'''

if 'skinningRootSignature_' not in content:
    content = content.replace(insert_pos_str, insert_pos_str + '\n' + skinning_code)
    with open(dx_cpp_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added Skinning RootSignature and PSO to DirectXCommon.cpp")
else:
    print("Skinning RootSignature already exists.")
