#include "RingEffect.h"
#include "Core/TimeManager.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Graphics/TextureManager.h"

RingEffect::RingEffect() {}

void RingEffect::Initialize(ID3D12Device* device, uint32_t textureHandle) {
    // ■ RingEffectのルートオブジェクト作成
    ringEffectRoot_ = std::make_unique<PrimitiveObject>();
    ringEffectRoot_->Initialize(device, nullptr); // 描画しない
    ringEffectRoot_->SetName("RingEffect");
    ringEffectRoot_->SetTranslation({0.0f, 0.0f, 0.0f});

    // Ring 1
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device, PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(textureHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 0.0f, 0.0f}); // 少し傾ける
        ring->SetIsBillboard(false);          // 立体感を出すためにビルボードOFF
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 1");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }

    // Ring 2
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device, PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(textureHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 0.785f, 0.0f}); // 45度
        ring->SetIsBillboard(false);
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 2");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }

    // Ring 3
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device, PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1,1,1,1}, {1,1,1,1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(textureHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 1.57f, 0.0f});  // 90度
        ring->SetIsBillboard(false);
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 3");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }

    // Ring 4
    {
        auto ring = std::make_unique<PrimitiveObject>();
        ring->Initialize(device, PrimitiveManager::GetInstance()->GetRing(0.5f, 1.0f, 64, 0.0f, 2.0f * 3.14159f, {1, 1, 1, 1}, {1, 1, 1, 1}, false));
        ring->GetMaterial().enableEnvironmentMap = 0;
        ring->GetMaterial().lightingType = 0;
        ring->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(textureHandle));
        ring->SetTranslation({0.0f, 0.0f, 0.0f});
        ring->SetRotation({0.4f, 2.355f, 0.0f}); // 135度
        ring->SetIsBillboard(false);
        ring->SetIsDoubleSided(true);
        ring->SetBlendMode(BlendMode::kBlendModeAdd);
        ring->SetName("Ring 4");
        ring->SetParent(ringEffectRoot_.get());
        primitiveParticles_.push_back(std::move(ring));
    }
}

void RingEffect::Update(float deltaTime) {
    // リングのアニメーション更新 (1秒周期でパッと出てゆっくり消える)
    ringEffectTimer_ += deltaTime;
    if (ringEffectTimer_ > kRingEffectDuration) {
        ringEffectTimer_ = 0.0f; // ループ
    }

    // 1.0 -> 0.0 へフェードアウト
    float alpha = 1.0f - (ringEffectTimer_ / kRingEffectDuration);

    // リング全体の更新
    if (ringEffectRoot_) {
        ringEffectRoot_->Update();
    }

    // リングのみフェードアニメーションと更新を行う
    for (auto& ring : primitiveParticles_) {
        ring->GetMaterial().color.w = alpha; // 透明度を適用
        ring->Update();
    }
}

void RingEffect::Draw(ID3D12GraphicsCommandList* commandList) {
    for (auto& ring : primitiveParticles_) {
        ring->Draw(commandList);
    }
}

std::vector<PrimitiveObject*> RingEffect::GetParticles() {
    std::vector<PrimitiveObject*> result;
    for (auto& p : primitiveParticles_) {
        result.push_back(p.get());
    }
    return result;
}
