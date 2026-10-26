#pragma once
#include "Core/Utility/Matrix4x4.h"
#include "Core/Utility/Vector3.h"
#include <memory>
#include <vector>

class BaseEffect;
class CoinEffect;
class GPUParticleSystem;
class GameCamera;
class ParticleCommon;
class PrimitiveObject;

class GameEffectManager {
public:
    GameEffectManager();
    ~GameEffectManager();

    // 初期化（CoinEffect, RingEffect, CylinderEffect, GPUParticleSystem）
    void Initialize();

    // 更新（isGameActive時はBaseEffectも更新、降雪パーティクルは常にカメラ追従更新）
    void Update(float deltaTime, bool isGameActive, GameCamera* camera);

    // 描画（3Dエフェクトおよび降雪パーティクル）
    void Draw(ParticleCommon* particleCommon, const Matrix4x4& viewProjectionMatrix);

    // コイン獲得時のエフェクト発生
    void EmitCoin(const Vector3& playerPosition);

    // 全エフェクトのリセット（プレイヤーリスポーン時など）
    void ResetAllEffects();

    // エフェクトリストの取得（リプレイ更新用）
    const std::vector<std::unique_ptr<BaseEffect>>& GetEffects() const { return effects_; }

    // プリミティブオブジェクト一覧の取得（ヒエラルキー・デバッグ描画用）
    std::vector<PrimitiveObject*> GetPrimitives() const;

private:
    std::vector<std::unique_ptr<BaseEffect>> effects_;
    CoinEffect* coinEffect_ = nullptr;
    std::unique_ptr<GPUParticleSystem> snowParticle_;
};
