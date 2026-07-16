#include "Object3d.hlsli"

// Transformation matrix for the object
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// Directional light (not used directly in this simple VS, but kept for consistency)
cbuffer gDirectionalLight : register(b4) {
    DirectionalLight gDirectionalLight;
}

// WellMatrix for skinning
struct Well {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

// SkinCluster data (Palette)
StructuredBuffer<Well> gPalette : register(t1);

// Vertex Shader Input
struct SkinningVertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

struct Skinned {
    float4 position;
    float3 normal;
};

Skinned Skinning(SkinningVertexShaderInput input) {
    Skinned skinned;
    
    // 位置の変換
    skinned.position = mul(input.position, gPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f; // 確実に1を入れる
    
    // 法線の変換
    skinned.normal = mul(input.normal, (float3x3)gPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float3x3)gPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float3x3)gPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float3x3)gPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal); // 正規化して戻してあげる
    
    return skinned;
}

VertexShaderOutput main(SkinningVertexShaderInput input) {
    VertexShaderOutput output;
    
    Skinned skinned = Skinning(input); // まずSkinning計算を行って、Skinning後の頂点情報を手に入れる
    
    // Skinning結果を使って変換
    output.position = mul(skinned.position, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinned.position, gTransformationMatrix.World).xyz;
    output.localPosition = input.position.xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinned.normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    
    output.color = input.color;
    
    return output;
}
