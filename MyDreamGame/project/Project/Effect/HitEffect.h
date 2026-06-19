#pragma once
#include "Effect/ParticleManager.h"

// ParticleManagerを継承し、独自の発生処理を持つHitEffectクラス
class HitEffect : public ParticleManager {
public:
    HitEffect() {
        name_ = "HitEffect"; // エディターで区別しやすいように名前を設定
    }
    ~HitEffect() override = default;

    // ヒットエフェクトの発生
    void EmitHit(const Vector3& position);

    // 剣撃エフェクトの発生
    void EmitSlash(const Vector3& position);

    // 毎フレームの更新処理
    void Update() override;

    // 描画処理（強制的に加算ブレンドにする）
    void Draw(const Matrix4x4& viewProjection);

    // ImGuiを描画する関数をオーバーライド
    void DrawImGui() override;

private:
    // エディターで調整するためのパラメータ群
    float scaleX_ = 0.075f;
    float scaleYMin_ = 0.6f;
    float scaleYMax_ = 2.25f;
    float lifeTime_ = 1.0f;
    int emitCountHit_ = 8;
    int emitCountSlash_ = 3;

    // 自動発生用タイマー
    float autoEmitTimer_ = 0.0f;

    // テスト発生用の座標
    Vector3 testEmitPosition_ = { 0.0f, 0.0f, 0.0f };
};
