#pragma once
#include "Scene/IScene.h"
#include <d3d12.h>
#include "Resource/Sprite/Sprite.h"
#include "Core/Utility/Utilityfunctions.h"
#include "Effect/ParticleManager.h"
#include <memory>
#include "Effect/ParticleCommon.h"
#include "GameObject/GameObject.h"
#include "Graphics/Skybox.h"
#include "Graphics/DebugCamera.h"
#include "GameObject/PrimitiveObject.h"
#include <vector>
#include "Resource/Model/ModelCommon.h"
#include "Resource/Sprite/SpriteCommon.h"

class TitleScene : public IScene {
public:
    ~TitleScene() override;
    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;
    std::vector<ParticleManager *> GetParticles() override;
    std::vector<PrimitiveObject *> GetPrimitives() override;

private:
    // UIスプライト
    uint32_t titleTextureHandle_ = 0;
    uint32_t startTextureHandle_ = 0;
    std::unique_ptr<Sprite> titleSprite_;
    std::unique_ptr<Sprite> startSprite_;

    // ■ タイトルシーン専用カメラ
    EulerTransform cameraTransform_{}; // カメラの座標・回転
    Matrix4x4 viewProjection_{};  // 描画に使う行列

    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    std::unique_ptr<DebugCamera> debugCamera_;

    // --- エディター停止中用 ---
    void UpdateEditor() override;

    bool isFirstFrame_ = true;
    float animationTime_ = 0.0f;
};
