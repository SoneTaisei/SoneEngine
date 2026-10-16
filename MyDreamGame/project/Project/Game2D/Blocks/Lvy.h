#pragma once
#include "BaseBlock.h"
#include <vector>

/// <summary>
/// Lvy (ツタ) - マリオ風の「ぐにゃっとしなる」物理スイング＆ジャンプギミック
/// 1ブロック分の長さを基本とし、下につなげるたびに自動で連結されて長くなる。
/// 上にブロック（天井）がある想定で、最上部の上端が固定ピボット軸となる。
/// 曲げ剛性スプリング物理により、プレイヤーがつかまるとリアルにしなって美しい弧を描く。
/// つかまった高さ（座標）でそのまま保持され、上下入力でツタを登り降りできる。
/// </summary>
class Lvy : public BaseBlock {
public:
    using BaseBlock::BaseBlock;

    struct SegmentPhysics {
        float angle = 0.0f;           // 各セグメントの傾き角度 (ラジアン)
        float angularVelocity = 0.0f;  // 各セグメントの角速度 (rad/s)
    };

    struct ChainInfo {
        Lvy* rootBlock = nullptr;         // チェーン最上部のブロック（親）
        Lvy* tailBlock = nullptr;         // チェーン最下部のブロック（末端）
        float totalLength = 1.0f;         // チェーン全体の長さ
        int totalSegments = 1;            // 連結されているブロック総数
        Vector3 pivotPos = { 0.0f, 0.0f, 0.0f }; // 最上部の固定回転軸
        std::vector<Lvy*> segments;       // 上から順に全セグメント
    };

    struct VinePoint {
        Vector3 position = { 0.0f, 0.0f, 0.0f }; // ワールド座標
        Vector3 velocity = { 0.0f, 0.0f, 0.0f }; // 接線速度
        size_t segmentIndex = 0;                 // 属しているセグメント番号
    };

    // 初期化処理（モデル・マテリアル・コライダーのセットアップ）
    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;

    // 毎フレームの更新処理（チェーン一括更新により親・子が完全同時同期）
    void Update() override;

    // 当たり判定の性質（つかまりギミックのためSolidではなくトリガー通過可能にする）
    bool IsSolid() const override { return false; }
    bool IsOneWay() const override { return false; }

    // プレイヤーが接触した瞬間の処理（ツタを掴む）
    void OnCollision(Player2D* player) override;

    // プレイヤーの掴まり処理（どのセグメントに触れてもRootに委譲）
    void HandlePlayerGrab(Player2D* player, const ChainInfo& chain);

    // しなり物理シミュレーション（Rootが一括計算）
    void SimulatePhysics(const ChainInfo& chain, float deltaTime, bool isPhysicsActive);

    // 距離 distance におけるツタのワールド座標と速度を計算
    VinePoint GetPointAtDistance(const ChainInfo& chain, float distance) const;

    // JSONプロパティの読み込み（エディタのインスペクターからのパラメータ設定）
    void SetProperties(const nlohmann::json& properties) override;

    // ステージ再開時・リトライ時の状態リセット処理
    void Reset() override;

    // AABBの取得（スイング変形・領域に応じたバウンディングボックス）
    AABB2D GetAABB() const override;

    // 各セグメントの角度・角速度の取得
    float GetSegmentAngle(size_t index) const;
    float GetSegmentAngularVelocity(size_t index) const;

    // チェーン構造の探索
    ChainInfo GetChainInfo() const;

    float GetBaseWorldX() const { return baseWorldX_; }
    float GetBaseWorldY() const { return baseWorldY_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }

#ifdef USE_IMGUI
    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数
    void DrawImGui() override;
#endif

private:
    // 初期配置情報
    float baseWorldX_ = 0.0f;
    float baseWorldY_ = 0.0f;
    float width_ = 1.0f;
    float height_ = 1.0f;
    float thickness_ = 0.24f;                  // ツタの太さ

    // 各セグメントの物理状態（Rootのみが保持・計算）
    std::vector<SegmentPhysics> segmentPhysics_;

    // しなり物理パラメータ
    float gravity_ = 26.0f;                    // 重力復元トルク
    float bendStiffness_ = 48.0f;              // 曲げ剛性（コシの強さ・小さいほどぐにゃっと曲がる）
    float bendDamping_ = 3.5f;                 // 曲げ内部減衰（しなりの振動を滑らかにする）
    float dampingEmpty_ = 0.7f;                // 未掴まり時の空気減衰
    float dampingHolding_ = 0.14f;             // 掴まり中の空気減衰（スイング持続）
    float swingPower_ = 15.0f;                 // 左右入力による漕ぎ力
    float jumpReleaseBonusY_ = 10.5f;          // ジャンプ射出時の上方向初速
    float jumpReleaseBonusX_ = 1.45f;          // ジャンプ射出時の接線速度倍率
    float maxAngle_ = 1.35f;                   // 各セグメントの最大振れ角（約77度）
    float climbSpeed_ = 3.0f;                  // ツタの上下移動（登り降り）速度 (m/s)

    // 状態管理
    Player2D* attachedPlayer_ = nullptr;       // 現在つかまっているプレイヤー（Rootのみ保持）
    float grabDistance_ = 0.0f;                // ピボットからのプレイヤー掴まり位置距離 (m)
    float cooldownTimer_ = 0.0f;               // 離脱後の再掴まりクールダウン
    bool isActive_ = true;                     // 有効/無効フラグ
    uint32_t lastUpdatedFrame_ = 0;            // 同一フレーム内の多重更新防止用フレームID
};
