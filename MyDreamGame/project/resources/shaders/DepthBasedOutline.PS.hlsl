#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSamplerLinear : register(s0);

Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSamplerPoint : register(s1);

struct ProjectionInverseParams
{
    float32_t4x4 projectionInverse;
};

ConstantBuffer<ProjectionInverseParams> gProjectionInverse : register(b1);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 3x3 Prewitt kernel constants for horizontal edge detection
static const float32_t3x3 kPrewittHorizontalKernel = float32_t3x3(
    -1.0f / 6.0f, 0.0f, 1.0f / 6.0f,
    -1.0f / 6.0f, 0.0f, 1.0f / 6.0f,
    -1.0f / 6.0f, 0.0f, 1.0f / 6.0f
);

// 3x3 Prewitt kernel constants for vertical edge detection
static const float32_t3x3 kPrewittVerticalKernel = float32_t3x3(
    -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f,
     0.0f,         0.0f,         0.0f,
     1.0f / 6.0f,  1.0f / 6.0f,  1.0f / 6.0f
);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // Get texture size for offset calculation
    float32_t width, height;
    gDepthTexture.GetDimensions(width, height);
    float32_t2 texelSize = float32_t2(1.0f / width, 1.0f / height);
    
    float32_t2 difference = float32_t2(0.0f, 0.0f);
    
    // Convolution loop (3x3 neighborhood)
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float32_t2 offset = float32_t2(x, y) * texelSize;
            float32_t2 uv = input.texcoord + offset;
            
            // Sample depth from texture
            float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, uv).r;
            
            // Reconstruct View space Z coordinate
            float32_t4 viewSpace = mul(float32_t4(0.0f, 0.0f, ndcDepth, 1.0f), gProjectionInverse.projectionInverse);
            float32_t viewZ = viewSpace.z * rcp(viewSpace.w);
            
            // Accumulate difference using Prewitt kernels
            difference.x += viewZ * kPrewittHorizontalKernel[y + 1][x + 1];
            difference.y += viewZ * kPrewittVerticalKernel[y + 1][x + 1];
        }
    }
    
    // Calculate edge weight (length of difference vector)
    float32_t weight = length(difference);
    weight = saturate(weight);
    
    // Sample original color texture
    float32_t4 originalColor = gTexture.Sample(gSamplerLinear, input.texcoord);
    
    // Blend original color with black outline based on edge weight
    output.color.rgb = (1.0f - weight) * originalColor.rgb;
    output.color.a = originalColor.a;
    
    return output;
}
