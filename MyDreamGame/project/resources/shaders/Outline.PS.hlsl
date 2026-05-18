// Outline.PS.hlsl
#include "Object3d.hlsli"

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

// Output solid black color for the outline
float4 main(VertexShaderOutput input) : SV_TARGET {
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
