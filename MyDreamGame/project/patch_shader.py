import os

shader_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/resources/shaders/Object3d.VS.hlsl'

with open(shader_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 構造体とStructuredBufferの追加
skinning_code = r'''
struct WellForGPU {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

struct VertexWeightData {
    float weight[4];
    int jointIndex[4];
};

StructuredBuffer<WellForGPU> gMatrixPalette : register(t1);
StructuredBuffer<VertexWeightData> gInfluence : register(t2);
'''

# main関数の引数に uint vertexID : SV_VertexID を追加
content = content.replace('VertexShaderOutput main(VertexShaderInput input) {', skinning_code + '\nVertexShaderOutput main(VertexShaderInput input, uint vertexID : SV_VertexID) {')

# スキニング処理を挿入
skinning_calc = r'''
    // スキニング計算
    VertexWeightData weightData = gInfluence[vertexID];
    
    matrix skinnedMatrix = 
        mul(weightData.weight[0], gMatrixPalette[weightData.jointIndex[0]].skeletonSpaceMatrix) +
        mul(weightData.weight[1], gMatrixPalette[weightData.jointIndex[1]].skeletonSpaceMatrix) +
        mul(weightData.weight[2], gMatrixPalette[weightData.jointIndex[2]].skeletonSpaceMatrix) +
        mul(weightData.weight[3], gMatrixPalette[weightData.jointIndex[3]].skeletonSpaceMatrix);

    matrix skinnedInverseTransposeMatrix = 
        mul(weightData.weight[0], gMatrixPalette[weightData.jointIndex[0]].skeletonSpaceInverseTransposeMatrix) +
        mul(weightData.weight[1], gMatrixPalette[weightData.jointIndex[1]].skeletonSpaceInverseTransposeMatrix) +
        mul(weightData.weight[2], gMatrixPalette[weightData.jointIndex[2]].skeletonSpaceInverseTransposeMatrix) +
        mul(weightData.weight[3], gMatrixPalette[weightData.jointIndex[3]].skeletonSpaceInverseTransposeMatrix);

    // スキニングされた頂点と法線
    float4 skinnedPosition = mul(input.position, skinnedMatrix);
    float3 skinnedNormal = normalize(mul(input.normal, (float3x3)skinnedInverseTransposeMatrix));

    // World行列などの適用
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.localPosition = skinnedPosition.xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    output.color = input.color;
    
    return output;
'''

# 既存の output の計算をごっそり置換
import re
content = re.sub(r'output\.position = mul\(input\.position, gTransformationMatrix\.WVP\);.*?return output;', skinning_calc.strip() + '\n    return output;', content, flags=re.DOTALL)

with open(shader_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated Object3d.VS.hlsl for Skinning")
