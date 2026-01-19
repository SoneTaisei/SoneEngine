#define int32_t int
typedef float float32_t;
typedef float2 float32_t2;
typedef float3 float32_t3;
typedef float4 float32_t4;
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
    int enableBlinnPhong;
    float2 padding;
    float4x4 uvTransform;
    float32_t shininess;
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
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

