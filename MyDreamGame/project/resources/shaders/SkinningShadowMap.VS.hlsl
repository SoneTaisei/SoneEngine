cbuffer TransformCB : register(b0) {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

cbuffer ShadowGlobalCB : register(b1) {
    float4x4 LightViewProjection;
};

struct Well {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<Well> gPalette : register(t1);

struct SkinningVSInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(SkinningVSInput input) {
    VSOutput output;
    
    // Skinning position transformation
    float4 skinnedPos = mul(input.position, gPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinnedPos += mul(input.position, gPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinnedPos += mul(input.position, gPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinnedPos += mul(input.position, gPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinnedPos.w = 1.0f;

    float4 worldPosition = mul(skinnedPos, World);
    output.position = mul(worldPosition, LightViewProjection);
    return output;
}
