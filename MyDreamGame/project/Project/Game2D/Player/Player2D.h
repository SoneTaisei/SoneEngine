#pragma once
#include "PlayerConfig.h"
#include "PlayerPhysics.h"
#include "PlayerInput.h"

#include "PlayerState.h"
#include "PlayerVisuals.h"
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Input/KeyboardInput.h"
#include "Core/TimeManager.h"
#include <memory>
#include <vector>
#include <random>
#include <string>
#include <nlohmann/json.hpp>
#include "Component/IComponent.h"

// 前方宣言
class MapChip2D;
class GameCamera;

/// <summary>
/// 2Dスクロールゲーム用プレイヤークラス
/// Componentシステムに対応
/// </summary>
class Player2D : public IComponent {
public:
    Player2D() = default;
    ~Player2D() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DisplayImGui() override;

    // TODO: Mapは別途シーンかServiceLocator等から取得するように変更するまでの暫定
    void UpdateWithMap(MapChip2D& map, bool isTransitioning = false);

    // 速度の設定と取得
    void SetVelocity(const Vector3& velocity) { state_.velocity_ = velocity; }
    Vector3 GetVelocity() const { return state_.velocity_; }
    void SetExternalVelocityX(float velX) { state_.externalVelocityX_ = velX; }
    void SetIsOnGround(bool state) { state_.isOnGround_ = state; }
    
    // セレステ風ジャンプブロック（ばね）用のアクション
    void RefillDash() { state_.canDash_ = true; state_.isDashing_ = false; state_.dashTimer_ = 0.0f; }
    void ApplyHitstop(float duration) { state_.hitstopTimer_ = duration; }
    void SetSpringControlDisable(float duration) { state_.springControlDisableTimer_ = duration; }
    // JSON Parameters

    // プレイヤーの位置を取得（カメラ追従用）
    const Vector3& GetPosition() const { return state_.position_; }
    void SetPosition(const Vector3& pos) { state_.position_ = pos; }
    const Vector3& GetStartPosition() const { return state_.startPosition_; }

    // カメラの設定と取得
    void SetCamera(GameCamera* camera) { camera_ = camera; }
    GameCamera* GetCamera() const { return camera_; }

    // マップからプレイヤー初期位置を検索して設定する
    void FindSpawnPoint(const MapChip2D& map);

    // AABBの取得（当たり判定用）

    // 将来の拡張用 OBB（Oriented Bounding Box）構造体
    struct OBB2D {
        Vector3 center;
        Vector3 extents; // half-width, half-height, z=0
        float rotation;  // radian
    };

    // AABB同士の交差判定ヘルパー
    static bool CheckAABBCollision(const AABB2D& a, const AABB2D& b);
    
    // OBBを用いた衝突判定（戻り値はMTV: Minimum Translation Vector）
    // （今回は不使用ですが将来のリフト回転対応用として実装）
    static bool CheckCollisionOBB(const OBB2D& obb1, const OBB2D& obb2, Vector3& outMTV);

    // ヒエラルキー用
    PrimitiveObject* GetPrimitiveObject() { return visuals_.GetPrimitiveObject(); }

    // ゲーム状態取得用
    int GetScore() const { return state_.score_; }
    void SetScore(int score) { state_.score_ = score; }

    // リプレイ巻き戻し用の状態復元メソッド

    // ブロックのOnCollisionから呼ばれるコールバック群
    void Kill(bool isFallDeath = false) {
        if (!state_.isDead_) {
            state_.isDead_ = true;
            state_.isRespawning_ = false;
            state_.deathTimer_ = 0.0f;
            if (isFallDeath) {
                // 落下・逸脱死の場合：上に跳ねず、下方向への初速を与えて重力で落とす
                state_.velocity_.y = -5.0f; // 下方向への初速
                state_.velocity_.x *= 0.2f; // 横方向の慣性はほぼなくす
                state_.isDashing_ = false;
                // スローモーションはかけず、通常速度で落ちていくようにする
                TimeManager::GetInstance().SetTimeScale(1.0f);
            } else {
                // 後ろによろける演出のための速度設定 (よろけ具合を約半分に低減)
                state_.velocity_ = { state_.velocity_.x > 0.0f ? -2.5f : (state_.velocity_.x < 0.0f ? 2.5f : -2.5f), 4.0f, 0.0f };
                state_.isDashing_ = false;
                // スローモーション開始
                TimeManager::GetInstance().SetTimeScale(0.3f);
            }
        }
    }
    void ReachGoal() {
        if (!state_.isGoal_) {
            state_.isGoal_ = true;
            state_.goalTimer_ = 0.0f;
            state_.velocity_ = { 0.0f, 0.0f, 0.0f };
            state_.isDashing_ = false;
            visuals_.SpawnConfetti(state_.position_);
        }
    }
    void AddScore(int score) {
        state_.score_ += score;
    }

    bool IsGoalComplete() const { return state_.isGoal_ && state_.goalTimer_ >= params_.goalWaitTime_; }

    // リプレイのループやシーク時に物理状態（速度や各種フラグ）をリセットする
    void ResetState(const Vector3& initPos);
    void ClearEffects() { visuals_.ClearEffects(); }

    // ゲーム状態取得用ゲッター追加
    AABB2D GetAABB() const;
    bool IsDead() const { return state_.isDead_; }
    bool IsGoal() const { return state_.isGoal_; }
    const PlayerParams& GetParams() const { return params_; }

private:

private:

    PlayerParams params_;

    PlayerState state_;
    PlayerVisuals visuals_;
    PlayerInput input_;
    InputState currentInput_;
    PlayerPhysics physics_;

    GameCamera* camera_ = nullptr;
};
