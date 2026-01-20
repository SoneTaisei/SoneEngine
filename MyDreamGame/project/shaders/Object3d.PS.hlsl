#include "object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
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
    
    if (gMaterial.lightingType == 2) {
        float3 normal = normalize(input.normal);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float3 lightDir = normalize(-gDirectionalLight.direction);

    // 1. フェイク屈折 (UV歪み)
    // 法線のXY成分を使ってUVを少しずらすと、レンズ越しに見ているような歪みが生まれます
    // 0.1f は歪みの強さ。大きくすると分厚いガラスになります
        float2 distortedUV = transformedUV.xy + (normal.xy * 0.1f);
    
    // 模様テクスチャをサンプリング
        float4 innerColor = gTexture.Sample(gSampler, distortedUV);

    // 2. フレネル反射 (縁の計算)
    // 視線と法線の角度。正面(1.0)～縁(0.0)
        float NdotV = saturate(dot(normal, toEye));
    // 縁に行くほど強くなる値 (3.0乗でカーブを調整)
        float fresnel = pow(1.0f - NdotV, 3.0f);

    // 3. 環境色（空の色）の合成
    // ビー玉は縁に行くほど、中の模様より「表面の反射」が強くなります
        float3 skyColor = float3(0.8f, 0.9f, 1.0f); // 薄い水色（環境光の代わり）
    
    // 中の模様(innerColor) と 環境色(skyColor) を フレネル(fresnel) で混ぜる
        float3 bodyColor = lerp(innerColor.rgb, skyColor, fresnel * 0.8f);

    // 4. 鋭いスペキュラ (光沢)
    // ガラスは硬いので、ハイライトは小さく鋭くします (Shininess高め)
        float3 halfVector = normalize(lightDir + toEye);
        float specularPow = pow(saturate(dot(normal, halfVector)), 256.0f); // 256など高い値
        float3 specular = float3(1.0f, 1.0f, 1.0f) * gDirectionalLight.intensity * specularPow;

    // 5. 最終合成
        outputColor.rgb = bodyColor * gMaterial.color.rgb + specular;
    
    // 透明度の調整 (中心は少し透けて、縁は不透明)
        float alphaBase = 0.4f; // 基礎透明度
        outputColor.a = saturate(alphaBase + fresnel) * gMaterial.color.a;
    
    } else if (gMaterial.lightingType != 0) {
        
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        
        float NdotL = dot(normal, lightDir);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f); // Half-Lambert
        if (gMaterial.lightingType == 1) {
            cos = saturate(NdotL); // Lambert
        }
        
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        float3 halfVector = normalize(lightDir + toEye);
        
        float NDotH = dot(normal, halfVector);
        
        float specularPow = pow(saturate(NDotH), gMaterial.shininess);
        
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
        
        outputColor.rgb = diffuse + specular;
        outputColor.a = gMaterial.color.a * textureColor.a;

    } else {
        outputColor = gMaterial.color * textureColor;
    }

    return outputColor;
}