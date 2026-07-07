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
#include "Core/Utility/Structs.h"
#include <memory>
#include <vector>
#include <random>
#include <string>
#include <nlohmann/json.hpp>

// 前方宣言
class MapChip2D;

/// <summary>
/// 2Dスクロールゲーム用プレイヤークラス
/// PrimitiveObject(Box)を内部に持ち、重力・移動・ジャンプを処理する
/// </summary>
class Player2D {
public:
    void Initialize(ID3D12GraphicsCommandList* commandList);
    void Update(MapChip2D& map, bool isTransitioning = false);
    void Draw(ID3D12GraphicsCommandList* commandList);

    // 速度の設定と取得
    void SetVelocity(const Vector3& velocity) { state_.velocity_ = velocity; }
    Vector3 GetVelocity() const { return state_.velocity_; }
    void SetExternalVelocityX(float velX) { state_.externalVelocityX_ = velX; }
    void SetIsOnGround(bool state) { state_.isOnGround_ = state; }
    void DisplayImGui();
    
    // JSON Parameters

    // プレイヤーの位置を取得（カメラ追従用）
    const Vector3& GetPosition() const { return state_.position_; }
    void SetPosition(const Vector3& pos) { state_.position_ = pos; }

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
    void Kill() {
        if (!state_.isDead_) {
            state_.isDead_ = true;
            state_.isRespawning_ = false;
            state_.deathTimer_ = 0.0f;
            // 後ろによろける演出のための速度設定 (よろけ具合を約半分に低減)
            state_.velocity_ = { state_.velocity_.x > 0.0f ? -2.5f : (state_.velocity_.x < 0.0f ? 2.5f : -2.5f), 4.0f, 0.0f };
            state_.isDashing_ = false;
            // スローモーション開始
            TimeManager::GetInstance().SetTimeScale(0.3f);
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

    // ゲーム状態取得用ゲッター追加
    AABB2D GetAABB() const;
    void SimulateCollisions(MapChip2D& map);
    bool IsDead() const { return state_.isDead_; }
    bool IsGoal() const { return state_.isGoal_; }

private:

private:

    PlayerParams params_;

    PlayerState state_;
    PlayerVisuals visuals_;
    PlayerInput input_;
    InputState currentInput_;
    PlayerPhysics physics_;





};
