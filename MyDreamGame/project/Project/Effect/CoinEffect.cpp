#include "CoinEffect.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include <random>

void CoinEffect::Initialize(ID3D12Device* device) {
    device_ = device;
}

void CoinEffect::Update(float deltaTime) {
    for (auto it = particles_.begin(); it != particles_.end(); ) {
        it->lifeTime -= deltaTime;
        if (it->lifeTime <= 0.0f) {
            it = particles_.erase(it);
            continue;
        }

        it->position += it->velocity * deltaTime;
        it->rotationZ += it->rotSpeed * deltaTime;
        
        it->obj->SetTranslation(it->position);
        it->obj->SetRotation({0.0f, 0.0f, it->rotationZ});

        // 縮小させながら消える
        float alpha = it->lifeTime / it->maxLifeTime;
        it->obj->SetScale({it->scale.x * alpha, it->scale.y * alpha, it->scale.z * alpha});
        
        it->obj->GetMaterial().color.w = alpha;

        it->obj->Update();

        ++it;
    }
}

void CoinEffect::Draw(ID3D12GraphicsCommandList* commandList) {
    for (auto& p : particles_) {
        p.obj->Draw(commandList);
    }
}

void CoinEffect::Emit(const Vector3& position) {
    std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> distVelocityX(-2.0f, 2.0f);
    std::uniform_real_distribution<float> distVelocityY(1.0f, 4.0f);
    std::uniform_real_distribution<float> distRot(-3.0f, 3.0f);
    std::uniform_real_distribution<float> distBaseScale(0.3f, 0.7f);
    std::uniform_real_distribution<float> distScaleMult(0.7f, 1.3f);
    std::uniform_real_distribution<float> distLife(0.4f, 0.8f);

    for (int i = 0; i < 6; ++i) { // 6個の星を飛ばす
        Particle p;
        p.obj = std::make_unique<PrimitiveObject>();
        p.obj->Initialize(device_, PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Star, 1.0f));
        
        p.obj->GetMaterial().lightingType = 0; // Unlit
        p.obj->GetMaterial().enableEnvironmentMap = 0;
        p.obj->SetBlendMode(BlendMode::kBlendModeAdd);
        p.obj->SetIsBillboard(true); // カメラの方向を向かせる
        p.obj->SetIsDoubleSided(true);

        float baseScale = distBaseScale(randomEngine);
        float mult = distScaleMult(randomEngine);
        float s = baseScale * mult;
        p.scale = {s, s, s};
        p.obj->SetScale(p.scale);
        
        p.position = position;
        p.velocity = {distVelocityX(randomEngine), distVelocityY(randomEngine), 0.0f};
        p.rotationZ = 0.0f;
        p.rotSpeed = distRot(randomEngine);
        p.maxLifeTime = distLife(randomEngine);
        p.lifeTime = p.maxLifeTime;

        p.obj->SetTranslation(p.position);
        p.obj->Update();

        particles_.push_back(std::move(p));
    }
}

std::vector<PrimitiveObject*> CoinEffect::GetParticles() {
    std::vector<PrimitiveObject*> result;
    for (auto& p : particles_) {
        result.push_back(p.obj.get());
    }
    return result;
}
