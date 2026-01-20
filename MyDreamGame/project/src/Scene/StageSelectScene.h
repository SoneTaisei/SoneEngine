#pragma once
#include "IScene.h"
#include "Model/Model.h"
#include <d3d12.h>
#include <memory>

class StageSelectScene : public IScene {
public:
    ~StageSelectScene() override;
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;

private:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    std::vector<std::unique_ptr<Model>> models_;

    Transform cameraTransform_; // カメラの座標・回転
    Matrix4x4 viewProjection_;  // 描画に使う行列
};
