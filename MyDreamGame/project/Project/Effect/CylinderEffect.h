#pragma once
#include "GameObject/PrimitiveObject.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>

class CylinderEffect {
public:
    CylinderEffect();
    ~CylinderEffect() = default;

    void Initialize(ID3D12Device* device, uint32_t textureHandle);
    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* commandList);

    PrimitiveObject* GetRoot() const { return cylinderEffectRoot_.get(); }
    PrimitiveObject* GetParticle() const { return cylinderObject_.get(); }

private:
    std::unique_ptr<PrimitiveObject> cylinderEffectRoot_;
    std::unique_ptr<PrimitiveObject> cylinderObject_;
};
