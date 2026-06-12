#pragma once
#include "Scene/IScene.h"
#include "Resource/Model/Model.h"
#include <d3d12.h>
#include <memory>
#include "GameObject/Object3D.h"
#include "Graphics/Skybox.h"

class StageSelectScene : public IScene {
public:
    ~StageSelectScene() override;
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override;

private:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    std::vector<std::unique_ptr<Object3D>> objects_;

    Transform cameraTransform_; // カメラの座標・回転
    Matrix4x4 viewProjection_;  // 描画に使う行列

    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;
};
