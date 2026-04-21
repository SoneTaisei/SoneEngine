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
    float4 outputColor;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    if (textureColor.a < 0.5f) {
        discard;
    }
    
    if (gMaterial.lightingType == 1) {
        float3 normal = normalize(input.normal);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        
        float3 directionalLightDir = normalize(-gDirectionalLight.direction);
        float directionalNdotL = dot(normal, directionalLightDir);
        float directionalCos = saturate(directionalNdotL);
        float3 diffuseDirectional = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * directionalCos * gDirectionalLight.intensity;
        
        float3 directionalHalfVector = normalize(directionalLightDir + toEye);
        float directionalNdotH = dot(normal, directionalHalfVector);
        float directionalSpecularPow = pow(saturate(directionalNdotH), gMaterial.shininess);
        float3 specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * directionalSpecularPow * float3(1.0f, 1.0f, 1.0f);
        
        
        float distance = length(gPointLight.position - input.worldPosition);
        float factor = pow(saturate(-distance/gPointLight.radius+1.0),gPointLight.decay);
        
        float3 pointLightDir = normalize(input.worldPosition - gPointLight.position);
        float pointNdotL = dot(normal, -pointLightDir);
        float pointCos = saturate(pointNdotL);
        
        float3 diffusePoint = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * pointCos * gPointLight.intensity * factor;
        
        float3 pointHalfVector = normalize(-pointLightDir + toEye);
        float pointNdotH = dot(normal, pointHalfVector);
        float pointSpecularPow = pow(saturate(pointNdotH), gMaterial.shininess);
        
        float3 specularPoint = gPointLight.color.rgb * gPointLight.intensity * pointSpecularPow * float3(1.0f, 1.0f, 1.0f) * factor;
        
        float3 spotLightDirOnSurface = normalize(input.worldPosition - gSpotLight.position);
        
        float spotDistance = length(gSpotLight.position - input.worldPosition);
        float spotAttenuation = pow(saturate(1.0f - (spotDistance / gSpotLight.distance)), gSpotLight.decay);
        
        float cosTheta = dot(spotLightDirOnSurface, normalize(gSpotLight.direction));
        float falloffRange = gSpotLight.cosFalloffStart - gSpotLight.cosAngle;
        float falloffFactor = saturate((cosTheta - gSpotLight.cosAngle) / max(0.001f, falloffRange));
        
        float spotCos = saturate(dot(normal, -spotLightDirOnSurface));
        float3 diffuseSpot = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * spotCos * gSpotLight.intensity * spotAttenuation * falloffFactor;
        
        float3 spotHalfVector = normalize(-spotLightDirOnSurface + toEye);
        float spotNdotH = dot(normal, spotHalfVector);
        float spotSpecularPow = pow(saturate(spotNdotH), gMaterial.shininess);
        float3 specularSpot = gSpotLight.color.rgb * gSpotLight.intensity * spotSpecularPow * float3(1.0f, 1.0f, 1.0f) * spotAttenuation * falloffFactor;
        
        float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
        float4 environmentColor = gEnvironmentMap.Sample(gSampler, reflectedVector);

        // --- 3. 最終色の合成 ---

        // まず、全てのライティング（拡散反射・鏡面反射）の合計値を計算する
        outputColor.rgb = diffuseDirectional + specularDirectional + // 平行光源
                          diffusePoint + specularPoint + // ポイントライト
                          diffuseSpot + specularSpot; // スポットライト

        // 環境マップによるLightingを追加する（置き換えるのではなく、そのまま足す）
        outputColor.rgb += environmentColor.rgb;

// アルファ値の設定
        outputColor.a = gMaterial.color.a * textureColor.a;

    } else {
        outputColor = gMaterial.color * textureColor;
    }

    return outputColor;
}