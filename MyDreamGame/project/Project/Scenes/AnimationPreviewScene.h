#pragma once
#include "Scene/IScene.h"
#include "Resource/Model/Model.h"
#include <d3d12.h>
#include <memory>
#include <vector>
#include "GameObject/GameObject.h"
#include "GameObject/PrimitiveObject.h"
#include "Component/MeshRendererComponent.h"
#include "Component/AnimatorComponent.h"
#include "Graphics/Skybox.h"

class AnimationPreviewScene : public IScene {
public:
    AnimationPreviewScene() = default;
    ~AnimationPreviewScene() override = default;

    void Initialize() override;
    void OnEnter(SceneManager *sceneManager) override;
    void OnExit(SceneManager *sceneManager) override;
    void Update(SceneManager *sceneManager) override;
    void UpdateEditor() override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;
    std::vector<std::shared_ptr<GameObject>> GetGameObjects() override { return gameObjects_; }
    std::vector<PrimitiveObject *> GetPrimitives() override;

private:
    std::vector<std::shared_ptr<GameObject>> gameObjects_;
    std::shared_ptr<GameObject> playerObject_;
    std::unique_ptr<PrimitiveObject> gridFloorObj_;
    std::unique_ptr<Skybox> skybox_;
    uint32_t skyboxTextureHandle_ = 0;
};
