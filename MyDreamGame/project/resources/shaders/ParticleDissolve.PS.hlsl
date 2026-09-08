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

// Pixel shader with dissolve edge effect based on vertex alpha
float4 main(PixelShaderInput input) : SV_TARGET {
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // Calculate effective alpha threshold using vertex color alpha (lifetime progression)
    float threshold = 1.0f - input.color.a;
    
    // Texture luminance used as mask pattern
    float mask = (textureColor.r + textureColor.g + textureColor.b) / 3.0f;
    
    // Discard pixels below the dissolve threshold
    if (mask < threshold) {
        discard;
    }
    
    // Add burning edge glow when close to threshold
    float edgeWidth = 0.15f;
    float edgeFactor = saturate((threshold + edgeWidth - mask) / edgeWidth);
    float3 fireColor = float3(1.0f, 0.5f, 0.1f) * 2.0f; // Fiery orange rim
    
    float4 outputColor = gMaterial.color * textureColor;
    outputColor.rgb = lerp(outputColor.rgb * input.color.rgb, fireColor, edgeFactor);
    outputColor.a = textureColor.a;
    
    if (outputColor.a <= 0.001f) {
        discard;
    }

    return outputColor;
}
