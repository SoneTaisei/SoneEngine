#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurParams {
    float32_t2 center;
    float32_t blurWidth;
    int32_t numSamples;
};
ConstantBuffer<RadialBlurParams> gRadialBlurParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    int32_t numSamples = clamp(gRadialBlurParams.numSamples, 1, 30);
    
    // Calculate direction from the center to current uv coordinate.
    // Unlike a standard unit vector, this direction is not normalized,
    // so points further from the center get sampled further away (radial effect).
    float32_t2 direction = input.texcoord - gRadialBlurParams.center;
    
    float32_t3 outputColor = float32_t3(0.0f, 0.0f, 0.0f);
    
    for (int32_t sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
        // Step along the direction vector from current uv, scaled by blurWidth
        float32_t2 texcoord = input.texcoord + direction * gRadialBlurParams.blurWidth * float32_t(sampleIndex);
        outputColor.rgb += gTexture.Sample(gSampler, texcoord).rgb;
    }
    
    // Calculate the average color
    outputColor.rgb *= rcp(float32_t(numSamples));
    
    PixelShaderOutput output;
    output.color.rgb = outputColor;
    output.color.a = 1.0f;
    return output;
}
