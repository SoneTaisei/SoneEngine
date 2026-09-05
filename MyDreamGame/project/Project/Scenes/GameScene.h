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

class GameCamera;

#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
class Skybox;


enum class GameState {
    StartReady,
    Playing,
    Clear
};

class GameScene : public IScene {
public:
    static std::string s_TargetMapFilePath;

    ~GameScene() override;

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
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

    // プレイヤー座標を基準とするアイリスイン（フェードイン）演出
    void StartIrisIn(const Vector3& playerPos, float duration = 1.2f);
    void UpdateIrisIn(const Vector3& playerPos, float dt);
    Vector2 WorldToScreenUV(const Vector3& worldPos) const;
    bool IsIrisInActive() const { return isIrisInActive_; }

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
    
    // 状態追跡用フラグ（Update内のstatic変数をメンバ化）
    bool wasCurrentlyPlaying_ = false;
    bool wasPlayingLastFrame_ = false;
    bool wasRewindingLastFrame_ = false;
    
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