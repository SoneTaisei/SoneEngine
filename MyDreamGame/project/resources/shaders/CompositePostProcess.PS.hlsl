#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gMaskTexture : register(t1);
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

    // Vector 7 (16 bytes)
    int enableDissolve;      // 1 to enable dissolve, 0 to disable
    float dissolveThreshold; // dissolve cutoff threshold [0.0, 1.0]
    float dissolveEdgeWidth; // width of the highlight edge
    float dissolvePadding;   // alignment padding

    // Vector 8 (16 bytes)
    float3 dissolveEdgeColor;// RGB color of the glowing edge
    float dissolvePadding2;  // alignment padding

    // Vector 9 (16 bytes)
    float3 dissolveBgColor;  // RGB color of the dissolved background
    float dissolvePadding3;  // alignment padding

    // Vector 10 (16 bytes)
    int enableNoise;         // 1 to enable noise, 0 to disable
    float noiseStrength;     // noise blending strength [0.0, 1.0]
    int noiseBlendMode;      // 0: Normal, 1: Add, 2: Multiply, 3: Screen, 4: Overlay
    float noiseScale;        // noise scale factor (grain size)

    // Vector 11 (16 bytes)
    float noiseTime;         // time factor for noise animation
    float3 noisePadding;     // alignment padding

    // Vector 12 (16 bytes)
    int enableIris;          // 1 to enable iris, 0 to disable
    float2 irisCenter;       // center coordinate of iris in UV space
    float irisRadius;        // radius of iris

    // Vector 13 (16 bytes)
    float irisSmoothness;    // edge smoothness width
    int isIrisIn;            // 0: Iris Out (close to mask), 1: Iris In (open to scene)
    float irisAspectRatio;   // aspect ratio (width / height)
    float irisPadding1;      // alignment padding

    // Vector 14 (16 bytes)
    float4 irisMaskColor;    // color of the masked outer region
};
ConstantBuffer<CompositeParams> gCompositeParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

// Generate 1D pseudo-random number from 2D coordinates
float32_t rand2dTo1d(float32_t2 uv) {
    return frac(sin(dot(uv, float32_t2(12.9898f, 78.233f))) * 43758.5453123f);
}

static const float32_t PI = 3.14159265f;


// 2D Gaussian function
float gauss(float x, float y, float sigma) {
    float exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

// Calculate distance from UV to custom center with aspect ratio correction
float CalculateIrisDistance(float2 uv, float2 center, float aspectRatio) {
    float2 diff = uv - center;
    diff.x *= aspectRatio;
    return length(diff);
}

// Calculate iris visibility factor
float CalculateIrisFactor(float2 uv, float2 center, float radius, float smoothness, float aspectRatio) {
    float dist = CalculateIrisDistance(uv, center, aspectRatio);
    float safeSmoothness = max(smoothness, 0.0001f);
    float halfSmooth = safeSmoothness * 0.5f;
    float edge0 = max(0.0f, radius - halfSmooth);
    float edge1 = radius + halfSmooth;
    return 1.0f - smoothstep(edge0, edge1, dist);
}

// Iris In / Iris Out function with configurable reference center
float4 ProcessIris(float4 sceneColor, float2 uv, float2 center, float radius, float smoothness, float aspectRatio, float4 maskColor, int isIrisIn) {
    float2 refCenter = center; // Configurable reference origin
    float factor = CalculateIrisFactor(uv, refCenter, radius, smoothness, aspectRatio);
    float3 rgb = lerp(maskColor.rgb, sceneColor.rgb, factor);
    float a = lerp(maskColor.a, sceneColor.a, factor);
    return float4(rgb, a);
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
    
    // 2. Apply Radial Blur (dynamic-sample radial blur overlay)
    if (gCompositeParams.enableRadialBlur != 0) {
        int32_t numSamples = clamp(gCompositeParams.radialBlurSamples, 1, 30);
        
        // Calculate direction from the center to current uv coordinate.
        float32_t2 direction = input.texcoord - gCompositeParams.radialBlurCenter;
        
        // Use processedColor (including box/gaussian blur if applied) for the first sample
        float32_t3 totalRadialColor = processedColor.rgb;
        
        for (int32_t sampleIndex = 1; sampleIndex < numSamples; ++sampleIndex) {
            // Step along the direction vector from current uv, scaled by radialBlurWidth
            float32_t2 texcoord = input.texcoord + direction * gCompositeParams.radialBlurWidth * float32_t(sampleIndex);
            totalRadialColor.rgb += gTexture.Sample(gSampler, texcoord).rgb;
        }
        
        // Average the accumulated color samples
        processedColor.rgb = totalRadialColor.rgb * rcp(float32_t(numSamples));
    }
    
    // 3. Apply Grayscale Effect
    if (gCompositeParams.grayscaleStrength > 0.0f) {
        // Calculate luma using BT.709 coefficients
        float32_t luma = dot(processedColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
        processedColor.rgb = lerp(processedColor.rgb, float32_t3(luma, luma, luma), gCompositeParams.grayscaleStrength);
    }
    
    // 4. Apply Sepia Effect
    if (gCompositeParams.sepiaStrength > 0.0f) {
        // Calculate luma using BT.709 coefficients
        float32_t value = dot(processedColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
        float32_t3 sepiaColor = value * float32_t3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);
        processedColor.rgb = lerp(processedColor.rgb, sepiaColor, gCompositeParams.sepiaStrength);
    }
    
    // 5. Apply Vignette Effect
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
    
    // 6. Apply Dissolve Effect
    if (gCompositeParams.enableDissolve != 0) {
        float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord).r;
        
        // Calculate edge highlight intensity safely avoiding division by zero
        float32_t edgeWidth = max(gCompositeParams.dissolveEdgeWidth, 0.0001f);
        float32_t edge = 1.0f - smoothstep(gCompositeParams.dissolveThreshold, gCompositeParams.dissolveThreshold + edgeWidth, mask);
        processedColor.rgb += edge * gCompositeParams.dissolveEdgeColor;

        // Background color replacement (instead of discard, so we can change background color)
        if (mask <= gCompositeParams.dissolveThreshold) {
            processedColor.rgb = gCompositeParams.dissolveBgColor;
        }
    }
    
    // 7. Apply Noise Effect
    if (gCompositeParams.enableNoise != 0) {
        // Use noiseScale to control grain size, and offset by fractional pseudo-random values generated from noiseTime to make it pop and animate beautifully
        float32_t2 timeOffset = float32_t2(
            frac(sin(gCompositeParams.noiseTime) * 43758.5453f),
            frac(cos(gCompositeParams.noiseTime) * 43758.5453f)
        );
        float32_t2 noiseUv = input.texcoord * gCompositeParams.noiseScale + timeOffset;
        
        float32_t noiseVal = rand2dTo1d(noiseUv);
        float32_t3 noiseColor = float32_t3(noiseVal, noiseVal, noiseVal);
        float32_t3 blendedColor = processedColor.rgb;
        
        if (gCompositeParams.noiseBlendMode == 0) {
            // Normal (Lerp / Mix)
            blendedColor = noiseColor;
        } else if (gCompositeParams.noiseBlendMode == 1) {
            // Add
            blendedColor = processedColor.rgb + noiseColor;
        } else if (gCompositeParams.noiseBlendMode == 2) {
            // Multiply
            blendedColor = processedColor.rgb * noiseColor;
        } else if (gCompositeParams.noiseBlendMode == 3) {
            // Screen
            blendedColor = 1.0f - (1.0f - processedColor.rgb) * (1.0f - noiseColor);
        } else if (gCompositeParams.noiseBlendMode == 4) {
            // Overlay
            float32_t3 overlayLow = 2.0f * processedColor.rgb * noiseColor;
            float32_t3 overlayHigh = 1.0f - 2.0f * (1.0f - processedColor.rgb) * (1.0f - noiseColor);
            blendedColor = lerp(overlayLow, overlayHigh, step(0.5f, processedColor.rgb));
        }
        
        processedColor.rgb = lerp(processedColor.rgb, blendedColor, gCompositeParams.noiseStrength);
    }
    
    // 8. Apply Iris Effect
    if (gCompositeParams.enableIris != 0) {
        processedColor = ProcessIris(
            processedColor,
            input.texcoord,
            gCompositeParams.irisCenter,
            gCompositeParams.irisRadius,
            gCompositeParams.irisSmoothness,
            gCompositeParams.irisAspectRatio,
            gCompositeParams.irisMaskColor,
            gCompositeParams.isIrisIn
        );
    }
    
    output.color.rgb = processedColor.rgb;
    output.color.a = originalColor.a;
    
    return output;
}
