#include "object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<PointLight> gPointLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET {
    float4 outputColor;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 透明度による抜き処理 [cite: 16, 17]
    if (textureColor.a < 0.5f) {
        discard;
    }
    
    if (gMaterial.lightingType == 1) {
        float3 normal = normalize(input.normal);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // --- 1. Directional Light (平行光源) --- [cite: 19, 20, 21, 22, 23]
        float3 directionalLightDir = normalize(-gDirectionalLight.direction);
        float directionalNdotL = dot(normal, directionalLightDir);
        float directionalCos = saturate(directionalNdotL);
        float3 diffuseDirectional = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * directionalCos * gDirectionalLight.intensity;
        
        float3 directionalHalfVector = normalize(directionalLightDir + toEye);
        float directionalNdotH = dot(normal, directionalHalfVector);
        float directionalSpecularPow = pow(saturate(directionalNdotH), gMaterial.shininess);
        float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * directionalSpecularPow * float3(1.0f, 1.0f, 1.0f);

        // --- 2. Point Light (ポイントライト) 【資料に基づき減衰を追加！】 --- [cite: 24, 25, 26, 27, 28]
        
        // 💡 逆二乗則による減衰係数の計算
        float distance = length(gPointLight.position - input.worldPosition); // ポイントライトへの距離
        float factor = pow(saturate(-distance/gPointLight.radius+1.0),gPointLight.decay); // 逆二乗則による減衰係数

        // ライトの向きと法線の計算
        float3 pointLightDir = normalize(input.worldPosition - gPointLight.position);
        float pointNdotL = dot(normal, -pointLightDir);
        float pointCos = saturate(pointNdotL);
        
        // 💡 拡散反射に減衰係数(factor)を乗算
        float3 diffusePoint = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * pointCos * gPointLight.intensity * factor;
        
        // ポイントライトの鏡面反射
        float3 pointHalfVector = normalize(-pointLightDir + toEye);
        float pointNdotH = dot(normal, pointHalfVector);
        float pointSpecularPow = pow(saturate(pointNdotH), gMaterial.shininess);
        
        // 💡 鏡面反射にも減衰係数(factor)を乗算
        float3 specularPoint = gPointLight.color.rgb * gPointLight.intensity * pointSpecularPow * float3(1.0f, 1.0f, 1.0f) * factor;

        // --- 3. 最終色の合成 --- [cite: 29, 30]
        outputColor.rgb = diffuseDirectional + specularDirectional + diffusePoint + specularPoint;
        outputColor.a = gMaterial.color.a * textureColor.a;

    } else {
        outputColor = gMaterial.color * textureColor; // ライティングなし [cite: 31]
    }

    return outputColor;
}