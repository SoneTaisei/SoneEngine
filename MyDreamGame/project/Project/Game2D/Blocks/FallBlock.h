#pragma once
#include "BaseBlock.h"

/// <summary>
/// FallBlock - プレイヤーが上から乗った時、または側面につかまった時に物理的に落下するブロック
/// </summary>
class FallBlock : public BaseBlock {
public:
    enum class State {
        Idle,           // 待機状態
        Shaking,        // 落下前の予兆（揺れ・沈み込み）
        Falling,        // 物理落下中（重力加速度）
        Landed,         // 着地停止
        Respawning      // リスポーン待機
    };

    using BaseBlock::BaseBlock;

    // 初期化処理（モデル・マテリアル・コライダーのセットアップ）
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 毎フレームの更新処理（状態遷移・物理計算・アニメーション）
    void Update() override;

    // 当たり判定の性質
    bool IsSolid() const override { return isSolid_ && (state_ != State::Respawning); }
    bool IsOneWay() const override { return false; }

    // 動く足場としてプレイヤーの追従・物理干渉を有効化
    bool IsMoving() const override { return true; }
    Vector3 GetVelocity() const override { return velocity_; }

    // プレイヤーが接触した瞬間の処理
    void OnCollision(Player2D* player) override;

    // プレイヤーがブロックの上に乗っている時の毎フレーム処理
    void OnPlayerStand() override;

    // プレイヤーが横や下から触れた時の処理
    void OnPlayerTouch() override;

    // JSONプロパティの読み込み（エディタのインスペクターからのパラメータ設定）
    void SetProperties(const nlohmann::json& properties) override;

    // ステージ再開時・リトライ時の状態リセット処理
    void Reset() override;

    // 落下をトリガーする外部・内部関数
    void TriggerFall();

    // デフォルトカラーの設定
    void SetDefaultColor(const Vector4& color);

#ifdef USE_IMGUI
    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数
    void DrawImGui() override;
#endif

private:
    // 地面（Solidブロック）との衝突判定と接地処理
    bool CheckGroundCollision(float nextY, float& outGroundY);

    // デフォルト色から予兆色（警告色）を自動生成
    void UpdateShakeColor();

private:
    State state_ = State::Idle;

    // 位置・速度・サイズ
    Vector3 initialPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    float width_ = 1.0f;
    float height_ = 1.0f;

    // 接触検知用フラグ
    bool isPlayerStanding_ = false;
    bool isPlayerTouchingSide_ = false;
    bool isSolid_ = true;

    // --- 物理・演出パラメータ ---
    float shakeDuration_ = 0.28f;       // 落下前の揺れ・予兆時間（秒）
    float shakeIntensity_ = 0.08f;      // 左右の揺れの強さ
    float sinkAmount_ = 0.12f;          // プレイヤーが乗ったときの沈み込み量
    float gravity_ = -32.0f;            // 重力加速度 (m/s^2)
    float terminalVelocity_ = -28.0f;   // 終端速度（最大落下速度）
    bool stopOnGround_ = true;          // 地面（Solidブロック）に着地したら停止するか
    float respawnTime_ = 0.0f;          // リスポーンまでの時間（0以下の場合はリスポーンせず）
    float fallDelayTimer_ = 0.0f;       // 予兆タイマー
    float respawnTimer_ = 0.0f;         // リスポーンタイマー
    float stateTimer_ = 0.0f;           // 各状態での経過時間

    // 外見演出用
    Vector4 defaultColor_{ 0.189f, 0.448f, 0.168f, 1.0f }; // 通常カラー（JSON/エディタ保存色と同期）
    Vector4 shakeColor_{ 0.85f, 0.45f, 0.20f, 1.0f };     // 予兆時カラー
    bool colorInitialized_ = false;                       // エディタ色キャプチャフラグ
};
