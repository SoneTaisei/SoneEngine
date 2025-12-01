#include "Particle.hlsli"

struct ParticleForGPU {
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    
    output.position = mul(input.position, gParticle[instanceId].WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gParticle[instanceId].World));
    
    output.color = gParticle[instanceId].color;
    
    return output;
}