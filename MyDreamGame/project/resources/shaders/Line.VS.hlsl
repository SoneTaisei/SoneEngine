// Vertex shader for debug line rendering
struct VSInput {
    float4 position : POSITION;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

cbuffer ViewProjectionBuffer : register(b0) {
    float4x4 viewProjection;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(input.position, viewProjection);
    output.color = input.color;
    return output;
}
