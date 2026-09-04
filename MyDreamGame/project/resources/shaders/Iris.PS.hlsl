#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Iris post-process parameter constant buffer (16-byte aligned)
struct IrisParams {
    float2 center;       // Reference center position in UV space [0.0, 1.0]
    float radius;        // Iris circle radius [0.0 ~ 1.5+]
    float smoothness;    // Edge feather / softness width

    float4 maskColor;    // Color of the masked region outside the iris circle

    int isIrisIn;        // 0: Iris-Out (closing to mask), 1: Iris-In (opening to scene)
    float aspectRatio;   // Screen aspect ratio (width / height) for circular correction
    float2 padding;      // Padding to align to 16 bytes
};
ConstantBuffer<IrisParams> gIrisParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

// Calculate distance from current UV to a specified reference center with aspect ratio correction
// Modifying 'center' alters the reference origin for the circular iris
float CalculateIrisDistance(float2 uv, float2 center, float aspectRatio) {
    float2 diff = uv - center;
    // Correct horizontal coordinate using aspect ratio to keep circle circular
    diff.x *= aspectRatio;
    return length(diff);
}

// Calculate visibility factor (0.0 = outer masked color, 1.0 = inner visible scene)
// center: Reference pivot point for the iris
// radius: Current radius of the circle
// smoothness: Smooth transition width at the edge of the circle
// aspectRatio: Aspect ratio of the viewport
float CalculateIrisFactor(float2 uv, float2 center, float radius, float smoothness, float aspectRatio) {
    float dist = CalculateIrisDistance(uv, center, aspectRatio);
    
    // Ensure smooth edge transition without division by zero
    float safeSmoothness = max(smoothness, 0.0001f);
    float halfSmooth = safeSmoothness * 0.5f;
    float edge0 = max(0.0f, radius - halfSmooth);
    float edge1 = radius + halfSmooth;
    
    // 1.0 inside the iris circle, 0.0 outside
    return 1.0f - smoothstep(edge0, edge1, dist);
}

// Iris-In function (Scene opens from mask color around the reference center)
// center: Reference anchor point for opening the iris
float4 ApplyIrisIn(float4 sceneColor, float2 uv, float2 center, float radius, float smoothness, float aspectRatio, float4 maskColor) {
    float factor = CalculateIrisFactor(uv, center, radius, smoothness, aspectRatio);
    float3 rgb = lerp(maskColor.rgb, sceneColor.rgb, factor);
    float a = lerp(maskColor.a, sceneColor.a, factor);
    return float4(rgb, a);
}

// Iris-Out function (Scene closes into mask color around the reference center)
// center: Reference anchor point for closing the iris
float4 ApplyIrisOut(float4 sceneColor, float2 uv, float2 center, float radius, float smoothness, float aspectRatio, float4 maskColor) {
    float factor = CalculateIrisFactor(uv, center, radius, smoothness, aspectRatio);
    float3 rgb = lerp(maskColor.rgb, sceneColor.rgb, factor);
    float a = lerp(maskColor.a, sceneColor.a, factor);
    return float4(rgb, a);
}

// Configurable Iris function: Allows changing the reference center inside or via parameter
// You can adjust the reference point directly inside this function (e.g. offset, custom anchors)
float4 ProcessIris(float4 sceneColor, float2 uv, float2 center, float radius, float smoothness, float aspectRatio, float4 maskColor, int isIrisIn) {
    // --- Customizable Reference Center Logic ---
    // The reference center can be modified here if needed (e.g., center = float2(0.5f, 0.5f) for screen center,
    // or modulated with coordinates/offsets). By default, it uses the provided reference 'center'.
    float2 refCenter = center;

    if (isIrisIn != 0) {
        return ApplyIrisIn(sceneColor, uv, refCenter, radius, smoothness, aspectRatio, maskColor);
    } else {
        return ApplyIrisOut(sceneColor, uv, refCenter, radius, smoothness, aspectRatio, maskColor);
    }
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // Sample original scene texture
    float32_t4 sceneColor = gTexture.Sample(gSampler, input.texcoord);
    
    // Execute iris processing with configurable reference center
    output.color = ProcessIris(
        sceneColor,
        input.texcoord,
        gIrisParams.center,
        gIrisParams.radius,
        gIrisParams.smoothness,
        gIrisParams.aspectRatio,
        gIrisParams.maskColor,
        gIrisParams.isIrisIn
    );
    
    return output;
}
