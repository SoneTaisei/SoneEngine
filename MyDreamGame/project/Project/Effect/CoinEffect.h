#pragma once
#include "GameObject/PrimitiveObject.h"
#include <memory>
#include <vector>

class CoinEffect {
public:
    CoinEffect() = default;
    ~CoinEffect() = default;

    void Initialize(ID3D12Device* device);
    void Update(float deltaTime);
    void Draw();

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
