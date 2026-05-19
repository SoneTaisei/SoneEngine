// Outline.VS.hlsl
#include "Object3d.hlsli"

// Constant buffer for transformation matrix
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// Constant buffer for outline parameters (dynamic thickness)
struct OutlineParams {
    float thickness;
    float3 padding;
};
ConstantBuffer<OutlineParams> gOutlineParams : register(b1);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    
    // Extrude the vertex position along the normal using the thickness parameter
    float4 localPos = input.position;
    localPos.xyz += input.normal * gOutlineParams.thickness;
    
    // Transform position to clip space
    output.position = mul(localPos, gTransformationMatrix.WVP);
    output.worldPosition = mul(localPos, gTransformationMatrix.World).xyz;
    output.texcoord = input.texcoord;
    
    // Transform normal to world space
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.color = input.color;
    
    return output;
}
