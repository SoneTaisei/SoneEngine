#pragma once
#include "BaseEffect.h"
#include "GameObject/PrimitiveObject.h"
#include <memory>
#include <vector>

class CoinEffect : public BaseEffect {
public:
    CoinEffect() = default;
    ~CoinEffect() override = default;

    void Initialize(ID3D12Device* device);
    void Update(float deltaTime) override;
    void Draw() override;
    void Reset() override { Clear(); }
    std::vector<PrimitiveObject*> GetPrimitives() override { return GetParticles(); }

    void Emit(const Vector3& position);
    void Clear();

    std::vector<PrimitiveObject*> GetParticles();

private:
    struct Particle {
        std::unique_ptr<PrimitiveObject> obj;
        float lifeTime;
        float maxLifeTime;
        Vector3 velocity;
        Vector3 position;
        float rotationZ;
        float rotSpeed;
        Vector3 scale;
    };

    ID3D12Device* device_ = nullptr;
    std::vector<Particle> particles_;
};
