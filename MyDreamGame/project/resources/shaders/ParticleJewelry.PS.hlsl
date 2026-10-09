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

// Crystal / Gemstone Shading for Particles (Clean, noise-free jewel rendering)
// Computes smooth multi-band iridescence, crisp specular, inner scattering, and edge glow.
float4 main(PixelShaderInput input) : SV_TARGET {
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 1. Safe, noise-free normal from vertex data
    float3 normal = input.normal;
    float normalLen = length(normal);
    if (normalLen > 0.001f) {
        normal = normalize(normal);
    } else {
        normal = float3(0.0f, 0.0f, -1.0f);
    }

    // 2. View vector (forward facing camera direction)
    float3 toEye = float3(0.0f, 0.0f, -1.0f);
    float NdotV = saturate(abs(dot(normal, toEye)));
    float fresnel = 1.0f - NdotV;
    float fresnelPow = pow(fresnel, 2.5f);

    // 3. Multi-band Iridescence (Prism dispersion across facet angles)
    float dispersionShift = frac(fresnel * 1.2f + abs(normal.x) * 0.35f + abs(normal.y) * 0.25f);
    float3 iridColor;
    float3 baseColor = gMaterial.color.rgb * input.color.rgb;

    if (dispersionShift < 0.33f) {
        float t = dispersionShift / 0.33f;
        iridColor = lerp(baseColor, float3(1.0f, 0.25f, 0.65f), t); // Base to Vivid Magenta-Pink
    } else if (dispersionShift < 0.66f) {
        float t = (dispersionShift - 0.33f) / 0.33f;
        iridColor = lerp(float3(1.0f, 0.25f, 0.65f), float3(0.55f, 0.25f, 0.95f), t); // Pink to Deep Violet
    } else {
        float t = (dispersionShift - 0.66f) / 0.34f;
        iridColor = lerp(float3(0.55f, 0.25f, 0.95f), float3(1.0f, 0.88f, 0.40f), t); // Violet to Golden Amber
    }

    // 4. Directional Specular (Sharp diamond facet reflections)
    float3 lightDir = normalize(float3(0.4f, 0.8f, -0.6f));
    float3 halfVector = normalize(lightDir + toEye);
    float NdotH = saturate(abs(dot(normal, halfVector)));
    float surfaceSpec = pow(NdotH, 32.0f);
    float facetSpec = pow(NdotH, 12.0f);
    float3 specular = float3(1.0f, 1.0f, 1.0f) * surfaceSpec * 1.4f + iridColor * facetSpec * 0.8f;

    // 5. Inner scattering glow
    float innerGlowFactor = pow(NdotV, 1.2f) * 0.6f + 0.4f;
    float3 innerGlow = lerp(baseColor, iridColor, 0.5f) * innerGlowFactor;

    // 6. Outer Rim Highlight (Fresnel edge shimmer)
    float rimFactor = fresnelPow;
    float3 rimColor = lerp(float3(0.4f, 0.85f, 1.0f), float3(1.0f, 0.45f, 0.85f), dispersionShift);
    float3 rimLight = rimColor * rimFactor * 1.2f;

    // 7. Final Composition (Add glowing crystal components, tint with texture)
    float3 finalRGB = (innerGlow + specular + rimLight) * textureColor.rgb;
    float finalAlpha = gMaterial.color.a * input.color.a * textureColor.a;

    if (finalAlpha <= 0.001f) {
        discard;
    }

    return float4(finalRGB, finalAlpha);
}

