#include "Particle.hlsli"

struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Pixel shader with glowing core and boosted brightness
float4 main(PixelShaderInput input) : SV_TARGET {
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // Base particle color
    float4 baseColor = gMaterial.color * textureColor * input.color;
    
    // Calculate distance from center in UV space
    float2 uvOffset = transformedUV.xy - float2(0.5f, 0.5f);
    float distFromCenter = length(uvOffset);
    
    // Core glow boost: brighter in the center
    float coreGlow = saturate(1.0f - distFromCenter * 2.0f);
    coreGlow = pow(coreGlow, 2.0f);
    
    // Boosted RGB for high-intensity glow
    float3 glowRGB = baseColor.rgb * 1.5f + float3(coreGlow, coreGlow, coreGlow) * baseColor.rgb;
    
    float4 outputColor = float4(glowRGB, baseColor.a);
    
    if (outputColor.a <= 0.001f) {
        discard;
    }

    return outputColor;
}
