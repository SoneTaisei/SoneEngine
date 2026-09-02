#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentMap : register(t1);
SamplerState gSampler : register(s0);

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<PointLight> gPointLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET {
    // 3D Infinite Procedural Floor Grid (enableBoxMapping == 2.0)
    if (gMaterial.enableBoxMapping > 1.5f) {
        float2 coord = input.worldPosition.xz;
        float2 deriv = max(fwidth(coord), 0.0001f);
        
        // Distance from camera to current surface point on XZ plane
        float camDist = length(input.worldPosition.xz - gCamera.worldPosition.xz);
        
        // Smooth distance fade factors for multi-scale LOD
        float fineFade = 1.0f - smoothstep(15.0f, 50.0f, camDist);
        float majorFade = 1.0f - smoothstep(50.0f, 250.0f, camDist);
        float megaFade = 1.0f - smoothstep(150.0f, 1000.0f, camDist);
        float axisFade = 1.0f - smoothstep(100.0f, 1500.0f, camDist);
        
        // 1-meter fine grid lines (fades out in medium distance)
        float2 fineGrid = abs(frac(coord - 0.5f) - 0.5f) / deriv;
        float fineLineVal = min(fineGrid.x, fineGrid.y);
        float fineLineWeight = (1.0f - min(fineLineVal, 1.0f)) * fineFade;
        
        // 10-meter major grid lines (fades out in far distance)
        float2 majorGrid = abs(frac(coord * 0.1f - 0.5f) - 0.5f) / (deriv * 0.1f);
        float majorLineVal = min(majorGrid.x, majorGrid.y);
        float majorLineWeight = (1.0f - min(majorLineVal, 1.0f)) * majorFade;
        
        // 50-meter mega grid lines (visible up to the horizon)
        float2 megaGrid = abs(frac(coord * 0.02f - 0.5f) - 0.5f) / (deriv * 0.02f);
        float megaLineVal = min(megaGrid.x, megaGrid.y);
        float megaLineWeight = (1.0f - min(megaLineVal, 1.0f)) * megaFade;
        
        // Base floor color
        float4 baseColor = gMaterial.color;
        
        // Line colors with hierarchical brightness
        float4 fineGridColor = float4(0.32f, 0.32f, 0.35f, 0.65f);
        float4 majorGridColor = float4(0.48f, 0.48f, 0.53f, 0.80f);
        float4 megaGridColor = float4(0.60f, 0.60f, 0.68f, 0.85f);
        
        // Axis lines: X-axis (z ~ 0: Red) and Z-axis (x ~ 0: Blue)
        float2 axisDist = abs(coord) / deriv;
        float xAxisWeight = (1.0f - min(axisDist.y, 1.0f)) * axisFade;
        float zAxisWeight = (1.0f - min(axisDist.x, 1.0f)) * axisFade;
        
        // Layered blending
        float4 finalColor = lerp(baseColor, fineGridColor, fineLineWeight * 0.70f);
        finalColor = lerp(finalColor, majorGridColor, majorLineWeight * 0.80f);
        finalColor = lerp(finalColor, megaGridColor, megaLineWeight * 0.85f);
        finalColor = lerp(finalColor, float4(0.92f, 0.25f, 0.30f, 1.0f), xAxisWeight * 0.95f);
        finalColor = lerp(finalColor, float4(0.25f, 0.55f, 0.95f, 1.0f), zAxisWeight * 0.95f);
        
        // Calculate dynamic alpha transparency
        float lineAlpha = max(max(fineLineWeight * 0.70f, majorLineWeight * 0.80f),
                              max(megaLineWeight * 0.85f, max(xAxisWeight * 0.95f, zAxisWeight * 0.95f)));
        finalColor.a = max(baseColor.a, lineAlpha);
        
        if (finalColor.a <= 0.001f) {
            discard;
        }
        
        return finalColor;
    }

    float4 outputColor;
    
    float4 transformedUV;
    
    if (gMaterial.enableBoxMapping > 0.5f) {
        float3 absNormal = abs(input.normal);
        float2 finalUV = float2(0.0f, 0.0f);
        // The object scale is embedded in the uvTransform's first two diagonal elements
        // (Since MapChip2D sets uvTransform = Scale(spanWidth, spanHeight, 1.0f))
        float spanWidth = gMaterial.uvTransform._11;
        float spanHeight = gMaterial.uvTransform._22;
        float spanDepth = gMaterial.uvTransform._33;
        
        float3 scaledPos = input.localPosition * float3(spanWidth, spanHeight, spanDepth);
        
        if (absNormal.z > 0.5f) { // Front/Back
            finalUV = float2(scaledPos.x + 0.5f * spanWidth, -scaledPos.y + 0.5f * spanHeight); 
        } else if (absNormal.x > 0.5f) { // Left/Right
            finalUV = float2(-scaledPos.z + 0.5f * spanDepth, -scaledPos.y + 0.5f * spanHeight);
        } else { // Top/Bottom
            finalUV = float2(scaledPos.x + 0.5f * spanWidth, scaledPos.z + 0.5f * spanDepth);
        }
        transformedUV = float4(finalUV, 0.0f, 1.0f);
    } else {
        transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    }
    
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // Perform alpha discard using threshold from material
    if (textureColor.a <= gMaterial.alphaReference) {
        discard;
    }
    
    // Dissolve discard using procedural blocky noise
    if (gMaterial.dissolveThreshold > 0.0f) {
        // Create blocky noise for dissolve (mosaic effect)
        // A smaller grid size means fewer, larger squares.
        float2 gridSize = float2(6.0f, 6.0f); 
        float2 blockUv = floor(input.texcoord * gridSize);
        float noiseValue = frac(sin(dot(blockUv, float2(12.9898f, 78.233f))) * 43758.5453f);
        
        if (noiseValue < gMaterial.dissolveThreshold) {
            discard;
        }
    }
    
    if (gMaterial.lightingType == 1) {
        float3 normal = normalize(input.normal);
        if (gDirectionalLight.enableFlatShading != 0) {
            float3 dpdx = ddx(input.worldPosition);
            float3 dpdy = ddy(input.worldPosition);
            float3 flatNormal = normalize(cross(dpdx, dpdy));
            if (dot(flatNormal, normal) < 0.0f) {
                flatNormal = -flatNormal;
            }
            normal = flatNormal;
        }
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        
        // 1. Directional Light (Smooth Half-Lambert diffuse and guarded Blinn-Phong specular)
        float3 directionalLightDir = normalize(-gDirectionalLight.direction);
        float directionalNdotL = dot(normal, directionalLightDir);
        float directionalHalfLambert = directionalNdotL * 0.5f + 0.5f;
        float directionalCos = directionalHalfLambert * directionalHalfLambert;
        float3 diffuseDirectional = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * directionalCos * gDirectionalLight.intensity;
        
        float3 directionalHalfVector = normalize(directionalLightDir + toEye);
        float directionalNdotH = dot(normal, directionalHalfVector);
        float directionalSpecularPow = (gMaterial.shininess > 0.0f) ? pow(saturate(directionalNdotH), gMaterial.shininess) : 0.0f;
        float directionalSpecularMask = saturate(directionalNdotL * 2.0f);
        float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * (directionalSpecularPow * directionalSpecularMask) * float3(1.0f, 1.0f, 1.0f);
        
        // 2. Point Light (Soft Half-Lambert diffuse and guarded specular)
        float distance = length(gPointLight.position - input.worldPosition);
        float factor = pow(saturate(-distance / max(0.0001f, gPointLight.radius) + 1.0f), gPointLight.decay);
        
        float3 pointLightDir = normalize(input.worldPosition - gPointLight.position);
        float pointNdotL = dot(normal, -pointLightDir);
        float pointHalfLambert = pointNdotL * 0.5f + 0.5f;
        float pointCos = pointHalfLambert * pointHalfLambert;
        float3 diffusePoint = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * pointCos * gPointLight.intensity * factor;
        
        float3 pointHalfVector = normalize(-pointLightDir + toEye);
        float pointNdotH = dot(normal, pointHalfVector);
        float pointSpecularPow = (gMaterial.shininess > 0.0f) ? pow(saturate(pointNdotH), gMaterial.shininess) : 0.0f;
        float pointSpecularMask = saturate(pointNdotL * 2.0f);
        float3 specularPoint = gPointLight.color.rgb * gPointLight.intensity * (pointSpecularPow * pointSpecularMask) * float3(1.0f, 1.0f, 1.0f) * factor;
        
        // 3. Spot Light (Soft Half-Lambert diffuse and guarded specular)
        float3 spotLightDirOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float spotDistance = length(gSpotLight.position - input.worldPosition);
        float spotAttenuation = pow(saturate(1.0f - (spotDistance / max(0.0001f, gSpotLight.distance))), gSpotLight.decay);
        
        float cosTheta = dot(spotLightDirOnSurface, normalize(gSpotLight.direction));
        float falloffRange = gSpotLight.cosFalloffStart - gSpotLight.cosAngle;
        float falloffFactor = saturate((cosTheta - gSpotLight.cosAngle) / max(0.001f, falloffRange));
        
        float spotNdotL = dot(normal, -spotLightDirOnSurface);
        float spotHalfLambert = spotNdotL * 0.5f + 0.5f;
        float spotCos = spotHalfLambert * spotHalfLambert;
        float3 diffuseSpot = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * spotCos * gSpotLight.intensity * spotAttenuation * falloffFactor;
        
        float3 spotHalfVector = normalize(-spotLightDirOnSurface + toEye);
        float spotNdotH = dot(normal, spotHalfVector);
        float spotSpecularPow = (gMaterial.shininess > 0.0f) ? pow(saturate(spotNdotH), gMaterial.shininess) : 0.0f;
        float spotSpecularMask = saturate(spotNdotL * 2.0f);
        float3 specularSpot = gSpotLight.color.rgb * gSpotLight.intensity * (spotSpecularPow * spotSpecularMask) * float3(1.0f, 1.0f, 1.0f) * spotAttenuation * falloffFactor;
        
        float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
        float4 environmentColor = gEnvironmentMap.Sample(gSampler, reflectedVector);

        // 4. Final color composition
        float3 diffuseTotal = (diffuseDirectional + diffusePoint + diffuseSpot) * input.color.rgb;
        float3 specularTotal = (specularDirectional + specularPoint + specularSpot) * input.color.rgb;
        // Ambient light for uniform visibility and smooth shading without pitch black shadows
        float3 ambient = gMaterial.color.rgb * textureColor.rgb * 0.35f;

        // Add environment map lighting if enabled
        if (gMaterial.enableEnvironmentMap != 0) {
            specularTotal += environmentColor.rgb * gMaterial.environmentCoefficient;
        }

        // Calculate final alpha
        float alpha = gMaterial.color.a * textureColor.a * input.color.a;

        // Compensate specular intensity under alpha blending
        if (alpha > 0.0f && alpha < 1.0f) {
            float safeAlpha = max(alpha, 0.01f);
            specularTotal /= safeAlpha;
        }

        outputColor.rgb = diffuseTotal + ambient + specularTotal;
        outputColor.a = alpha;

    } else {
        outputColor = gMaterial.color * textureColor * input.color;
    }

    return outputColor;
}