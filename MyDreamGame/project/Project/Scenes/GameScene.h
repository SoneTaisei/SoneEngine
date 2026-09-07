#pragma once
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"
#include "Effect/ParticleManager.h"
#include "Scene/IScene.h"
#include "Core/Utility/TransformFunctions.h"
#include <d3d12.h>
#include <memory>

// 2Dゲーム用クラス
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Chain/ChainManager.h"
#include "Game2D/Security/AlertSystem.h"

class GameCamera;
struct ImVec2;

#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
class Skybox;


enum class GameState {
    StartReady,
    Playing,
    Clear,
    Captured   // 警戒度が満タン → 捕獲演出 → ステージ選択へ
};

class GameScene : public IScene {
public:
    static std::string s_TargetMapFilePath;
    static bool s_QuickRestart; // ミス直後の読み直し：Ready の待ちを短くする

    ~GameScene() override;

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void Draw2D() override;
    void RenderShadowPass();
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;
    void DrawEditorOverlay(const Matrix4x4 &viewProjectionMatrix) override;
    void UpdateEditor() override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;
    std::vector<ParticleManager *> GetParticles() override;
    std::vector<PrimitiveObject *> GetPrimitives() override;

    // マップチップの取得
    MapChip2D* GetMapChip() override { return map_.get(); }

    // プレイヤーの取得
    Player2D* GetPlayer() override { return player_; }

    // プレイヤー座標を基準とするアイリスイン（フェードイン）・アイリスアウト演出
    void StartIrisIn(const Vector3& playerPos, float duration = 1.2f);
    void UpdateIrisIn(const Vector3& playerPos, float dt);
    void StartIrisOut(const Vector3& worldPos, float duration = 0.5f);
    void UpdateIrisOut(float dt);
    Vector2 WorldToScreenUV(const Vector3& worldPos) const;
    bool IsIrisInActive() const { return isIrisInActive_; }
    bool IsIrisOutActive() const { return isIrisOutActive_; }

private:
    // カメラ用行列（Updateで必要なためメンバに追加）
    Matrix4x4 viewProjection_ = TransformFunctions::MakeIdentity4x4();
    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();

    // ---------------------------------------------------
    // 2Dゲーム用オブジェクト
    std::unique_ptr<GameObject> playerObj_;
    Player2D* player_ = nullptr;
    std::unique_ptr<MapChip2D> map_;

    // 鎖の管理（プレイヤー鎖 + 吊り鎖 + 落とした自由鎖、ユニット制）
    std::unique_ptr<ChainManager> chainManager_;

    // 警戒度（0〜100。満タンで捕獲）
    std::unique_ptr<AlertSystem> alert_;
    void DrawAlertHud(const ImVec2& viewPos, float viewWidth, float viewHeight);
    void DrawCaptureOverlay(const ImVec2& viewPos, float viewWidth, float viewHeight);
    
    // 状態追跡用フラグ（Update内のstatic変数をメンバ化）
    bool wasCurrentlyPlaying_ = false;
    bool wasPlayingLastFrame_ = false;
    bool wasRewindingLastFrame_ = false;
    bool playerWasDead_ = false;      // 復活の検出（警戒度の猶予用）
    bool capturedByMiss_ = false;     // 捕獲画面の原因（true = 接触・落下などのミス、false = 見つかった回数）
    
    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    // マップ背景用板ポリゴン（スポットライト等のライティング視認用）
    std::unique_ptr<PrimitiveObject> backgroundPlane_;

    // ---------------------------------------------------
    // 共通システム
    // ---------------------------------------------------
    // コマンドリストを覚えておくための変数
    


    // ステージクリア遷移で覆い切った後の行き先（いったんステージ選択へ戻る）
    void GoToNextStage(SceneManager* sceneManager);

    // 灰色の背景壁（BackgroundPlane）の設定保存・読込
    void SaveBackgroundConfig();
    void LoadBackgroundConfig();

    // 警備員の懐中電灯スポットライトをModelCommonに同期
    void UpdateGuardLights();

    GameState gameState_ = GameState::StartReady;
    float stateTimer_ = 0.0f;
    float transitionAlpha_ = 0.0f; // 画面遷移演出用(フェードイン - アイリスインへ置き換え)

    // アイリスイン演出用
    bool isIrisInActive_ = false;
    float irisInTimer_ = 0.0f;
    float irisInDuration_ = 1.2f;
    float irisInMaxRadius_ = 3.2f; // 約2倍に拡大（画面全体を十分に覆う）

    // アイリスアウト演出用
    bool isIrisOutActive_ = false;
    float irisOutTimer_ = 0.0f;
    float irisOutDuration_ = 0.5f;
    Vector3 irisOutTargetPos_ = { 0.0f, 0.0f, 0.0f };

    // ---------------------------------------------------
    // 死亡演出（帽子・鎖・宝石の残留、アイリスアウト/インリスポーン）
    // ---------------------------------------------------
    std::shared_ptr<GameObject> deathHatObject_;
    bool isDeathHatActive_ = false;
    Vector3 deathHatPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 deathHatVelocity_ = { 0.0f, 0.0f, 0.0f };
    float deathHatFloorY_ = 0.0f;
    float deathHatRotationZ_ = 0.0f;
    bool isDeathHatGrounded_ = false;
    float deathHatGroundedTimer_ = 0.0f;
    bool isIrisOutStarted_ = false;

    bool isDeathSequenceActive_ = false;
    float deathSequenceTimer_ = 0.0f;
    Vector3 deathRespawnPos_ = { 0.0f, 0.0f, 0.0f };

    void TriggerDeathSequence();
    void UpdateDeathSequence(float dt, SceneManager* sceneManager);

    // ---------------------------------------------------
    // ポーズメニュー関連
    // ---------------------------------------------------
    bool isPaused_ = false;
    int pauseMenuIndex_ = 0; // 0: リトライ (restartText.png), 1: タイトル (titleText.png)
    float pausePulseTimer_ = 0.0f;
    float pauseCooldown_ = 0.0f;

    std::unique_ptr<class Sprite> pauseBackdropSprite_;
    std::unique_ptr<class Sprite> pauseTitleSprite_;
    std::unique_ptr<class Sprite> pauseRestartSprite_;
    std::unique_ptr<class Sprite> pauseTitleTextSprite_;

    uint32_t pauseBackdropTexHandle_ = 0;
    uint32_t pauseTitleTexHandle_ = 0;
    uint32_t pauseRestartTexHandle_ = 0;
    uint32_t pauseTitleTextTexHandle_ = 0;

    void UpdatePauseMenu(float dt, SceneManager* sceneManager);
    void DrawPauseMenu();
};