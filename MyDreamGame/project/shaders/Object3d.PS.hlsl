#include "object3d.hlsli"

Texture2D<float4> gTexture : register(t3);
SamplerState gSampler : register(s0);

ConstantBuffer<Material> gMaterial : register(b0);

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

ConstantBuffer<Camera> gCamera : register(b2);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

cbuffer MaterialCB : register(b3) {
    Material material;
};

cbuffer TransformCB : register(b4) {
    TransformationMatrix transform;
};

float4 main(VertexShaderOutput input) : SV_TARGET {
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);
    
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
    float3 halfVector = normalize(lightDir + toEye);
    float NdotH = dot(normal, halfVector);
    float specular = pow(saturate(NdotH), 100.0f);

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
   
    if (textureColor.a < 0.5f) {
        discard;
    }
    
    float4 color;

    float NdotL = dot(normal, lightDir);

    if (gMaterial.lightingType == 2) { 
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        color.a = gMaterial.color.a * textureColor.a;
    } else if (gMaterial.lightingType == 1) { 
        float cos = saturate(NdotL);
        color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        color.a = gMaterial.color.a * textureColor.a;
    } else { 
        color = gMaterial.color * textureColor;
    }

    return color;
}