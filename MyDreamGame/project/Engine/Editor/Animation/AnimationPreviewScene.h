#pragma once
#ifdef USE_IMGUI
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
#include "AnimationLightingConfig.h"

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

    // モデル追加・削除
    std::shared_ptr<GameObject> AddGameObjectFromModel(const std::string& directoryPath, const std::string& fileName, const std::string& displayName);
    bool RemoveGameObject(const std::shared_ptr<GameObject>& obj);

    // 選択中オブジェクト（表示対象のフィルタリング用）
    void SetSelectedGameObject(const std::shared_ptr<GameObject>& obj) { selectedGameObject_ = obj; }
    std::shared_ptr<GameObject> GetSelectedGameObject() const { return selectedGameObject_; }

    // アニメーションエディター専用ライティング
    AnimationLightingConfig& GetLightingConfig() { return lightingConfig_; }
    const AnimationLightingConfig& GetLightingConfig() const { return lightingConfig_; }
    void SetLightingConfig(const AnimationLightingConfig& cfg) { lightingConfig_ = cfg; }
    void ResetLightingConfig() { lightingConfig_.ResetToDefault(); }
    void SaveLightingConfig() { lightingConfig_.SaveToFile(); }
    void LoadLightingConfig() { lightingConfig_.LoadFromFile(); }

    // ヒエラルキーオブジェクトのJSON保存・読み込み・初期化
    bool SaveHierarchyToJson(const std::string& filePath = "");
    bool LoadHierarchyFromJson(const std::string& filePath = "");
    void ResetHierarchyToDefault();
    static const char* GetDefaultHierarchyJsonPath();

private:
    void ApplyAnimationLighting();
    std::shared_ptr<GameObject> CreateDefaultPlayerObject();

    std::vector<std::shared_ptr<GameObject>> gameObjects_;
    std::shared_ptr<GameObject> playerObject_;
    std::shared_ptr<GameObject> selectedGameObject_;
    std::unique_ptr<PrimitiveObject> gridFloorObj_;
    std::unique_ptr<Skybox> skybox_;
    uint32_t skyboxTextureHandle_ = 0;

    AnimationLightingConfig lightingConfig_;

    // アニメーションエディター専用の独立定数バッファ (GPU非同期競合を完全に排除)
    Microsoft::WRL::ComPtr<ID3D12Resource> animDirLightResource_;
    DirectionalLight* mappedAnimDirLight_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> animPointLightResource_;
    PointLight* mappedAnimPointLight_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> animSpotLightResource_;
    SpotLightGroup* mappedAnimSpotLight_ = nullptr;
};
#endif
