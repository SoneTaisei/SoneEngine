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
#include "CapePhysics.h"

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

struct SmokeParticle {
    Vector3 position;
    Vector3 velocity;
    Vector4 color;
    float startSize;
    float endSize;
    float timer;
    float duration;
    float rotation;
    float rotSpeed;
    bool active;
};

struct SoulParticle {
    Vector3 position;
    Vector3 velocity;
    Vector4 color;
    float startSize;
    float endSize;
    float timer;
    float duration;
    float rotation;
    float rotSpeed;
    float swayOffset;
    float swaySpeed;
    bool isAdditive = false; // true: 加算合成（光球・オーラ・スパーク）、false: 通常αブレンド（煙・霊気）
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
    void ReloadAnimations();
    void Update(const PlayerState& state, const PlayerParams& params, float deltaTime);
    void UpdateClearAnimation(const PlayerState& state, const PlayerParams& params, float clearTimer, float deltaTime);
    void Draw(const PlayerState& state, const PlayerParams& params);

    void SpawnJumpDust(const Vector3& basePos, float dirX);
    void SpawnRunDust(const Vector3& basePos, float dirX);
    void SpawnConfetti(const Vector3& pos);
    void SpawnDashRing(const Vector3& basePos, const Vector3& dashDir);
    void SpawnSmokeBomb(const Vector3& pos);
    void SpawnSoulSmoke(const Vector3& pos);
    void ClearEffects();

    PrimitiveObject* GetPrimitiveObject() { return primitiveObj_.get(); }
    Object3D* GetModelObject() { return modelObj_.get(); }
    AnimatorComponent* GetAnimator() { return animator_.get(); }
    CapePhysics& GetCapePhysics() { return capePhysics_; }

private:
    CapePhysics capePhysics_;
    std::unique_ptr<PrimitiveObject> primitiveObj_;
    std::unique_ptr<Object3D> modelObj_;
    std::unique_ptr<AnimatorComponent> animator_;
    Animation idleAnimation_;
    Animation walkAnimation_;
    Animation jumpAnimation_;
    Animation wallClimbAnimation_;
    Animation holdingWallAnimation_;
    Animation airDashAnimation_;
    Animation swingAnimation_;
    std::unique_ptr<PrimitiveObject> dashRingPrimitive_;
    std::unique_ptr<PrimitiveObject> dustPrimitive_;
    std::unique_ptr<PrimitiveObject> confettiPrimitive_;
    std::unique_ptr<PrimitiveObject> smokePrimitive_;
    std::unique_ptr<PrimitiveObject> soulPrimitive_;

    std::vector<DustParticle> dustParticles_;
    std::vector<ConfettiParticle> confettiParticles_;
    std::vector<DashRingParticle> dashRingParticles_;
    std::vector<SmokeParticle> smokeParticles_;
    std::vector<SoulParticle> soulParticles_;

    enum class PlayerAnimType {
        None,
        Idle,
        Walk,
        Jump,
        WallClimb,
        HoldingWall,
        AirDash,
        Swing,
        Hold
    };

    PlayerAnimType currentAnimType_ = PlayerAnimType::None;

    float visualTime_ = 0.0f;
    float climbBlendFactor_ = 0.0f;
    float wallClimbAnimTime_ = 0.0f;
    float holdingWallAnimTime_ = 0.0f;
    float airDashAnimTime_ = 0.0f;
    float swingAnimTime_ = 0.0f;

public:
    // しがみつき時の腕の調整用パラメータ（親空間での回転：X=ピッチ, Y=ヨー, Z=ロール、ラジアン単位）
    float debugLArmRot_[3] = { -0.200f, -3.140f, 0.262f }; 
    float debugRArmRot_[3] = { -0.200f, 3.140f, -0.262f };
    float debugLForeArmRot_[3] = { -0.334f, 0.0f, 0.0f };
    float debugRForeArmRot_[3] = { -0.334f, 0.0f, 0.0f };
    float debugHipsRot_[3] = { 0.0f, 0.0f, 0.0f };

#ifdef USE_IMGUI
    void DisplayImGui();
#endif

private:
    float EaseInElastic(float t) const;
};
