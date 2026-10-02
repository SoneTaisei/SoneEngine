#pragma once
#include "BaseBlock.h"
#include <memory>

class GPUParticleSystem;

/// <summary>
/// DashRecovery - ダッシュ回復ブロック/アイテム
/// プレイヤーが接触するとダッシュ回数を即座に回復し、GPUパーティクルエフェクトを再生します。
/// </summary>
class DashRecovery : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    // 初期化処理（モデル・マテリアル・コライダー・GPUパーティクルのセットアップ）
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 毎フレームの更新処理（浮遊アニメーション・リスポーン管理・GPUパーティクル更新）
    void Update() override;

    // 描画処理（本体およびGPUパーティクル描画）
    void Draw() override;

    // 当たり判定の性質（デフォルトですり抜けアイテム、設定で足場化可能）
    bool IsSolid() const override { return isSolid_ && isActive_; }
    bool IsOneWay() const override { return false; }

    // プレイヤーが接触した瞬間の処理（ダッシュ回復・エフェクト発動）
    void OnCollision(Player2D* player) override;

    // プレイヤーがブロックの上に乗っている時の処理
    void OnPlayerStand() override;

    // プレイヤーが横や下から触れた時の処理
    void OnPlayerTouch() override;

    // JSONプロパティの読み込み（エディタのインスペクターからのパラメータ設定）
    void SetProperties(const nlohmann::json& properties) override;

    // ステージ再開時・リトライ時の状態リセット処理
    void Reset() override;

#ifdef USE_IMGUI
    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数
    void DrawImGui() override;
#endif

    // 回復・取得処理（DashRecaveryEffectの再生含む）
    void Collect(Player2D* player);

    // リスポーン（復活）処理
    void Respawn();

private:
    // 配置・外観パラメータ
    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Vector4 color_{ 0.35f, 0.85f, 1.0f, 1.0f }; // 鮮やかなシアンクリスタル色
    float inactiveAlpha_ = 0.25f;               // クールダウン中のゴースト透明度

    // アニメーション用
    float hoverTimer_ = 0.0f;
    float hoverSpeed_ = 3.0f;
    float hoverAmplitude_ = 0.12f;
    float rotationSpeed_ = 1.8f;
    float respawnAnimTimer_ = 0.0f;
    const float respawnAnimDuration_ = 0.35f;

    // ゲームプレイパラメータ
    bool isSolid_ = false;              // 足場として乗れるようにするか（falseですり抜けアイテム）
    bool isActive_ = true;              // 現在取得可能か
    float respawnTime_ = 2.5f;          // 取得後の復活時間（秒）
    float respawnTimer_ = 0.0f;         // クールダウンタイマー
    float hitstopDuration_ = 0.03f;     // 取得時のヒットストップ時間
    float cameraShakePower_ = 0.15f;    // 取得時の微小なカメラシェイク

    // エフェクト調整パラメータ
    bool useBurstTrigger_ = false;      // バースト強制全放出モード（OFF: エディタ通りの自然な発生）

    // GPUパーティクルエフェクト (DashRecaveryEffect)
    std::unique_ptr<GPUParticleSystem> gpuParticleSystem_;

    // グローバル連続取得防止ガード & デバッグ統計
    static float sGlobalCollectCooldown_;
    static int sTotalCollectCount_;
};
