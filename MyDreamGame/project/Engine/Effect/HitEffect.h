#pragma once
#include "Effect/ParticleManager.h"
#include <string>

class HitEffect {
public:
    static HitEffect* GetInstance() {
        static HitEffect instance;
        return &instance;
    }

    void Initialize(ID3D12GraphicsCommandList* commandList, ParticleCommon* particleCommon, int srvIndex);
    
    // 資料通りの「ヒットエフェクトっぽいもの」を発生させる
    void EmitHit(const Vector3& position);

    // 資料通りの「剣撃エフェクト」を発生させる
    void EmitSlash(const Vector3& position);

    void Update();
    void Draw(const Matrix4x4& viewProjection);

private:
    HitEffect() = default;
    ~HitEffect() = default;

    std::unique_ptr<ParticleManager> hitParticle_;
    std::unique_ptr<ParticleManager> slashParticle_;
    
    std::string texturePath_ = "resources/circle2.png";
};
