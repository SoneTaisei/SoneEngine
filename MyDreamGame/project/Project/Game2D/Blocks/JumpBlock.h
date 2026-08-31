#pragma once
#include "BaseBlock.h"

/// <summary>
/// JumpBlock - 自作ブロッククラス
/// </summary>
class JumpBlock : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    // 初期化処理（モデル・マテリアル・コライダーのセットアップ）
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 毎フレームの更新処理（タイマー・移動・アニメーションなど）
    void Update() override;

    // 当たり判定の性質
    bool IsSolid() const override { return true; }
    bool IsOneWay() const override { return false; }

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

#ifdef USE_IMGUI
    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数
    void DrawImGui() override;
#endif

private:
    // --- カスタムパラメータ例 ---
    float customPower_ = 10.0f;     // パワー・強さ
    float speed_ = 2.0f;           // 移動・アニメ速度
    float timer_ = 0.0f;           // 内部タイマー
    bool isActive_ = true;         // 有効/無効フラグ
};
