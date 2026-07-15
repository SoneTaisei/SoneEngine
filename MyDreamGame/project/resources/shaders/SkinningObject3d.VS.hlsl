#include "Object3d.hlsli"

// Transformation matrix for the object
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// Directional light (not used directly in this simple VS, but kept for consistency)
cbuffer gDirectionalLight : register(b4) {
    DirectionalLight gDirectionalLight;
}

// WellMatrix for skinning
struct Well {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

// SkinCluster data (Palette)
StructuredBuffer<Well> gPalette : register(t1);

// Vertex Shader Input (now including Weight and Indices)
struct SkinningVertexShaderInput {
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float4 weight : WEIGHT;
    int4 jointIndices : BONEINDICES;
};

VertexShaderOutput main(SkinningVertexShaderInput input) {
    VertexShaderOutput output;
    
    matrix skinnedMatrix = (matrix)0;
    
    // Calculate skinned matrix
    for (int i = 0; i < 4; ++i) {
        if (input.weight[i] > 0.0f) {
            skinnedMatrix += gPalette[input.jointIndices[i]].skeletonSpaceMatrix * input.weight[i];
        }
    }
    
    // If no weights are assigned (fallback), use identity matrix
    if (input.weight[0] == 0.0f && input.weight[1] == 0.0f && 
        input.weight[2] == 0.0f && input.weight[3] == 0.0f) {
        skinnedMatrix = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    }
    
    // Transform position by skinning matrix
    float4 skinnedPosition = mul(input.position, skinnedMatrix);
    
    // Final position using WVP matrix
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.localPosition = input.position.xyz;
    output.texcoord = input.texcoord;
    
    // Transform normal by skinning matrix
    float3 skinnedNormal = mul(input.normal, (float3x3)skinnedMatrix);
    output.normal = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    
    output.color = input.color;
    
    return output;
}
