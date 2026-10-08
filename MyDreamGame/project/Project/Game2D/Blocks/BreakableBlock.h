#pragma once
#include "BaseBlock.h"
#include <vector>
#include <memory>

/// <summary>
/// BreakableBlock - 通常時は普通のブロックとして機能し、
/// ダッシュで激突した時のみ破壊されるブロッククラス
/// </summary>
class BreakableBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    // 初期化処理（モデル・マテリアル・コライダー・破片演出のセットアップ）
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 毎フレームの更新処理（破片アニメーション・復活タイマーなど）
    void Update() override;

    // 描画処理（壊れていない時は本体、壊れている時は破片を描画）
    void Draw() override;

    // 当たり判定の性質（壊れていない時は通常ブロックと同じSolid）
    bool IsSolid() const override { return !isBroken_; }
    bool IsOneWay() const override { return false; }

    // プレイヤーが接触した瞬間の処理（ダッシュ中なら破壊）
    void OnCollision(Player2D* player) override;

    // プレイヤーがブロックの上に乗っている時の毎フレーム処理
    void OnPlayerStand() override;

    // プレイヤーが横や下から触れた時の処理
    void OnPlayerTouch() override;

    // ブロックの破壊処理
    void Break(Player2D* player = nullptr, bool triggerChain = true);

    // 単体ブロックの内部破壊処理
    void BreakInternal(Player2D* player, const Vector3& inheritedDashDir = { 1.0f, 0.0f, 0.0f });

    // ブロックの復活処理
    void Respawn();

    // JSONプロパティの読み込み（エディタのインスペクターからのパラメータ設定）
    void SetProperties(const nlohmann::json& properties) override;

    // ステージ再開時・リトライ時の状態リセット処理
    void Reset() override;

    // 状態取得
    bool IsBroken() const { return isBroken_; }
    bool IsBreakConnected() const { return breakConnected_; }
    void SetBreakConnected(bool enable) { breakConnected_ = enable; }

    // 接触面とダッシュ方向から破壊すべきかを判定するヘルパー関数
    bool ShouldBreakFromContact(Player2D* player) const;

#ifdef USE_IMGUI
    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数
    void DrawImGui() override;
#endif

private:
    struct Debris {
        std::unique_ptr<GameObject> gameObject;
        Vector3 initialScale{ 0.3f, 0.3f, 0.3f };
        Vector3 velocity{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationVelocity{ 0.0f, 0.0f, 0.0f };
        float timer = 0.0f;
        float lifetime = 0.6f;
        bool active = false;
    };

    bool isBroken_ = false;               // 破壊状態フラグ
    bool requireDirectionalDash_ = true;  // 接触面に応じた方向のダッシュのみで壊れるか
    bool breakConnected_ = true;          // 連結しているブロックをまとめて破壊するか
    float respawnTimer_ = 0.0f;           // 自動復活カウントタイマー
    float respawnTime_ = 0.0f;            // 自動復活までの秒数（0以下なら復活しない）
    
    // 演出パラメータ
    Vector4 blockColor_ = { 0.72f, 0.48f, 0.34f, 1.0f }; // ブロックの色（壊れやすい岩/レンガ調）
    float hitstopDuration_ = 0.06f;       // 破壊時のヒットストップ時間（秒）
    float cameraShakePower_ = 0.25f;      // 破壊時のカメラ揺れの強さ
    float cameraShakeDuration_ = 0.15f;   // 破壊時のカメラ揺れ時間（秒）

    // 初期配置情報
    Vector3 basePosition_{ 0.0f, 0.0f, 0.0f };
    Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };

    // 破片パーティクルリスト
    std::vector<Debris> debrisList_;
};
