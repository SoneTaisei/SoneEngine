#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Unified Composite Post-Process Parameters (96 bytes, 16-byte aligned)
struct CompositeParams {
    // Vector 1 (16 bytes)
    float grayscaleStrength;
    float sepiaStrength;
    int enableVignette;
    float vignetteScale;
    
    // Vector 2 (16 bytes)
    float vignettePower;
    int blurType;            // 0: None, 1: BoxBlur, 2: GaussianBlur
    int boxBlurKernelSize;   // radius of box blur (1 = 3x3, 2 = 5x5, etc.)
    float boxBlurStrength;   // lerp factor: 0 = original, 1 = fully blurred
    
    // Vector 3 (16 bytes)
    float4 vignetteColor;
    
    // Vector 4 (16 bytes)
    float gaussianSigma;     // standard deviation for Gaussian blur
    float2 texelSize;        // 1.0 / textureResolution
    float padding;           // padding to align to 16 bytes

    // Vector 5 & 6 (32 bytes aligned)
    int enableRadialBlur;    // 1 to enable radial blur, 0 to disable
    float2 radialBlurCenter; // center of radial blur in UV coordinates
    float radialBlurWidth;   // width/strength of radial blur
    int radialBlurSamples;   // number of samples for radial blur
    float3 radialPadding;    // padding to align to 16 bytes
};
ConstantBuffer<CompositeParams> gCompositeParams : register(b0);

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
    float32_t4 processedColor = originalColor;
    
    // 1. Apply Blur Effects
    if (gCompositeParams.blurType == 1) {
        // Box Blur (Smoothing)
        int halfSize = gCompositeParams.boxBlurKernelSize;
        float32_t4 totalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        int sampleCount = 0;
        
        for (int y = -halfSize; y <= halfSize; y++) {
            for (int x = -halfSize; x <= halfSize; x++) {
                float32_t2 offset = float32_t2(float(x), float(y)) * gCompositeParams.texelSize;
                totalColor += gTexture.Sample(gSampler, input.texcoord + offset);
                sampleCount++;
            }
        }
        
        float32_t4 blurredColor = totalColor / float(sampleCount);
        processedColor.rgb = lerp(processedColor.rgb, blurredColor.rgb, gCompositeParams.boxBlurStrength);
        
    } else if (gCompositeParams.blurType == 2) {
        // Gaussian Blur (3x3 Kernel)
        float32_t weight = 0.0f;
        float32_t kernel3x3[3][3];
        
        // Calculate kernel weights dynamically based on sigma
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                kernel3x3[x + 1][y + 1] = gauss(float(x), float(y), gCompositeParams.gaussianSigma);
                weight += kernel3x3[x + 1][y + 1];
            }
        }
        
        // Perform 3x3 convolution
        float32_t4 totalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                float32_t2 offset = float32_t2(float(x), float(y)) * gCompositeParams.texelSize;
                totalColor += gTexture.Sample(gSampler, input.texcoord + offset) * kernel3x3[x + 1][y + 1];
            }
        }
        
        // Normalize the output color
        processedColor.rgb = totalColor.rgb * rcp(weight);
    }
    
    // 2. Apply Grayscale Effect
    if (gCompositeParams.grayscaleStrength > 0.0f) {
        // Calculate luma using BT.709 coefficients
        float32_t luma = dot(processedColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
        processedColor.rgb = lerp(processedColor.rgb, float32_t3(luma, luma, luma), gCompositeParams.grayscaleStrength);
    }
    
    // 3. Apply Sepia Effect
    if (gCompositeParams.sepiaStrength > 0.0f) {
        // Calculate luma using BT.709 coefficients
        float32_t value = dot(processedColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
        float32_t3 sepiaColor = value * float32_t3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);
        processedColor.rgb = lerp(processedColor.rgb, sepiaColor, gCompositeParams.sepiaStrength);
    }
    
    // 4. Apply Vignette Effect
    if (gCompositeParams.enableVignette != 0) {
        // Adjust coordinates so edges are 0 and center is brighter
        float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
        // Apply scale (max value at center is 0.0625, so we multiply by scale)
        float vignette = correct.x * correct.y * gCompositeParams.vignetteScale;
        // Apply power
        vignette = saturate(pow(vignette, gCompositeParams.vignettePower));
        // Interpolate: 0 = edge (vignette color), 1 = center (original processed color)
        processedColor.rgb = lerp(gCompositeParams.vignetteColor.rgb, processedColor.rgb, vignette);
    }
    
    // 5. Apply Radial Blur (dynamic-sample radial blur overlay)
    if (gCompositeParams.enableRadialBlur != 0) {
        int32_t numSamples = clamp(gCompositeParams.radialBlurSamples, 1, 30);
        
        // Calculate direction from the center to current uv coordinate.
        float32_t2 direction = input.texcoord - gCompositeParams.radialBlurCenter;
        
        float32_t3 totalRadialColor = float32_t3(0.0f, 0.0f, 0.0f);
        
        for (int32_t sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
            // Step along the direction vector from current uv, scaled by radialBlurWidth
            float32_t2 texcoord = input.texcoord + direction * gCompositeParams.radialBlurWidth * float32_t(sampleIndex);
            totalRadialColor.rgb += gTexture.Sample(gSampler, texcoord).rgb;
        }
        
        // Average the accumulated color samples
        processedColor.rgb = totalRadialColor.rgb * rcp(float32_t(numSamples));
    }
    
    output.color.rgb = processedColor.rgb;
    output.color.a = originalColor.a;
    
    return output;
}
