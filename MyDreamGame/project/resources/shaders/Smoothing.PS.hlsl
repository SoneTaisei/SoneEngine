#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Smoothing parameters
struct SmoothingParams {
    int kernelSize;    // half-size of the box kernel (1 = 3x3, 2 = 5x5, etc.)
    float2 texelSize;  // 1.0 / textureResolution
    float strength;    // blend factor: 0 = original, 1 = fully smoothed
};
ConstantBuffer<SmoothingParams> gSmoothingParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float32_t4 originalColor = gTexture.Sample(gSampler, input.texcoord);
    
    int halfSize = gSmoothingParams.kernelSize;
    
    // accumulate color from neighboring texels (box blur)
    float32_t4 totalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    int sampleCount = 0;
    
    for (int y = -halfSize; y <= halfSize; y++) {
        for (int x = -halfSize; x <= halfSize; x++) {
            float32_t2 offset = float32_t2(float(x), float(y)) * gSmoothingParams.texelSize;
            totalColor += gTexture.Sample(gSampler, input.texcoord + offset);
            sampleCount++;
        }
    }
    
    float32_t4 smoothedColor = totalColor / float(sampleCount);
    
    // blend between original and smoothed based on strength
    output.color.rgb = lerp(originalColor.rgb, smoothedColor.rgb, gSmoothingParams.strength);
    output.color.a = originalColor.a;
    
    return output;
}
