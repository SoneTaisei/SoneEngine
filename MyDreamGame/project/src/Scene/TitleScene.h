#pragma once
#include "IScene.h"
#include <d3d12.h>
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "Utility/Utilityfunctions.h"
#include "Effect/Particle.h"
#include <memory>

class TitleScene : public IScene {
public:
    ~TitleScene() override;
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;

private:
    // メンバ変数としてモデル、テクスチャ、座標を持つ
    uint32_t textureHandle_ = 0;
    Transform transform_ = {};
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    std::vector<std::unique_ptr<Model>> models_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::vector<std::unique_ptr<Particle>> particle_;

};
