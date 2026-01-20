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
    
    if (textureColor.a < 0.5f) {
        discard;
    }
    
    if (gMaterial.lightingType == 1) {
        
        float3 normal = normalize(input.normal);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // ===============================================
        // 1. Directional Light (平行光源) の計算
        // ===============================================
        float3 directionalLightDir = normalize(-gDirectionalLight.direction);
        
        float directionalNdotL = dot(normal, directionalLightDir);
        float directionalCos = pow(directionalNdotL * 0.5f + 0.5f, 2.0f); // Half-Lambert
        if (gMaterial.lightingType == 1) {
            directionalCos = saturate(directionalNdotL); // Lambert
        }
        
        // 変数名を diffuse → diffuseDirectional に変更してわかりやすく
        float3 diffuseDirectional = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * directionalCos * gDirectionalLight.intensity;
        
        float3 directionalHalfVector = normalize(directionalLightDir + toEye);
        float directionalNdotH = dot(normal, directionalHalfVector);
        float directionalSpecularPow = pow(saturate(directionalNdotH), gMaterial.shininess);
        
        // 変数名を specular → specularDirectional に変更
        float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * directionalSpecularPow * float3(1.0f, 1.0f, 1.0f);

        // ===============================================
        // 2. Point Light (ポイントライト) の計算 【ここに追加！】
        // ===============================================
        // 資料スクリーンショット1の計算式
        // input.worldPosition - gPointLight.position は「ライト→床」の向きになるので、
        // 入射光として扱うならこれでOKですが、拡散反射(dot)の計算には「床→ライト」の向き（-pointLightDir）を使います。
        float3 pointLightDir = normalize(input.worldPosition - gPointLight.position);
        
        float pointNdotL = dot(normal, -pointLightDir); // 逆向きにして内積
        float pointCos = saturate(pointNdotL); // ポイントライトは基本Lambertで計算
        
        // 距離による減衰（Intensityだけ使う簡易版）
        // ※もし距離減衰(Attentuation)を実装するならここで割り算などが入りますが、資料に合わせてIntensityのみにします
        float3 diffusePoint = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * pointCos * gPointLight.intensity;
        
        // ポイントライトの鏡面反射
        float3 pointHalfVector = normalize(-pointLightDir + toEye);
        float pointNdotH = dot(normal, pointHalfVector);
        float pointSpecularPow = pow(saturate(pointNdotH), gMaterial.shininess);
        
        float3 specularPoint = gPointLight.color.rgb * gPointLight.intensity * pointSpecularPow * float3(1.0f, 1.0f, 1.0f);

        // ===============================================
        // 3. 全部足す (資料スクリーンショット2)
        // ===============================================
        outputColor.rgb = diffuseDirectional + specularDirectional + diffusePoint + specularPoint;
        
        outputColor.a = gMaterial.color.a * textureColor.a;

    } else {
        outputColor = gMaterial.color * textureColor;
    }

    return outputColor;
}