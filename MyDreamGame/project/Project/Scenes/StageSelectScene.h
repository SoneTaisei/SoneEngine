#pragma once
#include "Scene/IScene.h"
#include "Resource/Model/Model.h"
#include <d3d12.h>
#include <memory>
#include "GameObject/GameObject.h"
#include "Component/MeshRendererComponent.h"
#include "Component/PrimitiveRendererComponent.h"
#include "Component/AnimatorComponent.h"
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
    std::vector<std::shared_ptr<GameObject>> GetGameObjects() override { return gameObjects_; }

private:
    void SaveConfig();
    void LoadConfig();

    struct StageConfig {
        char jsonPath[256];
    };

    int currentStageIndex_ = 0;
    int stageCount_ = 1;
    std::vector<StageConfig> stageConfigs_;

    

    std::vector<std::shared_ptr<GameObject>> gameObjects_;

    EulerTransform cameraTransform_; // カメラの座標・回転
    Matrix4x4 viewProjection_;  // 描画に使う行列

    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

    float inputDelayTimer_ = 0.5f; // シーン遷移直後の入力受付までの遅延時間（秒）

    static constexpr int kMaxSelectableStages = 3;

    struct StageBoxData {
        std::shared_ptr<GameObject> gameObject;
        Vector3 position{0.0f, 0.0f, 0.0f};
        Vector3 rotation{0.0f, 0.0f, 0.0f};
        float scale = 1.0f;
    };
    std::vector<StageBoxData> stageBoxes_;

    float boxSpacing_ = 3.2f;
    float selectedScale_ = 1.8f;
    float unselectedScale_ = 1.2f;
    float selectedZ_ = 0.0f;
    float unselectedZ_ = 1.5f;
    float rotateSpeed_ = 1.2f;
    bool enableLighting_ = false;

    std::vector<std::string> availableMapFiles_;
    void RefreshAvailableMapFiles();
};
