struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct Material {
    float4 color;
    int lightingType;
    float4x4 uvTransform;
};

cbuffer gMaterial : register(b0) {
    Material gMaterialData;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(PixelShaderInput input) : SV_TARGET {
    // UVトランスフォームを適用してテクスチャをサンプリング
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterialData.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // テクスチャの色 * マテリアルの色
    float4 outputColor = gMaterialData.color * textureColor;
    
    // アルファが極小なら描画を破棄（透明部分の処理・フェードアウト時の残像防止）
    if (outputColor.a <= 0.001f) {
        discard;
    }
    
    return outputColor;
}