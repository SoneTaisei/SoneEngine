#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Gaussian parameters
struct GaussianParams {
    float sigma;        // Standard deviation
    float2 texelSize;   // 1.0 / textureResolution
};
ConstantBuffer<GaussianParams> gGaussianParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t PI = 3.14159265f;

// 2D Gaussian function
float gauss(float x, float y, float sigma) {
    float exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float32_t4 originalColor = gTexture.Sample(gSampler, input.texcoord);
    
    // Calculate Gaussian kernel weight
    float32_t weight = 0.0f;
    float32_t kernel3x3[3][3];
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            kernel3x3[x + 1][y + 1] = gauss(float(x), float(y), gGaussianParams.sigma);
            weight += kernel3x3[x + 1][y + 1];
        }
    }
    
    // Accumulate color from neighboring texels (convolution)
    float32_t4 totalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float32_t2 offset = float32_t2(float(x), float(y)) * gGaussianParams.texelSize;
            totalColor += gTexture.Sample(gSampler, input.texcoord + offset) * kernel3x3[x + 1][y + 1];
        }
    }
    
    // Normalize and assign final color
    output.color.rgb = totalColor.rgb * rcp(weight);
    output.color.a = originalColor.a;
    
    return output;
}
