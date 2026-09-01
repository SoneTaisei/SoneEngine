struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float3 localPosition : POSITION1;
    float4 color : COLOR0;
};

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
};

struct Material {
    float4 color;
    int lightingType;
    int enableBlinnPhong;
    int enableEnvironmentMap;
    float alphaReference;
    float4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    float dissolveThreshold;
    float enableBoxMapping;
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
    int enableFlatShading;
    float3 padding;
};

struct PointLight {
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay; 
};

static const uint kMaxSpotLights = 8;

struct SpotLight {
    float4 color; 
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    int enable;
};

struct SpotLightGroup {
    SpotLight spotLights[kMaxSpotLights];
    int spotLightCount;
    float ambientIntensity;
    float2 padding;
};

struct ViewProjection {
    float4x4 viewProjectionMatrix;
    float3 cameraPosition;
    float padding;
};

struct Camera {
    float3 worldPosition;
    float padding; // 16バイトアラインメントに合わせる
};