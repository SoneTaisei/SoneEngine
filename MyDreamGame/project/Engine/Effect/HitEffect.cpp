#include "HitEffect.h"
#include <numbers>

void HitEffect::Initialize(ID3D12GraphicsCommandList* commandList, ParticleCommon* particleCommon, int srvIndex) {
    hitParticle_ = std::make_unique<ParticleManager>();
    hitParticle_->Initialize(commandList, particleCommon, 256, texturePath_, srvIndex, kBlendModeAdd);
    
    slashParticle_ = std::make_unique<ParticleManager>();
    slashParticle_->Initialize(commandList, particleCommon, 256, texturePath_, srvIndex + 1, kBlendModeAdd);
}

void HitEffect::EmitHit(const Vector3& position) {
    // 資料: Z回転を加えて星型にして、Emitterからの発生個数を8個にしてみる
    ParticleProperty minProp, maxProp;
    minProp.scale = { 0.05f, 1.0f, 1.0f };
    maxProp.scale = { 0.05f, 1.0f, 1.0f };
    minProp.rotate = { 0.0f, 0.0f, -std::numbers::pi_v<float> };
    maxProp.rotate = { 0.0f, 0.0f, std::numbers::pi_v<float> };
    minProp.velocity = { 0.0f, 0.0f, 0.0f };
    maxProp.velocity = { 0.0f, 0.0f, 0.0f };
    minProp.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    maxProp.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    minProp.lifeTime = 0.5f;
    maxProp.lifeTime = 1.0f;
    
    hitParticle_->EmitCustom(position, minProp, maxProp, 8);
}

void HitEffect::EmitSlash(const Vector3& position) {
    // 資料: 縦方向の大きさにランダムを入れて3個程度発生させると、剣撃で出るようなEffectになる
    ParticleProperty minProp, maxProp;
    minProp.scale = { 0.05f, 0.4f, 1.0f };
    maxProp.scale = { 0.05f, 1.5f, 1.0f };
    minProp.rotate = { 0.0f, 0.0f, -0.2f }; // 少し傾きにランダム性を持たせる
    maxProp.rotate = { 0.0f, 0.0f, 0.2f };
    minProp.velocity = { 0.0f, 0.0f, 0.0f };
    maxProp.velocity = { 0.0f, 0.0f, 0.0f };
    minProp.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    maxProp.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    minProp.lifeTime = 0.3f;
    maxProp.lifeTime = 0.6f;
    
    slashParticle_->EmitCustom(position, minProp, maxProp, 3);
}

void HitEffect::Update() {
    hitParticle_->Update();
    slashParticle_->Update();
}

void HitEffect::Draw(const Matrix4x4& viewProjection) {
    hitParticle_->Draw(viewProjection);
    slashParticle_->Draw(viewProjection);
}
