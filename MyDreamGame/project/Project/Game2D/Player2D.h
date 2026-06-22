#pragma once
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Input/KeyboardInput.h"
#include "Core/TimeManager.h"
#include "Core/Utility/Structs.h"
#include <memory>
#include <vector>
#include <random>

// 前方宣言
class MapChip2D;

/// <summary>
/// 2Dスクロールゲーム用プレイヤークラス
/// PrimitiveObject(Box)を内部に持ち、重力・移動・ジャンプを処理する
/// </summary>
class Player2D {
public:
    void Initialize(ID3D12GraphicsCommandList* commandList);
    void Update(MapChip2D& map);
    void Draw(ID3D12GraphicsCommandList* commandList);
    void DisplayImGui();

    // プレイヤーの位置を取得（カメラ追従用）
    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3& pos) { position_ = pos; }

    // マップからプレイヤー初期位置を検索して設定する
    void FindSpawnPoint(const MapChip2D& map);

    // AABBの取得（当たり判定用）
    struct AABB {
        float left, top, right, bottom;
    };
    AABB GetAABB() const;

    // 将来の拡張用 OBB（Oriented Bounding Box）構造体
    struct OBB2D {
        Vector3 center;
        Vector3 extents; // half-width, half-height, z=0
        float rotation;  // radian
    };

    // AABB同士の交差判定ヘルパー
    static bool CheckAABBCollision(const AABB& a, const AABB& b);
    
    // OBBを用いた衝突判定（戻り値はMTV: Minimum Translation Vector）
    // （今回は不使用ですが将来のリフト回転対応用として実装）
    static bool CheckCollisionOBB(const OBB2D& obb1, const OBB2D& obb2, Vector3& outMTV);

    // ヒエラルキー用
    PrimitiveObject* GetPrimitiveObject() { return primitiveObj_.get(); }

    // ゲーム状態取得用
    int GetScore() const { return score_; }
    void SetScore(int score) { score_ = score; }

    // リプレイ巻き戻し用の状態復元メソッド
    void SimulateCollisions(MapChip2D& map);

    // ブロックのOnCollisionから呼ばれるコールバック群
    void Kill() {
        if (!isDead_) {
            isDead_ = true;
            isRespawning_ = false;
            deathTimer_ = 0.0f;
            // 後ろによろける演出のための速度設定 (よろけ具合を約半分に低減)
            velocity_ = { velocity_.x > 0.0f ? -2.5f : (velocity_.x < 0.0f ? 2.5f : -2.5f), 4.0f, 0.0f };
            isDashing_ = false;
            // スローモーション開始
            TimeManager::GetInstance().SetTimeScale(0.3f);
        }
    }
    void ReachGoal() {
        if (!isGoal_) {
            isGoal_ = true;
            goalTimer_ = 0.0f;
            velocity_ = { 0.0f, 0.0f, 0.0f };
            isDashing_ = false;
            SpawnConfetti();
        }
    }
    void AddScore(int score) {
        score_ += score;
    }

    bool IsGoalComplete() const { return isGoal_ && goalTimer_ >= goalWaitTime_; }

private:
    // 入力処理
    void HandleInput();

    // 物理シミュレーション
    void ApplyGravity(float deltaTime);

    // マップとの当たり判定
    void ResolveCollisionY(const MapChip2D& map);
    void ResolveCollisionX(const MapChip2D& map);

private:
    std::unique_ptr<PrimitiveObject> primitiveObj_;

    // 物理パラメータ
    Vector3 position_ = { 2.0f, 5.0f, 0.0f }; // 初期位置
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };

    // 足場（リフト）関連
    bool isOnMovingPlatform_ = false;
    Vector3 platformVelocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 recentPlatformVelocity_ = { 0.0f, 0.0f, 0.0f }; // 慣性保存用
    float platformInertiaTimer_ = 0.0f; // 慣性猶予時間（コヨーテタイム）
    float externalVelocityX_ = 0.0f; // 慣性用の外部速度

    float moveSpeed_ = 5.0f;       // 左右移動速度
    float jumpPower_ = 10.0f;      // ジャンプ力
    float gravity_ = -20.0f;       // 重力加速度
    float maxFallSpeed_ = -15.0f;  // 最大落下速度

    bool isOnGround_ = false;      // 地面にいるか
    
    // ダッシュ用パラメータ
    bool canDash_ = true;          // ダッシュ可能か（接地で回復）
    bool isDashing_ = false;       // ダッシュ中か
    float dashTimer_ = 0.0f;       // ダッシュ経過時間
    float dashDuration_ = 0.15f;   // ダッシュ継続時間
    float dashSpeed_ = 15.0f;      // ダッシュの速さ
    Vector3 dashVelocity_ = {0.0f, 0.0f, 0.0f}; // ダッシュ中の固定速度

    // プレイヤーの色
    Vector4 colorNormal_ = { 0.2f, 0.6f, 1.0f, 1.0f }; // 通常時（青）
    Vector4 colorDashed_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // ダッシュ使用後（白）

    // 壁ジャンプ用パラメータ
    bool isTouchingWallRight_ = false;
    bool isTouchingWallLeft_ = false;
    float wallJumpTimer_ = 0.0f;       // 壁ジャンプ後の入力制限時間
    float wallJumpDuration_ = 0.2f;    // 制限時間の長さ
    Vector2 wallJumpPower_ = { 8.0f, 10.0f }; // 壁ジャンプ時の X, Y 速度
    
    // 壁ずり落ち・張り付き用パラメータ
    float wallSlideSpeed_ = -2.0f;     // 壁ずり落ち時の最大落下速度
    bool isWallSliding_ = false;       // 壁ずり落ち中か
    bool isWallClinging_ = false;      // 壁張り付き中か

    float halfWidth_ = 0.4f;
    float halfHeight_ = 0.4f;

    // 死亡演出用パラメータ
    bool isDead_ = false;           // 死亡演出中か
    float deathTimer_ = 0.0f;       // 死亡経過時間
    float deathDuration_ = 0.175f;  // 死亡演出の時間 (ノックバックしながらディゾルブする)
    Vector3 startPosition_ = { 2.0f, 5.0f, 0.0f }; // スタート地点・リスポーン位置

    // リスポーン演出用パラメータ
    bool isRespawning_ = false;
    float respawnTimer_ = 0.0f;
    float respawnDuration_ = 0.5f;

    // ゴール・スコア用パラメータ
    bool isGoal_ = false;
    float goalTimer_ = 0.0f;
    float goalWaitTime_ = 2.0f;
    int score_ = 0;
    
    // 砂埃エフェクト用パラメータ
    struct DustParticle {
        Vector3 position;
        Vector3 velocity;
        float timer;
        float duration;
        float startSize;
        bool active;
    };
    std::vector<DustParticle> dustParticles_;
    
    // 砂埃を発生させる
    void SpawnJumpDust(const Vector3& basePos, float dirX);
    void SpawnRunDust(const Vector3& basePos, float dirX);
    
    float runDustTimer_ = 0.0f;
    float runDustInterval_ = 0.1f;

    // 紙吹雪エフェクト用パラメータ
    struct ConfettiParticle {
        Vector3 position;
        Vector3 velocity;
        Vector4 color;
        Vector3 rotation;
        Vector3 rotationSpeed;
        float timer;
        float duration;
        float size;
        bool active;
    };
    std::vector<ConfettiParticle> confettiParticles_;
    
    // 紙吹雪を発生させる
    void SpawnConfetti();

    // イージング関数
    float EaseInElastic(float t) const;
};
