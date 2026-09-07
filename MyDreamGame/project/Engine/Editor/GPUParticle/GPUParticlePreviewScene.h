#pragma once
#ifdef USE_IMGUI
#include "Scene/IScene.h"
#include <d3d12.h>
#include <memory>
#include <vector>
#include "GameObject/GameObject.h"
#include "GameObject/PrimitiveObject.h"
#include "Graphics/Skybox.h"

class GPUParticleSystem;

class GPUParticlePreviewScene : public IScene {
public:
    GPUParticlePreviewScene() = default;
    ~GPUParticlePreviewScene() override = default;

    void Initialize() override;
    void OnEnter(SceneManager* sceneManager) override;
    void OnExit(SceneManager* sceneManager) override;
    void Update(SceneManager* sceneManager) override;
    void UpdateEditor() override;
    void Draw(const Matrix4x4& viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    std::vector<Object3D*> GetObjects() override { return {}; }
    std::vector<std::shared_ptr<GameObject>> GetGameObjects() override { return gameObjects_; }
    std::vector<PrimitiveObject*> GetPrimitives() override;

    void SetParticleSystem(GPUParticleSystem* system) { particleSystem_ = system; }

private:
    std::vector<std::shared_ptr<GameObject>> gameObjects_;
    std::unique_ptr<PrimitiveObject> gridFloorObj_;
    std::unique_ptr<Skybox> skybox_;
    uint32_t skyboxTextureHandle_ = 0;
    GPUParticleSystem* particleSystem_ = nullptr;
    SceneManager* sceneManager_ = nullptr;
};
#endif
