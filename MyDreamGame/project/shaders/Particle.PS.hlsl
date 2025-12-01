#include "Particle.hlsli"

struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t3);
SamplerState gSampler : register(s0);

float4 main(PixelShaderInput input) : SV_TARGET {
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    float4 outputColor = gMaterial.color * textureColor * input.color;
    
    if (outputColor.a == 0.0) {
        discard;
    }

    return outputColor;
}