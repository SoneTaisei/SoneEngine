#pragma once
#include "BaseBlock.h"
#include <string>
#include <unordered_map>

/// <summary>
/// SavePoint - 中間ポイントブロック
/// プレイヤーが接触するとセーブされ、ミス時にこの位置から再開される
/// </summary>
class SavePoint : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    struct CheckpointData {
        bool isValid = false;
        Vector3 spawnPosition = { 0.0f, 0.0f, 0.0f };
        int chipX = -1;
        int chipY = -1;
        int chainLength = -1; // 通った時に持っていた鎖の本数（-1 = 記録なし）
    };

    // --- 静的セーブデータ管理 ---
    static void SetActiveSavePoint(const std::string& stagePath, const Vector3& pos, int chipX, int chipY, int chainLength = -1);
    static bool HasActiveSavePoint(const std::string& stagePath);
    static Vector3 GetActiveSavePoint(const std::string& stagePath);
    static int GetActiveChipX(const std::string& stagePath);
    static int GetActiveChipY(const std::string& stagePath);
    /// <summary>そのセーブポイントを通った時の鎖の本数。記録が無ければ -1</summary>
    static int GetActiveChainLength(const std::string& stagePath);
    static void Clear(const std::string& stagePath = "");
    static void ClearAll();
    static std::string NormalizeStageKey(const std::string& stagePath);

    // 初期化処理（モデル・マテリアル・コライダーのセットアップ）
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 毎フレームの更新処理（浮遊・自転・発光パルス演出）
    void Update() override;

    // 当たり判定の性質（通り抜け可能）
    bool IsSolid() const override { return false; }
    bool IsOneWay() const override { return false; }

    // プレイヤーが接触した瞬間の処理
    void OnCollision(Player2D* player) override;

    // プレイヤーがブロックの上に乗っている時の毎フレーム処理
    void OnPlayerStand() override;
    void OnPlayerStand(Player2D* player) override;

    // プレイヤーが横や下から触れた時の処理
    void OnPlayerTouch() override;
    void OnPlayerTouch(Player2D* player) override;

    // JSONプロパティの読み込み（エディタのインスペクターからのパラメータ設定）
    void SetProperties(const nlohmann::json& properties) override;

    // ステージ再開時・リトライ時の状態リセット処理
    void Reset() override;

    bool IsActive() const;

#ifdef USE_IMGUI
    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数
    void DrawImGui() override;
#endif

private:
    void Activate(Player2D* player);
    std::string GetCurrentStageKey() const;
    bool CheckIfCurrentActive() const;

    static std::unordered_map<std::string, CheckpointData> s_ActiveCheckpoints;

    float centerX_ = 0.0f;
    float centerY_ = 0.0f;
    float baseWidth_ = 1.0f;
    float baseHeight_ = 1.0f;

    bool isActivated_ = false;        // このセーブポイントが触れられたことがあるか
    float pulseTimer_ = 0.0f;         // 触れた瞬間のパルス拡大演出用タイマー
    float animTime_ = 0.0f;           // 浮遊・アニメーション用タイマー
    float customSpawnOffsetY_ = 0.0f; // リスポーン位置の微調整オフセット
};
