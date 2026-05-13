#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VignetteParams {
    float32_t4 color;
    float scale;
    float power;
};
ConstantBuffer<VignetteParams> gVignetteParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    
    // adjust coords so edges are 0 and center is brighter
    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    // adjust scale because max value at center is only 0.0625
    float vignette = correct.x * correct.y * gVignetteParams.scale;
    // apply power
    vignette = saturate(pow(vignette, gVignetteParams.power));
    
    // interpolate between vignette color and original color
    // 0 = edge (use color), 1 = center (use original image)
    output.color.rgb = lerp(gVignetteParams.color.rgb, output.color.rgb, vignette);
    
    return output;
}
