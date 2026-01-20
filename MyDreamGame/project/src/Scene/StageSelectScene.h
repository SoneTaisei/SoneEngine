#pragma once
#include "IScene.h"
#include <d3d12.h>
#include <memory>
#include "Model/Model.h"
#include "GameObject/Object3D.h"

class StageSelectScene : public IScene {
public:
    ~StageSelectScene() override;
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;

private:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

     std::vector<std::unique_ptr<Model>> models_;
    std::vector<std::unique_ptr<Object3D>> objects_;
};
