#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentMap : register(t1);
Texture2D<float> gShadowMap : register(t2);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<PointLight> gPointLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<SpotLightGroup> gSpotLightGroup : register(b4);

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
        
        // 3. Multi Spot Lights (Soft Half-Lambert diffuse and guarded specular)
        float3 diffuseSpotTotal = float3(0.0f, 0.0f, 0.0f);
        float3 specularSpotTotal = float3(0.0f, 0.0f, 0.0f);
        
        int activeSpotCount = min(gSpotLightGroup.spotLightCount, (int)kMaxSpotLights);
        for (int i = 0; i < activeSpotCount; ++i) {
            if (gSpotLightGroup.spotLights[i].enable == 0) {
                continue;
            }
            SpotLight sl = gSpotLightGroup.spotLights[i];
            
            float3 spotLightDirOnSurface = normalize(input.worldPosition - sl.position);
            float spotDistance = length(sl.position - input.worldPosition);
            
            // Distance attenuation
            float spotAttenuation = pow(saturate(1.0f - (spotDistance / max(0.0001f, sl.distance))), sl.decay);
            
            // Angular falloff: full intensity inside core angle (cosFalloffStart), smoothly falls off to 0 outside (cosAngle)
            float cosTheta = dot(spotLightDirOnSurface, normalize(sl.direction));
            float falloffRange = sl.cosFalloffStart - sl.cosAngle;
            float rawFalloff = saturate((cosTheta - sl.cosAngle) / max(0.0001f, falloffRange));
            // Smoothstep curve for clear distinction between safe falloff zone and dangerous core zone
            float falloffFactor = smoothstep(0.0f, 1.0f, rawFalloff);
            
            float spotNdotL = dot(normal, -spotLightDirOnSurface);
            float spotHalfLambert = spotNdotL * 0.5f + 0.5f;
            float spotCos = spotHalfLambert * spotHalfLambert;

            // Shadow mapping evaluation
            float shadowFactor = 1.0f;
            if (sl.shadowMapIndex >= 0) {
                float4 lightSpacePos = mul(float4(input.worldPosition, 1.0f), sl.viewProjection);
                if (lightSpacePos.w > 0.0f) {
                    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
                    float2 shadowUV;
                    shadowUV.x = projCoords.x * 0.5f + 0.5f;
                    shadowUV.y = -projCoords.y * 0.5f + 0.5f;
                    float currentDepth = projCoords.z;

                    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
                        shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
                        currentDepth >= 0.0f && currentDepth <= 1.0f) {
                        
                        // Adaptive slope-scale depth bias to prevent shadow acne
                        float nDotL = saturate(dot(normal, -spotLightDirOnSurface));
                        float slopeFactor = 1.0f - nDotL;
                        float bias = max(sl.shadowBias * (1.0f + slopeFactor * 2.0f), 0.0002f);
                        
                        // 3x3 Percentage-Closer Filtering (PCF)
                        float shadow = 0.0f;
                        float2 texelSize = float2(1.0f / 2048.0f, 1.0f / 2048.0f);
                        [unroll]
                        for (int x = -1; x <= 1; ++x) {
                            [unroll]
                            for (int y = -1; y <= 1; ++y) {
                                shadow += gShadowMap.SampleCmpLevelZero(
                                    gShadowSampler,
                                    shadowUV + float2(x, y) * texelSize,
                                    currentDepth - bias
                                );
                            }
                        }
                        shadow /= 9.0f;
                        shadowFactor = lerp(1.0f - sl.shadowIntensity, 1.0f, shadow);
                    }
                }
            }

            diffuseSpotTotal += gMaterial.color.rgb * textureColor.rgb * sl.color.rgb * spotCos * sl.intensity * spotAttenuation * falloffFactor * shadowFactor;
            
            float3 spotHalfVector = normalize(-spotLightDirOnSurface + toEye);
            float spotNdotH = dot(normal, spotHalfVector);
            float spotSpecularPow = (gMaterial.shininess > 0.0f) ? pow(saturate(spotNdotH), gMaterial.shininess) : 0.0f;
            float spotSpecularMask = saturate(spotNdotL * 2.0f);
            specularSpotTotal += sl.color.rgb * sl.intensity * (spotSpecularPow * spotSpecularMask) * float3(1.0f, 1.0f, 1.0f) * spotAttenuation * falloffFactor * shadowFactor;
        }
        
        float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
        float4 environmentColor = gEnvironmentMap.Sample(gSampler, reflectedVector);

        // 4. Final color composition
        float3 diffuseTotal = (diffuseDirectional + diffusePoint + diffuseSpotTotal) * input.color.rgb;
        float3 specularTotal = (specularDirectional + specularPoint + specularSpotTotal) * input.color.rgb;
        // Ambient light modulated by ambientIntensity (allows creating full pitch black darkness)
        float3 ambient = gMaterial.color.rgb * textureColor.rgb * input.color.rgb * (0.35f * gSpotLightGroup.ambientIntensity);

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

    } else if (gMaterial.lightingType == 2) {
        // Crystal / Gemstone Shading Mode (e.g. Uzushio Crystal)
        // Computes thin-film iridescence, fake internal refraction, inner scattering glow, sharp facet specular, and rim highlights.

        // 1. Calculate facet-enhanced normal using screen-space derivatives
        float3 dpdx = ddx(input.worldPosition);
        float3 dpdy = ddy(input.worldPosition);
        float3 flatNormal = normalize(cross(dpdx, dpdy));
        float3 smoothNormal = normalize(input.normal);
        if (dot(flatNormal, smoothNormal) < 0.0f) {
            flatNormal = -flatNormal;
        }
        // Strongly favor flatNormal to give faceted, crisp crystal cuts
        float3 normal = normalize(lerp(smoothNormal, flatNormal, 0.85f));
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // 2. View and Fresnel calculations
        float NdotV = saturate(dot(normal, toEye));
        float fresnel = 1.0f - NdotV;
        float fresnelPow = pow(fresnel, 2.5f);

        // 3. Multi-band Iridescence (Thin-film interference / Prism dispersion)
        // Dynamically shifts between base amber, vivid pink/magenta, deep purple, and golden yellow based on viewing angle
        float dispersionShift = frac(fresnel * 1.35f + dot(normal, float3(0.35f, 0.65f, 0.25f)) * 0.45f);
        float3 iridColor;
        if (dispersionShift < 0.33f) {
            float t = dispersionShift / 0.33f;
            iridColor = lerp(gMaterial.color.rgb, float3(1.0f, 0.12f, 0.58f), t); // Amber to Vivid Pink
        } else if (dispersionShift < 0.66f) {
            float t = (dispersionShift - 0.33f) / 0.33f;
            iridColor = lerp(float3(1.0f, 0.12f, 0.58f), float3(0.65f, 0.18f, 0.95f), t); // Pink to Violet
        } else {
            float t = (dispersionShift - 0.66f) / 0.34f;
            iridColor = lerp(float3(0.65f, 0.18f, 0.95f), float3(1.0f, 0.90f, 0.35f), t); // Violet to Golden Yellow
        }

        // 4. Incident Lighting and Ambient Illumination evaluation
        // Calculate the light energy arriving at the crystal so it darkens proportionally with scene lighting.
        float3 totalDirectDiffuse = float3(0.0f, 0.0f, 0.0f);
        float3 crystalSpecularTotal = float3(0.0f, 0.0f, 0.0f);
        float baseShininess = max(gMaterial.shininess, 48.0f);

        // (a) Directional Light
        float3 directionalLightDir = normalize(-gDirectionalLight.direction);
        float directionalNdotL = dot(normal, directionalLightDir);
        float directionalHalfLambert = directionalNdotL * 0.5f + 0.5f;
        float dirLightingIntensity = directionalHalfLambert * directionalHalfLambert * gDirectionalLight.intensity;
        totalDirectDiffuse += gDirectionalLight.color.rgb * dirLightingIntensity;

        float3 directionalHalfVector = normalize(directionalLightDir + toEye);
        float directionalNdotH = dot(normal, directionalHalfVector);
        float dirSurfaceSpec = pow(saturate(directionalNdotH), baseShininess);
        float dirFacetSpec = pow(saturate(directionalNdotH), baseShininess * 0.4f);
        crystalSpecularTotal += gDirectionalLight.color.rgb * gDirectionalLight.intensity * (
            dirSurfaceSpec * float3(1.0f, 1.0f, 1.0f) * 1.8f + // Crisp white surface reflection
            dirFacetSpec * iridColor * 1.1f                     // Colored inner facet sparkle
        );

        // (b) Point Light
        float pDist = length(gPointLight.position - input.worldPosition);
        float pFactor = pow(saturate(-pDist / max(0.0001f, gPointLight.radius) + 1.0f), gPointLight.decay);
        float3 pointLightDir = normalize(input.worldPosition - gPointLight.position);
        float pointNdotL = dot(normal, -pointLightDir);
        float pointHalfLambert = pointNdotL * 0.5f + 0.5f;
        float pLightingIntensity = pointHalfLambert * pointHalfLambert * gPointLight.intensity * pFactor;
        totalDirectDiffuse += gPointLight.color.rgb * pLightingIntensity;

        float3 pointHalfVector = normalize(-pointLightDir + toEye);
        float pointNdotH = dot(normal, pointHalfVector);
        float pointSurfaceSpec = pow(saturate(pointNdotH), baseShininess);
        crystalSpecularTotal += gPointLight.color.rgb * gPointLight.intensity * pointSurfaceSpec * pFactor * float3(1.0f, 1.0f, 1.0f) * 1.5f;

        // (c) Spot Lights (with attenuation, falloff, and shadow mapping)
        int activeSpotCount = min(gSpotLightGroup.spotLightCount, (int)kMaxSpotLights);
        for (int i = 0; i < activeSpotCount; ++i) {
            if (gSpotLightGroup.spotLights[i].enable == 0) continue;
            SpotLight sl = gSpotLightGroup.spotLights[i];
            float3 spotLightDirOnSurface = normalize(input.worldPosition - sl.position);
            float spotDistance = length(sl.position - input.worldPosition);
            float spotAttenuation = pow(saturate(1.0f - (spotDistance / max(0.0001f, sl.distance))), sl.decay);

            float cosTheta = dot(spotLightDirOnSurface, normalize(sl.direction));
            float falloffRange = sl.cosFalloffStart - sl.cosAngle;
            float rawFalloff = saturate((cosTheta - sl.cosAngle) / max(0.0001f, falloffRange));
            float falloffFactor = smoothstep(0.0f, 1.0f, rawFalloff);

            float spotNdotL = dot(normal, -spotLightDirOnSurface);
            float spotHalfLambert = spotNdotL * 0.5f + 0.5f;

            // Shadow mapping evaluation
            float shadowFactor = 1.0f;
            if (sl.shadowMapIndex >= 0) {
                float4 lightSpacePos = mul(float4(input.worldPosition, 1.0f), sl.viewProjection);
                if (lightSpacePos.w > 0.0f) {
                    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
                    float2 shadowUV;
                    shadowUV.x = projCoords.x * 0.5f + 0.5f;
                    shadowUV.y = -projCoords.y * 0.5f + 0.5f;
                    float currentDepth = projCoords.z;

                    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
                        shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
                        currentDepth >= 0.0f && currentDepth <= 1.0f) {
                        float nDotL = saturate(dot(normal, -spotLightDirOnSurface));
                        float bias = max(sl.shadowBias * (1.0f + (1.0f - nDotL) * 2.0f), 0.0002f);
                        float shadow = 0.0f;
                        float2 texelSize = float2(1.0f / 2048.0f, 1.0f / 2048.0f);
                        [unroll]
                        for (int x = -1; x <= 1; ++x) {
                            [unroll]
                            for (int y = -1; y <= 1; ++y) {
                                shadow += gShadowMap.SampleCmpLevelZero(
                                    gShadowSampler,
                                    shadowUV + float2(x, y) * texelSize,
                                    currentDepth - bias
                                );
                            }
                        }
                        shadow /= 9.0f;
                        shadowFactor = lerp(1.0f - sl.shadowIntensity, 1.0f, shadow);
                    }
                }
            }

            float spotLightingIntensity = spotHalfLambert * spotHalfLambert * sl.intensity * spotAttenuation * falloffFactor * shadowFactor;
            totalDirectDiffuse += sl.color.rgb * spotLightingIntensity;

            float3 spotHalfVector = normalize(-spotLightDirOnSurface + toEye);
            float spotNdotH = dot(normal, spotHalfVector);
            float spotSurfaceSpec = pow(saturate(spotNdotH), baseShininess);
            crystalSpecularTotal += sl.color.rgb * sl.intensity * spotSurfaceSpec * spotAttenuation * falloffFactor * shadowFactor * float3(1.0f, 1.0f, 1.0f) * 1.5f;
        }

        // (d) Ambient light modulation
        float ambientEnergy = gSpotLightGroup.ambientIntensity;

        // (e) Normalized lighting scale [0.0 = completely pitch dark, 1.0 = standard lit environment]
        // Standard lit scenes have ambient ~ 1.0 and direct light ~ 0.5 - 1.0.
        // By normalizing against 0.75f, standard lit rooms yield lightingScale == 1.0 (preserving 100% of the crystal's exact vibrant look).
        // When scene lights are turned off or ambient drops to 0 in dark rooms, lightingScale smoothly fades to 0.0 (completely dark).
        float totalDirectLightEnergy = dirLightingIntensity + pLightingIntensity + totalDirectDiffuse.r * 0.5f;
        float totalSceneIllum = totalDirectLightEnergy + ambientEnergy * 0.65f;
        float lightingScale = saturate(totalSceneIllum / 0.75f);

        // 5. Inner glow (scaled by lightingScale so it darkens in dark environments, but retains 100% of original vibrant look when lit)
        float innerGlowFactor = pow(NdotV, 1.2f) * 0.75f + 0.35f;
        float3 baseInnerGlow = lerp(gMaterial.color.rgb, iridColor, 0.55f) * innerGlowFactor;
        float3 litInnerGlow = baseInnerGlow * lightingScale;

        // 6. Fake Refraction modulated by lighting scale
        float3 refractDir = refract(-toEye, normal, 1.0f / 1.33f);
        if (length(refractDir) < 0.01f) {
            refractDir = reflect(-toEye, normal); // Total internal reflection fallback
        }
        float4 envRefractColor = gEnvironmentMap.Sample(gSampler, refractDir);
        float envCoeff = (gMaterial.enableEnvironmentMap != 0) ? max(gMaterial.environmentCoefficient, 0.5f) : 0.5f;
        float3 refractedLight = envRefractColor.rgb * iridColor * envCoeff * 1.1f * lightingScale;

        // 7. Environment Mirror Reflection modulated by lighting scale
        float3 reflectDir = reflect(-toEye, normal);
        float4 envReflectColor = gEnvironmentMap.Sample(gSampler, reflectDir);
        float3 envSpecular = envReflectColor.rgb * envCoeff * (fresnelPow * 0.85f + 0.15f) * lightingScale;

        // 8. Outer Rim Highlight modulated by lighting scale
        float rimFactor = pow(fresnel, 3.0f);
        float3 rimColor = lerp(float3(0.20f, 0.85f, 1.0f), float3(1.0f, 0.35f, 0.80f), dispersionShift);
        float3 rimLight = rimColor * rimFactor * 1.4f * lightingScale;

        // 9. Final Color Composition
        // In standard lit conditions (lightingScale == 1.0), this matches the original crystal visual 1:1.
        // In dark conditions (lightingScale -> 0.0), all terms smoothly scale down to pitch black.
        float3 crystalColor = litInnerGlow * 0.85f + refractedLight * 0.8f + crystalSpecularTotal + envSpecular + rimLight;
        outputColor.rgb = crystalColor * input.color.rgb;
        outputColor.a = gMaterial.color.a * textureColor.a * input.color.a;

    } else {
        outputColor = gMaterial.color * textureColor * input.color;
    }

    return outputColor;
}