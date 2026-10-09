#pragma once
#include "Effect/BaseEffect.h"
#include "GameObject/PrimitiveObject.h"
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <vector>

class CylinderEffect : public BaseEffect {
public:
    CylinderEffect();
    ~CylinderEffect() override = default;

    void Initialize(ID3D12Device* device, uint32_t textureHandle);
    void Update(float deltaTime) override;
    void Draw() override;

    std::vector<PrimitiveObject*> GetPrimitives() override {
        if (cylinderEffectRoot_) {
            return { cylinderEffectRoot_.get() };
        }
        return {};
    }

    PrimitiveObject* GetRoot() const { return cylinderEffectRoot_.get(); }
    PrimitiveObject* GetParticle() const { return cylinderObject_.get(); }

private:
    std::unique_ptr<PrimitiveObject> cylinderEffectRoot_;
    std::unique_ptr<PrimitiveObject> cylinderObject_;
};
