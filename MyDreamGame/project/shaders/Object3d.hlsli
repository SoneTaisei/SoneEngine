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
    int lightingType;
    int enableBlinnPhong;
    float2 padding;
    float4x4 uvTransform;
    float shininess;
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

struct PointLight {
    float4 color;
    float3 position;
    float intensity;
    float radius; // 追加（シェーダ側で距離減衰に使用）
    float decay; // 追加（減衰のべき乗）
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