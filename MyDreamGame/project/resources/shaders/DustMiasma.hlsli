// Dust / Miasma / Celeste Dust Bunny Procedural Shading Helper
// All comments in English as required by guidelines.

#ifndef DUST_MIASMA_HLSLI
#define DUST_MIASMA_HLSLI

// 2D Hash function
float dust_hash21(float2 p) {
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

// 3D Hash function
float dust_hash31(float3 p) {
    p = frac(p * float3(123.34f, 456.21f, 789.12f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y * p.z);
}

// 3D Value Noise with cubic Hermite interpolation
float dust_noise3d(float3 p) {
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float n000 = dust_hash31(i + float3(0.0f, 0.0f, 0.0f));
    float n100 = dust_hash31(i + float3(1.0f, 0.0f, 0.0f));
    float n010 = dust_hash31(i + float3(0.0f, 1.0f, 0.0f));
    float n110 = dust_hash31(i + float3(1.0f, 1.0f, 0.0f));
    float n001 = dust_hash31(i + float3(0.0f, 0.0f, 1.0f));
    float n101 = dust_hash31(i + float3(1.0f, 0.0f, 1.0f));
    float n011 = dust_hash31(i + float3(0.0f, 1.0f, 1.0f));
    float n111 = dust_hash31(i + float3(1.0f, 1.0f, 1.0f));
    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

// 3D Billowing Noise for puffy, volumetric cloud lobes
float dust_billow3d(float3 p) {
    float v = 0.0f;
    float a = 0.5f;
    for (int i = 0; i < 3; ++i) {
        float n = dust_noise3d(p);
        // Inverted absolute creates puffy rounded lobes instead of sharp fire peaks
        float lobe = 1.0f - abs(n * 2.0f - 1.0f);
        v += a * lobe;
        p = p * 2.05f + float3(1.7f, 9.2f, 3.4f);
        a *= 0.5f;
    }
    return v;
}

// 2D Value Noise with cubic Hermite interpolation
float dust_noise2d(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = dust_hash21(i);
    float b = dust_hash21(i + float2(1.0f, 0.0f));
    float c = dust_hash21(i + float2(0.0f, 1.0f));
    float d = dust_hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// Fractal Brownian Motion (FBM)
float dust_fbm2d(float2 p) {
    float v = 0.0f;
    float a = 0.5f;
    for (int i = 0; i < 3; ++i) {
        v += a * dust_noise2d(p);
        p = p * 2.08f + float2(1.7f, 9.2f);
        a *= 0.5f;
    }
    return v;
}

// Cellular spherical puff field: creates 3D overlapping cloud spheres with 3D surface normals and crease detection
float dust_sphericalPuffs(float2 p, float t, out float3 outPuffNormal, out float outSecondDome) {
    float2 i = floor(p + 0.5f);
    float2 f = p - i;
    float maxDome = 0.0f;
    float secondDome = 0.0f;
    outPuffNormal = float3(0.0f, 0.0f, -1.0f);
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 neighbor = float2((float)x, (float)y);
            float2 cellId = i + neighbor;
            float h1 = dust_hash21(cellId);
            float h2 = dust_hash21(cellId + float2(17.3f, 31.7f));
            float angle = t * (0.85f + h1 * 0.55f) + h1 * 6.28318f;
            float2 center = neighbor + 0.30f * float2(cos(angle), sin(angle * 0.85f + h2 * 6.28f));
            float radius = 0.78f + 0.24f * sin(t * 1.05f + h2 * 6.28318f);
            float2 diff = f - center;
            float dist = length(diff);
            float normDist = saturate(dist / radius);
            // Spherical dome height: 1 at center, 0 at sphere boundary
            float dome = 1.0f - normDist * normDist;
            if (dome > maxDome) {
                secondDome = maxDome;
                maxDome = dome;
                // Reconstruct 3D hemisphere normal for this cloud puff
                float2 nxy = diff / max(radius, 0.001f);
                float nz = -sqrt(max(1.0f - dot(nxy, nxy), 0.05f));
                outPuffNormal = normalize(float3(nxy.x, nxy.y, nz));
            } else if (dome > secondDome) {
                secondDome = dome;
            }
        }
    }
    outSecondDome = secondDome;
    return maxDome;
}

// Fast 2D spherical puff density evaluation (matches dust_sphericalPuffs dome height)
float dust_sphericalPuffsDensity(float2 p, float t) {
    float2 i = floor(p + 0.5f);
    float2 f = p - i;
    float maxDome = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 neighbor = float2((float)x, (float)y);
            float2 cellId = i + neighbor;
            float h1 = dust_hash21(cellId);
            float h2 = dust_hash21(cellId + float2(17.3f, 31.7f));
            float angle = t * (0.85f + h1 * 0.55f) + h1 * 6.28318f;
            float2 center = neighbor + 0.30f * float2(cos(angle), sin(angle * 0.85f + h2 * 6.28f));
            float radius = 0.78f + 0.24f * sin(t * 1.05f + h2 * 6.28318f);
            float dist = length(f - center);
            float normDist = saturate(dist / radius);
            float dome = 1.0f - normDist * normDist;
            maxDome = max(maxDome, dome);
        }
    }
    return maxDome;
}

// Shades a spherical puff core pixel using the exact 2D/3D overlapping dome lighting and crevice rims
float4 ShadeMiasmaPuffCore(
    float2 puffSamplePos,
    float t,
    float edgeMargin,
    float dome1,
    float3 puffNormal1,
    float secondDome1,
    float3 viewDir3D,
    float zDepthShade
) {
    float2 flowPos = puffSamplePos - float2(0.0f, t * 0.18f);
    float3 puffNormal2;
    float secondDome2;
    float dome2 = dust_sphericalPuffs(flowPos * 3.7f + float2(4.1f, 2.3f), t * 1.25f, puffNormal2, secondDome2);

    float3 combinedNormal = normalize(puffNormal1 * 0.70f + puffNormal2 * 0.30f);

    // 3D directional & view lighting across each spherical cloud puff
    float3 keyLightDir = normalize(float3(-0.35f, 0.65f, -0.65f));
    float puffDiffuse = saturate(dot(combinedNormal, keyLightDir) * 0.5f + 0.5f);
    float puffFacing = saturate(dot(combinedNormal, viewDir3D));
    float puffRim3D = 1.0f - puffFacing;

    // Crevice & overlapping rim detection between adjacent cloud spheres
    float overlapDiff1 = dome1 - secondDome1;
    float overlapDiff2 = dome2 - secondDome2;

    // Deep ambient occlusion in the crease between spheres
    float creaseAO = smoothstep(0.0f, 0.045f, overlapDiff1) * smoothstep(0.0f, 0.04f, overlapDiff2);

    // Curved internal crimson/maroon rim highlight on the edge of foreground spheres
    float internalPuffRim1 = (1.0f - smoothstep(0.03f, 0.095f, overlapDiff1)) * step(0.022f, overlapDiff1);
    float internalPuffRim2 = (1.0f - smoothstep(0.025f, 0.08f, overlapDiff2)) * step(0.018f, overlapDiff2);
    float internalRimMask = saturate(max(internalPuffRim1, internalPuffRim2 * 0.7f) * (0.4f + 0.6f * puffDiffuse));

    // Base volumetric palette
    float3 abyssalBlack   = float3(0.015f, 0.008f, 0.025f);
    float3 deepPlumPuff   = float3(0.14f, 0.022f, 0.085f);
    float3 litPuffCrest   = float3(0.24f, 0.035f, 0.12f);
    float3 innerCrimson   = float3(0.58f, 0.05f, 0.15f);

    // Outer shell vs deep interior modulation
    float outerShell = 1.0f - smoothstep(0.055f, 0.32f, edgeMargin);

    // Shade each sphere from deep abyssal black in the crevices to dark plum/crimson on crests
    float3 puffColor = lerp(abyssalBlack, deepPlumPuff, saturate(dome1 * puffDiffuse * 0.85f * zDepthShade));
    puffColor = lerp(puffColor, litPuffCrest, outerShell * puffDiffuse * smoothstep(0.45f, 0.85f, puffRim3D) * zDepthShade);

    // Apply crease ambient occlusion
    puffColor *= (0.35f + 0.65f * creaseAO);

    // Add curved internal puff rim highlights so stacked cloud layers pop out in 3D
    puffColor = lerp(puffColor, innerCrimson * zDepthShade, internalRimMask * (0.45f + 0.55f * outerShell));

    return float4(puffColor, 1.0f);
}

// Evaluates 2D rounded-corner distance from the 4 edge distances of a face
float EvaluateRoundedFaceDist(float d1, float d2, float d3, float d4) {
    float dA = min(d1, d2);
    float dB = min(d3, d4);
    float cornerR = 0.42f;
    float2 cornerVec = max(float2(cornerR - dA, cornerR - dB), 0.0f);
    return min(min(dA, dB), cornerR) - length(cornerVec);
}

// Computes watertight 6-face 3D billowing cloud miasma with zero raymarching and zero camera-angle disappearance
float4 ComputeDustMiasma(
    float2 uv,
    float3 worldPos,
    float3 localPos,
    float3 normal,
    float3 cameraPos,
    float4 baseCoreColor,
    float4x4 uvTransform,
    float pixelResolution,
    float customSpeed
) {
    // 0. Back-face cull in shader matching hardware rasterizer
    float3 viewToSurface = worldPos - cameraPos;
    if (dot(normal, -viewToSurface) <= 0.0f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 1. Decode 8-neighbor Miasma mask (bits 0..7) and 4-neighbor solid block mask (bits 8..11) from uvTransform[3][1]
    int rawMask = (int)round(uvTransform[3][1]);
    int neighborMask = (rawMask >= 256) ? (rawMask - 256) : 0;
    bool hasL   = (neighborMask & 1) != 0;
    bool hasR   = (neighborMask & 2) != 0;
    bool hasB   = (neighborMask & 4) != 0;
    bool hasT   = (neighborMask & 8) != 0;
    bool hasBL  = (neighborMask & 16) != 0;
    bool hasBR  = (neighborMask & 32) != 0;
    bool hasTL  = (neighborMask & 64) != 0;
    bool hasTR  = (neighborMask & 128) != 0;
    bool solidL = (neighborMask & 256) != 0;
    bool solidR = (neighborMask & 512) != 0;
    bool solidB = (neighborMask & 1024) != 0;
    bool solidT = (neighborMask & 2048) != 0;

    // Discard internal or blocked 3D side/top/bottom faces immediately
    if (normal.x < -0.5f && (hasL || solidL)) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (normal.x >  0.5f && (hasR || solidR)) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (normal.y < -0.5f && (hasB || solidB)) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (normal.y >  0.5f && (hasT || solidT)) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 2. Physical 3D box half-extents (+0.06 overhang per side) and virtual SDF half-extents (+0.24 overhang)
    float3 spanSize = max(float3(uvTransform[0][0], uvTransform[1][1], uvTransform[2][2]), float3(1.0f, 1.0f, 1.0f));
    float3 nominalHalf = spanSize * 0.5f;
    float3 boxHalf = nominalHalf + float3(0.06f, 0.06f, 0.06f);
    float3 effHalf = nominalHalf + float3(0.24f, 0.24f, 0.24f);
    float3 p0 = localPos * (boxHalf * 2.0f);
    float3 boxCenter = worldPos - p0;

    // Exact visibility of the 6 faces of this block from cameraPos
    bool visL    = !hasL && !solidL && (cameraPos.x < boxCenter.x - boxHalf.x);
    bool visR    = !hasR && !solidR && (cameraPos.x > boxCenter.x + boxHalf.x);
    bool visB    = !hasB && !solidB && (cameraPos.y < boxCenter.y - boxHalf.y);
    bool visT    = !hasT && !solidT && (cameraPos.y > boxCenter.y + boxHalf.y);
    bool visF    = (cameraPos.z < boxCenter.z - boxHalf.z);
    bool visBack = (cameraPos.z > boxCenter.z + boxHalf.z);

    // Clip overhang on sides connected to another miasma tile so adjacent hulls meet flush at +/-nominalHalf
    if (hasL && p0.x < -nominalHalf.x - 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (hasR && p0.x >  nominalHalf.x + 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (hasB && p0.y < -nominalHalf.y - 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (hasT && p0.y >  nominalHalf.y + 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 45-degree miter clip in overhang regions at concave L-corners so protruding overhangs never overlap
    float dxL = -nominalHalf.x - p0.x;
    float dxR =  p0.x - nominalHalf.x;
    float dyB = -nominalHalf.y - p0.y;
    float dyT =  p0.y - nominalHalf.y;
    if (hasTL) {
        if (!hasL && dxL > 0.0f && (nominalHalf.y - p0.y) < dxL) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (!hasT && dyT > 0.0f && (p0.x + nominalHalf.x) < dyT) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (hasTR) {
        if (!hasR && dxR > 0.0f && (nominalHalf.y - p0.y) < dxR) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (!hasT && dyT > 0.0f && (nominalHalf.x - p0.x) < dyT) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (hasBL) {
        if (!hasL && dxL > 0.0f && (p0.y + nominalHalf.y) < dxL) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (!hasB && dyB > 0.0f && (p0.x + nominalHalf.x) < dyB) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (hasBR) {
        if (!hasR && dxR > 0.0f && (p0.y + nominalHalf.y) < dxR) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (!hasB && dyB > 0.0f && (nominalHalf.x - p0.x) < dyB) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 3. World-Space Pixel Quantization & Continuous Time
    float pixelDensity = (pixelResolution > 1.0f) ? pixelResolution : 28.0f;
    float pixelSize = 1.0f / pixelDensity;

    float time = uvTransform[3][0];
    if (abs(time) < 0.0001f) {
        time = frac(worldPos.x * 0.05f + worldPos.y * 0.05f);
    }
    float animSpeed = (customSpeed > 0.0f) ? customSpeed : 1.2f;
    float t = time * animSpeed;

    float outlineThickness = 0.055f;
    float4 crimsonOutline = float4(0.98f, 0.08f, 0.20f, 1.0f);
    float3 viewDir3D = normalize(cameraPos - worldPos);
    float zNorm = saturate((p0.z + boxHalf.z) / max(boxHalf.z * 2.0f, 0.1f));

    // 4. Front / Back Face (abs(normal.z) > 0.5f): 100% Exact 2D Billowing Cloud Appearance + Seamless 3D Corner Bridge
    if (abs(normal.z) > 0.5f) {
        float2 samplePos = floor(worldPos.xy / pixelSize) * pixelSize;

        float dL = hasL ? 10.0f : (effHalf.x + p0.x);
        float dR = hasR ? 10.0f : (effHalf.x - p0.x);
        float dB = hasB ? 10.0f : (effHalf.y + p0.y);
        float dT = hasT ? 10.0f : (effHalf.y - p0.y);
        float distFromExpandedEdge = EvaluateRoundedFaceDist(dL, dR, dB, dT);

        float2 flowPos = samplePos - float2(0.0f, t * 0.18f);
        float3 puffNormal1;
        float secondDome1;
        float dome1 = dust_sphericalPuffs(flowPos * 1.95f, t, puffNormal1, secondDome1);
        float medPuff = dust_noise2d(flowPos * 3.8f + float2(t * 0.4f, -t * 0.5f));
        float puffField = dome1 * 0.72f + medPuff * 0.28f;

        float macroWave = sin(samplePos.x * 2.3f + t * 0.7f) * cos(samplePos.y * 2.3f - t * 0.6f) * 0.5f + 0.5f;
        float requiredDist = 0.04f + (1.0f - puffField) * 0.20f + (1.0f - macroWave) * 0.04f;
        float edgeMargin = distFromExpandedEdge - requiredDist;

        if (distFromExpandedEdge >= 0.275f) {
            edgeMargin = max(edgeMargin, 0.065f);
        }

        // Rising Spores / Hollow Crimson Bubble Rings around the Front perimeter
        bool isRisingBubble = false;
        if (edgeMargin < 0.0f && edgeMargin > -0.18f) {
            float2 bubbleCell = floor(samplePos * 1.5f);
            float cellSeed = dust_hash21(bubbleCell);
            if (cellSeed > 0.45f) {
                float bubbleCycle = frac(t * 0.45f + cellSeed);
                float2 bubbleCenter = (bubbleCell + 0.5f) / 1.5f;
                bubbleCenter.y += (bubbleCycle - 0.5f) * 0.35f;
                bubbleCenter.x += sin(bubbleCycle * 6.28f + cellSeed * 10.0f) * 0.10f;
                float bubbleRadius = 0.055f * sin(bubbleCycle * 3.14159f);
                float distToBubble = length(samplePos - bubbleCenter);
                if (abs(distToBubble - bubbleRadius) < pixelSize * 1.15f && bubbleRadius > 0.015f) {
                    isRisingBubble = true;
                }
            }
        }

        float frontShade = (normal.z < 0.0f) ? 1.0f : 0.62f;
        if (edgeMargin >= outlineThickness) {
            return ShadeMiasmaPuffCore(samplePos, t, edgeMargin, dome1, puffNormal1, secondDome1, viewDir3D, frontShade);
        } else if (edgeMargin >= 0.0f || isRisingBubble) {
            return float4(crimsonOutline.rgb * frontShade, 1.0f);
        }

        // Seamless Corner-Wrap Bridge into visible 3D Top/Bottom/Side faces (visL, visR, visB, visT)
        if (visL || visR || visB || visT) {
            if ((solidL && p0.x < -nominalHalf.x - 0.002f) ||
                (solidR && p0.x >  nominalHalf.x + 0.002f) ||
                (solidB && p0.y < -nominalHalf.y - 0.002f) ||
                (solidT && p0.y >  nominalHalf.y + 0.002f)) {
                return float4(0.0f, 0.0f, 0.0f, 0.0f);
            }
            float dL_sil = (hasL || solidL || visL) ? 10.0f : dL;
            float dR_sil = (hasR || solidR || visR) ? 10.0f : dR;
            float dB_sil = (hasB || solidB || visB) ? 10.0f : dB;
            float dT_sil = (hasT || solidT || visT) ? 10.0f : dT;
            float distSil = EvaluateRoundedFaceDist(dL_sil, dR_sil, dB_sil, dT_sil);
            float silMargin = distSil - requiredDist;
            if (distSil >= 0.275f) {
                silMargin = max(silMargin, 0.065f);
            }
            if (silMargin >= outlineThickness) {
                return ShadeMiasmaPuffCore(samplePos, t, silMargin, dome1, puffNormal1, secondDome1, viewDir3D, 0.78f);
            } else if (silMargin >= 0.0f) {
                return float4(crimsonOutline.rgb * 0.92f, 1.0f);
            }
        }
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 5. Top / Bottom 3D Faces (abs(normal.y) > 0.5f): Watertight 3D Depth Surface
    if (abs(normal.y) > 0.5f) {
        if (solidL && p0.x < -nominalHalf.x - 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
        if (solidR && p0.x >  nominalHalf.x + 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

        float zOffset = max(0.0f, worldPos.z - (boxCenter.z - boxHalf.z));
        float2 unfoldedXY = float2(worldPos.x, worldPos.y + ((normal.y > 0.0f) ? zOffset : -zOffset));
        float2 samplePos = floor(unfoldedXY / pixelSize) * pixelSize;

        float2 flowPos = samplePos - float2(0.0f, t * 0.18f);
        float3 puffNormal1;
        float secondDome1;
        float dome1 = dust_sphericalPuffs(flowPos * 1.95f, t, puffNormal1, secondDome1);
        float medPuff = dust_noise2d(flowPos * 3.8f + float2(t * 0.4f, -t * 0.5f));
        float puffField = dome1 * 0.72f + medPuff * 0.28f;
        float macroWave = sin(samplePos.x * 2.3f + t * 0.7f) * cos(samplePos.y * 2.3f - t * 0.6f) * 0.5f + 0.5f;
        float requiredDist = 0.04f + (1.0f - puffField) * 0.20f + (1.0f - macroWave) * 0.04f;

        float dL_own = (hasL || solidL) ? 10.0f : (effHalf.x + p0.x);
        float dR_own = (hasR || solidR) ? 10.0f : (effHalf.x - p0.x);
        float dF_own = visF    ? 10.0f : (effHalf.z + p0.z);
        float dB_own = visBack ? 10.0f : (effHalf.z - p0.z);
        float distOwn = EvaluateRoundedFaceDist(dL_own, dR_own, dF_own, dB_own);
        float ownMargin = distOwn - requiredDist;
        if (distOwn >= 0.275f) {
            ownMargin = max(ownMargin, 0.065f);
        }

        float topShade = lerp(0.78f, 0.52f, zNorm);
        if (ownMargin >= outlineThickness) {
            return ShadeMiasmaPuffCore(samplePos, t, ownMargin, dome1, puffNormal1, secondDome1, viewDir3D, topShade);
        } else if (ownMargin >= 0.0f) {
            return float4(crimsonOutline.rgb * lerp(0.92f, 0.68f, zNorm), 1.0f);
        }

        // Bridge Top/Bottom face into visible Left/Right side face
        if (visL || visR) {
            float dL_sil = (hasL || solidL || visL) ? 10.0f : dL_own;
            float dR_sil = (hasR || solidR || visR) ? 10.0f : dR_own;
            float distSil = EvaluateRoundedFaceDist(dL_sil, dR_sil, dF_own, dB_own);
            float silMargin = distSil - requiredDist;
            if (distSil >= 0.275f) {
                silMargin = max(silMargin, 0.065f);
            }
            if (silMargin >= outlineThickness) {
                return ShadeMiasmaPuffCore(samplePos, t, silMargin, dome1, puffNormal1, secondDome1, viewDir3D, lerp(0.72f, 0.48f, zNorm));
            } else if (silMargin >= 0.0f) {
                return float4(crimsonOutline.rgb * lerp(0.88f, 0.65f, zNorm), 1.0f);
            }
        }
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 6. Left / Right 3D Side Faces (abs(normal.x) > 0.5f): Watertight 3D Depth Surface
    if (solidB && p0.y < -nominalHalf.y - 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (solidT && p0.y >  nominalHalf.y + 0.002f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float zOffset = max(0.0f, worldPos.z - (boxCenter.z - boxHalf.z));
    float2 unfoldedXY = float2(worldPos.x + ((normal.x > 0.0f) ? zOffset : -zOffset), worldPos.y);
    float2 samplePos = floor(unfoldedXY / pixelSize) * pixelSize;

    float2 flowPos = samplePos - float2(0.0f, t * 0.18f);
    float3 puffNormal1;
    float secondDome1;
    float dome1 = dust_sphericalPuffs(flowPos * 1.95f, t, puffNormal1, secondDome1);
    float medPuff = dust_noise2d(flowPos * 3.8f + float2(t * 0.4f, -t * 0.5f));
    float puffField = dome1 * 0.72f + medPuff * 0.28f;
    float macroWave = sin(samplePos.x * 2.3f + t * 0.7f) * cos(samplePos.y * 2.3f - t * 0.6f) * 0.5f + 0.5f;
    float requiredDist = 0.04f + (1.0f - puffField) * 0.20f + (1.0f - macroWave) * 0.04f;

    float dB_sil = (hasB || solidB || visB) ? 10.0f : (effHalf.y + p0.y);
    float dT_sil = (hasT || solidT || visT) ? 10.0f : (effHalf.y - p0.y);
    float dF_sil = visF    ? 10.0f : (effHalf.z + p0.z);
    float dBack  = visBack ? 10.0f : (effHalf.z - p0.z);
    float distSide = EvaluateRoundedFaceDist(dB_sil, dT_sil, dF_sil, dBack);
    float sideMargin = distSide - requiredDist;
    if (distSide >= 0.275f) {
        sideMargin = max(sideMargin, 0.065f);
    }

    float sideShade = lerp(0.74f, 0.48f, zNorm);
    if (sideMargin >= outlineThickness) {
        return ShadeMiasmaPuffCore(samplePos, t, sideMargin, dome1, puffNormal1, secondDome1, viewDir3D, sideShade);
    } else if (sideMargin >= 0.0f) {
        return float4(crimsonOutline.rgb * lerp(0.90f, 0.65f, zNorm), 1.0f);
    }

    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

#endif // DUST_MIASMA_HLSLI
