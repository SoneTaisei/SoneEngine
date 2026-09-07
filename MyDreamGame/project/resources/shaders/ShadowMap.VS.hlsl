struct VSInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

cbuffer TransformCB : register(b0) {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

cbuffer ShadowGlobalCB : register(b1) {
    float4x4 LightViewProjection;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4 worldPosition = mul(input.position, World);
    output.position = mul(worldPosition, LightViewProjection);
    return output;
}
