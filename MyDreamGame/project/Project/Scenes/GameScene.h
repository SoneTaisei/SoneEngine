#pragma once
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"  // これが必要
#include "Effect/ParticleManager.h" // これが必要
#include "Game2D/Effect/GameEffectManager.h"
#include "Scene/IScene.h"
#include "Core/Utility/TransformFunctions.h" // 行列計算用
#include <memory>
#include <vector>

// 2Dゲーム用クラス
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/UI/CoinHUD.h"
#include "Game2D/UI/StaminaHUD.h"
#include "Game2D/UI/PauseMenu.h"
#include "Game2D/UI/ReadyGoUI.h"
#include "Game2D/UI/ClearUI.h"
#include "Game2D/Replay/GameReplayController.h"
#include "Game2D/Replay/ReplayGhostRenderer.h"
#include "Game2D/Camera/GameCameraController.h"
#include "Resource/Sprite/Sprite.h"
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "State/StateMachine.h"
#include "Game2D/IGameUI.h"

class GameCamera;
class IGameSceneState;
class Skybox;


class GameScene : public IScene {
public:
    GameScene();
    ~GameScene() override;

    static void SetTargetMapFilePath(const std::string &path) { s_TargetMapFilePath = path; }
    static const std::string &GetTargetMapFilePath() { return s_TargetMapFilePath; }

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void Draw2D() override;
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

    std::string GetLevelDataJsonPath() const override;

    
    void ChangeState(std::unique_ptr<IState<GameScene>> nextState);
    IGameSceneState *GetCurrentState() const;

     // ステートから使うゲッター群
    ReadyGoUI *GetReadyGoUI() const { return readyGoUI_.get(); }
    ClearUI *GetClearUI() const { return clearUI_.get(); }
    PauseMenu *GetPauseMenu() const { return pauseMenu_.get(); }
    GameCameraController *GetCameraController() const { return cameraController_.get(); }
    SceneManager *GetSceneManager() const { return activeSceneManager_; }

private:
    StateMachine<GameScene> stateMachine_;
    SceneManager *activeSceneManager_ = nullptr;
    // ---------------------------------------------------
    // 3D・エフェクト関連 (develop)
    // ---------------------------------------------------
    std::unique_ptr<GameEffectManager> effectManager_;

    // カメラ用行列（Updateで必要なためメンバに追加）
    Matrix4x4 viewProjection_ = TransformFunctions::MakeIdentity4x4();
    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();

    // ---------------------------------------------------
    // 2Dゲーム用オブジェクト (Game_develop)
    std::unique_ptr<GameObject> playerObj_;
    Player2D* player_ = nullptr;
    std::unique_ptr<MapChip2D> map_;

    int previousScore_ = 0; // コインエフェクト発生用
    
    // 状態追跡用フラグ（Update内のstatic変数をメンバ化）
    bool wasCurrentlyPlaying_ = false;
    bool wasRespawning_ = false;
    
    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    // ---------------------------------------------------
    // 共通システム
    // ---------------------------------------------------
    // コマンドリストを覚えておくための変数
    


    float transitionAlpha_ = 1.0f; // 画面遷移演出用(フェードイン)

    // UIコンポーネント
    std::unique_ptr<ReadyGoUI> readyGoUI_;
    std::unique_ptr<ClearUI> clearUI_;
    std::unique_ptr<PauseMenu> pauseMenu_;
    std::unique_ptr<CoinHUD> coinHUD_;
    std::unique_ptr<StaminaHUD> staminaHUD_;

    // カメラ制御
    std::unique_ptr<GameCameraController> cameraController_;

    // リプレイ・ゴースト描画
    std::unique_ptr<GameReplayController> replayController_;
    std::unique_ptr<ReplayGhostRenderer> ghostRenderer_;

    static std::string s_TargetMapFilePath;

    std::vector<IGameUI *> allUIs_;
};