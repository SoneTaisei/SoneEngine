#define int32_t int
struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct Material {
    float4 color;
    int32_t lightingType;
    float4x4 uvTransform;
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};

struct DirectionalLight {
    float4 color;
    float3 direction; 
    float intensity; 
};

struct ViewProjection {
    matrix viewProjectionMatrix;
    float3 cameraPosition; 
    float padding; 
};

struct Camera {
    float3 worldPosition;
};

