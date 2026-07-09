#pragma once
#include "Scene/IScene.h"
#include "Resource/Model/Model.h"
#include <d3d12.h>
#include <memory>
#include "GameObject/Object3D.h"
#include "Graphics/Skybox.h"

#include <string>
#include <vector>

class StageSelectScene : public IScene {
public:
    ~StageSelectScene() override;
    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;

private:
    void SaveConfig();
    void LoadConfig();

    struct StageConfig {
        char jsonPath[256];
    };

    int currentStageIndex_ = 0;
    int stageCount_ = 1;
    std::vector<StageConfig> stageConfigs_;

    

    std::vector<std::unique_ptr<Object3D>> objects_;

    Transform cameraTransform_; // カメラの座標・回転
    Matrix4x4 viewProjection_;  // 描画に使う行列

    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    float inputDelayTimer_ = 0.5f; // シーン遷移直後の入力受付までの遅延時間（秒）

    std::vector<std::string> availableMapFiles_;
    void RefreshAvailableMapFiles();
};
