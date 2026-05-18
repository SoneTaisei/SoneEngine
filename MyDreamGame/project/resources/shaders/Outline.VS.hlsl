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
    
    // Expand the vertex position in local space
    float4 localPos = input.position;
    
    // Check if the shape is flat (like Ring, Circle or Plane) to expand it radially
    if (abs(input.normal.z) > 0.99f && abs(input.normal.x) < 0.01f && abs(input.normal.y) < 0.01f) {
        // Flat shape in XY plane: expand radially from local origin
        float len = length(input.position.xy);
        if (len > 0.0001f) {
            localPos.xy += (input.position.xy / len) * gOutlineParams.thickness;
        } else {
            localPos.xyz += input.normal * gOutlineParams.thickness;
        }
    } else if (abs(input.normal.y) > 0.99f && abs(input.normal.x) < 0.01f && abs(input.normal.z) < 0.01f) {
        // Flat shape in XZ plane: expand radially from local origin
        float len = length(input.position.xz);
        if (len > 0.0001f) {
            localPos.xz += (input.position.xz / len) * gOutlineParams.thickness;
        } else {
            localPos.xyz += input.normal * gOutlineParams.thickness;
        }
    } else if (abs(input.normal.x) > 0.99f && abs(input.normal.y) < 0.01f && abs(input.normal.z) < 0.01f) {
        // Flat shape in YZ plane: expand radially from local origin
        float len = length(input.position.yz);
        if (len > 0.0001f) {
            localPos.yz += (input.position.yz / len) * gOutlineParams.thickness;
        } else {
            localPos.xyz += input.normal * gOutlineParams.thickness;
        }
    } else {
        // Standard 3D volumetric shape: expand along the vertex normal
        localPos.xyz += input.normal * gOutlineParams.thickness;
    }
    
    // Transform expanded position to clip space
    output.position = mul(localPos, gTransformationMatrix.WVP);
    output.worldPosition = mul(localPos, gTransformationMatrix.World).xyz;
    output.texcoord = input.texcoord;
    
    // Transform normal to world space
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.color = input.color;
    
    return output;
}
