#pragma once
#include "PlayerConfig.h"
#include "PlayerInput.h"
#include "PlayerPhysics.h"

#include "Component/IComponent.h"
#include "Core/TimeManager.h"
#include "GameObject/PrimitiveObject.h"
#include "Input/KeyboardInput.h"
#include "PlayerState.h"
#include "PlayerVisuals.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "States/IPlayerState.h"
#include <nlohmann/json.hpp>

// 前方宣言
class MapChip2D;
class GameCamera;
class PlayerNormalState;
class PlayerDeadState;
class PlayerRespawnState;
class PlayerGoalState;
class PlayerNormalState;
class PlayerDeadState;
class PlayerRespawnState;
class PlayerGoalState;

/// <summary>
/// 2Dスクロールゲーム用プレイヤークラス
/// Componentシステムに対応
/// </summary>
class Player2D : public IComponent {

public:
    // --- 1. 状態取得（読み取り専用にする！） ---
    // 非constの GetState() を廃止し、const参照のみ公開
    const PlayerState &GetState() const { return state_; }

    // --- 2. 通常移動・部屋管理用 ---
    void ResetActionFlags();                   // 死亡・ゴール・リスポーンフラグのリセット
    void UpdateWallJumpTimer(float deltaTime); // 壁ジャンプタイマーの減算
    void UpdatePhysics(float deltaTime);       // 物理演算の実行（内部でphysics_.Updateを呼ぶ）
    void SetCurrentRoomIndex(int roomIndex) { state_.currentRoomIndex_ = roomIndex; }
    void UpdateRunDust(float deltaTime); // 走りエフェクトタイマー更新
    void UpdateVisualColor();            // 状態（ダッシュ・疲労等）に応じた描画色の更新
    // --- 3. 死亡ステート用 ---
    void StartDeathSequence(bool isFallDeath);  // 死亡開始（初速・スロー・フラグ設定）
    void UpdateDeathKnockback(float deltaTime); // 死亡時のノックバック物理とタイマー進行
    void RespawnAt(const Vector3 &respawnPos);  // リスポーン地点へ復帰＆慣性リセット
    void SetCheckpoint(const Vector3& pos, int score) { state_.lastCheckpointPosition_ = pos; state_.checkpointScore_ = score; }
    const Vector3& GetCheckpointPosition() const { return state_.lastCheckpointPosition_; }
    int GetCheckpointScore() const { return state_.checkpointScore_; }
    void RestoreCheckpointScore() { state_.score_ = state_.checkpointScore_; }
    float GetDeathTimer() const { return state_.deathTimer_; }
    // --- 4. リスポーンステート用 ---
    void StartRespawnSequence(); // リスポーン開始（タイマー初期化等）
    void UpdateRespawnTimer(float deltaTime) { state_.respawnTimer_ += deltaTime; }
    void FinishRespawn(); // リスポーン完了時の後処理
    float GetRespawnTimer() const { return state_.respawnTimer_; }
    // --- 5. ゴールステート用 ---
    void StartGoalSequence();              // ゴール開始（紙吹雪・速度ゼロ等）
    void UpdateGoalTimer(float deltaTime); // ゴールタイマー進行
    float GetGoalTimer() const { return state_.goalTimer_; }
    PlayerVisuals &GetVisuals() { return visuals_; }
    PlayerPhysics &GetPhysics() { return physics_; }
    const InputState &GetCurrentInput() const { return currentInput_; }
    Player2D() = default;
    ~Player2D() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DisplayImGui() override;

    // TODO: Mapは別途シーンかServiceLocator等から取得するように変更するまでの暫定
    void UpdateWithMap(MapChip2D &map, bool isTransitioning = false, bool canControl = true);

    // 速度の設定と取得
    void SetVelocity(const Vector3 &velocity) { state_.velocity_ = velocity; }
    Vector3 GetVelocity() const { return state_.velocity_; }
    void SetExternalVelocityX(float velX) { state_.externalVelocityX_ = velX; }
    void SetIsOnGround(bool state) { state_.isOnGround_ = state; }

    // ダッシュ回復・キャンセルアクション
    void RefillDash() { state_.canDash_ = true; }
    void CancelDash() {
        state_.isDashing_ = false;
        state_.dashTimer_ = 0.0f;
    }
    void RefillStamina() {
        state_.stamina_ = params_.maxStamina_;
        state_.isExhausted_ = false;
    }
    bool CanDash() const { return state_.canDash_; }
    bool IsExhausted() const { return state_.isExhausted_; }
    float GetStamina() const { return state_.stamina_; }
    void ApplyHitstop(float duration) { state_.hitstopTimer_ = duration; }
    void SetSpringControlDisable(float duration) { state_.springControlDisableTimer_ = duration; }
    void SetIsGrabbingVine(bool grabbing) { state_.isGrabbingVine_ = grabbing; }
    bool IsGrabbingVine() const { return state_.isGrabbingVine_; }

    // ダッシュ状態の取得
    bool IsDashing() const { return state_.isDashing_; }
    const Vector3 &GetDashVelocity() const { return state_.dashVelocity_; } // JSON Parameters

    // プレイヤーの位置を取得（カメラ追従用）
    const Vector3 &GetPosition() const { return state_.position_; }
    void SetPosition(const Vector3 &pos) { state_.position_ = pos; }
    const Vector3 &GetStartPosition() const { return state_.startPosition_; }

    // カメラの設定と取得
    void SetCamera(GameCamera *camera) { camera_ = camera; }
    GameCamera *GetCamera() const { return camera_; }

    // マップからプレイヤー初期位置を検索して設定する
    void FindSpawnPoint(const MapChip2D &map);

    // AABBの取得（当たり判定用）

    // 将来の拡張用 OBB（Oriented Bounding Box）構造体
    struct OBB2D {
        Vector3 center;
        Vector3 extents; // half-width, half-height, z=0
        float rotation;  // radian
    };

    // AABB同士の交差判定ヘルパー
    static bool CheckAABBCollision(const AABB2D &a, const AABB2D &b);

    // OBBを用いた衝突判定（戻り値はMTV: Minimum Translation Vector）
    // （今回は不使用ですが将来のリフト回転対応用として実装）
    static bool CheckCollisionOBB(const OBB2D &obb1, const OBB2D &obb2, Vector3 &outMTV);

    // ヒエラルキー用
    PrimitiveObject *GetPrimitiveObject() { return visuals_.GetPrimitiveObject(); }
    const PrimitiveObject *GetPrimitiveObject() const { return visuals_.GetPrimitiveObject(); }
    Object3D *GetModelObject() { return visuals_.GetModelObject(); }
    const Object3D *GetModelObject() const { return visuals_.GetModelObject(); }
    AnimatorComponent *GetAnimator() { return visuals_.GetAnimator(); }
    const AnimatorComponent *GetAnimator() const { return visuals_.GetAnimator(); }

    // ゲーム状態取得用
    int GetScore() const { return state_.score_; }
    void SetScore(int score) { state_.score_ = score; }

    // リプレイ巻き戻し用の状態復元メソッド

    // ブロックのOnCollisionから呼ばれるコールバック群
    void Kill(bool isFallDeath = false);
    void ReachGoal();
    void AddScore(int score) {
        state_.score_ += score;
    }

    // ステート遷移（ポリモーフィズム）
    void ChangeState(std::unique_ptr<IPlayerState> nextState);
    IPlayerState *GetCurrentState() const { return currentState_.get(); }

    bool IsGoalComplete() const { return state_.isGoal_ && state_.goalTimer_ >= params_.goalWaitTime_; }

    // リプレイのループやシーク時に物理状態（速度や各種フラグ）をリセットする
    void ResetState(const Vector3 &initPos);
    void ClearEffects() { visuals_.ClearEffects(); }

    // ゲーム状態取得用ゲッター追加
    AABB2D GetAABB() const;
    bool IsDead() const { return state_.isDead_; }
    bool IsRespawning() const { return state_.isRespawning_; }
    bool IsGoal() const { return state_.isGoal_; }
    const PlayerParams &GetParams() const { return params_; }

private:
    PlayerParams params_;

    PlayerState state_;
    PlayerVisuals visuals_;
    PlayerInput input_;
    InputState currentInput_;
    PlayerPhysics physics_;

    // 現在のアクションステート（ポリモーフィズム管理）
    std::unique_ptr<IPlayerState> currentState_;

    GameCamera *camera_ = nullptr;
};
