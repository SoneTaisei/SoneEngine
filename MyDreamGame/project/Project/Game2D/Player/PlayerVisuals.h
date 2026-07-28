#include <d3d12.h>
#pragma once
#include <memory>
#include <vector>
#include "GameObject/PrimitiveObject.h"
#include "PlayerConfig.h"
#include "PlayerState.h"
#include "Core/Utility/Structs.h"

#include "GameObject/Object3D.h"
#include "Component/AnimatorComponent.h"

struct DustParticle {
    Vector3 position;
    Vector3 velocity;
    float timer;
    float duration;
    float startSize;
    bool active;
};

struct ConfettiParticle {
    Vector3 position;
    Vector3 velocity;
    Vector4 color;
    Vector3 rotation;
    Vector3 rotationSpeed;
    float timer;
    float duration;
    float size;
    bool active;
};

struct DashRingParticle {
    Vector3 position;
    Vector3 rotation;
    float timer;
    float duration;
    float startSize;
    float endSize;
    bool active;
};

class PlayerVisuals {
public:
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, Primitive* ringPrimitive, uint32_t texHandle, Model* playerModel);
    void Update(const PlayerState& state, const PlayerParams& params, float deltaTime);
    void Draw(const PlayerState& state, const PlayerParams& params);

    void SpawnJumpDust(const Vector3& basePos, float dirX);
    void SpawnRunDust(const Vector3& basePos, float dirX);
    void SpawnConfetti(const Vector3& pos);
    void SpawnDashRing(const Vector3& basePos, const Vector3& dashDir);

    PrimitiveObject* GetPrimitiveObject() { return primitiveObj_.get(); }
    Object3D* GetModelObject() { return modelObj_.get(); }

private:
    std::unique_ptr<PrimitiveObject> primitiveObj_;
    std::unique_ptr<Object3D> modelObj_;
    std::unique_ptr<AnimatorComponent> animator_;
    Animation idleAnimation_;
    Animation walkAnimation_;
    std::unique_ptr<PrimitiveObject> dashRingPrimitive_;
    std::unique_ptr<PrimitiveObject> dustPrimitive_;
    std::unique_ptr<PrimitiveObject> confettiPrimitive_;

    std::vector<DustParticle> dustParticles_;
    std::vector<ConfettiParticle> confettiParticles_;
    std::vector<DashRingParticle> dashRingParticles_;

    float visualTime_ = 0.0f;

    float EaseInElastic(float t) const;
};
