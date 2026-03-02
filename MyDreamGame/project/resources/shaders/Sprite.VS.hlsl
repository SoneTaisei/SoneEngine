struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

cbuffer gTransformationMatrix : register(b1) {
    float4x4 WVP;
    float4x4 World;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    // スプライトなのでライト計算不要。WVP行列のみ適用
    output.position = mul(input.position, WVP);
    output.texcoord = input.texcoord;
    return output;
}