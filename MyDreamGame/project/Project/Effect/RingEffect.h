#pragma once
#include "GameObject/PrimitiveObject.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <vector>

class RingEffect {
public:
    RingEffect();
    ~RingEffect() = default;

    void Initialize(ID3D12Device* device, uint32_t textureHandle);
    void Update(float deltaTime);
    void Draw();

    PrimitiveObject* GetRoot() const { return ringEffectRoot_.get(); }
    std::vector<PrimitiveObject*> GetParticles();

private:
    std::unique_ptr<PrimitiveObject> ringEffectRoot_;
    std::vector<std::unique_ptr<PrimitiveObject>> primitiveParticles_;

    float ringEffectTimer_ = 0.0f;
    const float kRingEffectDuration = 2.0f;
};
