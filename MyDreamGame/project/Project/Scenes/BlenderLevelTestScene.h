#pragma once
#include "Scene/IScene.h"
#include "Scene/LevelDataLoader.h"
#include "GameObject/Object3D.h"
#include "Graphics/Camera.h"
#include "Graphics/DebugCamera.h"
#include <memory>
#include <vector>

class BlenderLevelTestScene : public IScene {
public:
    BlenderLevelTestScene() = default;
    ~BlenderLevelTestScene() override = default;

    void Initialize() override;
    void OnEnter(SceneManager* sceneManager) override;
    void OnExit(SceneManager* sceneManager) override;
    void Update(SceneManager* sceneManager) override;
    void UpdateEditor() override;
    void Draw(const Matrix4x4& viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    std::vector<Object3D*> GetObjects() override;

private:
    void ReloadLevel(const std::string& filePath);

private:
    std::unique_ptr<LevelDataLoader> levelDataLoader_;
    std::vector<std::unique_ptr<Object3D>> levelObjects_;

    // 3D空間確認用カメラ
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<DebugCamera> debugCamera_;

    bool useDebugCamera_ = true;
    std::string currentLevelPath_ = "c:/1_授業/学年/3年前期/TL1/TL.json";
};
