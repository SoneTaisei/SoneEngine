#pragma once
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"  // これが必要
#include "Effect/ParticleManager.h" // これが必要
#include "Effect/CoinEffect.h"
#include "Effect/CylinderEffect.h"
#include "Effect/RingEffect.h"
#include "Scene/IScene.h"
#include "Core/Utility/TransformFunctions.h" // 行列計算用
#include <d3d12.h>
#include <memory>

// 2Dゲーム用クラス
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"

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

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;
    void UpdateEditor() override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override { return {}; }
    std::vector<ParticleManager *> GetParticles() override;
    std::vector<PrimitiveObject *> GetPrimitives() override;

    // マップチップの取得
    MapChip2D* GetMapChip() override { return map_.get(); }

private:
    // ---------------------------------------------------
    // 3D・パーティクル関連 (develop)
    // ---------------------------------------------------
    // パーティクル管理クラス
    std::unique_ptr<CoinEffect> coinEffect_;
    std::unique_ptr<CylinderEffect> cylinderEffect_;
    std::unique_ptr<RingEffect> ringEffect_;

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
    bool wasPlayingLastFrame_ = false;
    bool wasRewindingLastFrame_ = false;
    
    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    // ---------------------------------------------------
    // 共通システム
    // ---------------------------------------------------
    // コマンドリストを覚えておくための変数
    

    GameState gameState_ = GameState::StartReady;
    float stateTimer_ = 0.0f;
    float transitionAlpha_ = 1.0f; // 画面遷移演出用(フェードイン)
};