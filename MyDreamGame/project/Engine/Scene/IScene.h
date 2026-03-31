#pragma once
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include"Core/Utility/UtilityFunctions.h"

// 前方宣言
class SceneManager;
class SpriteCommon;
class ModelCommon;
class ParticleCommon;

class IScene {
public:
    virtual ~IScene() = default;

    // 初期化
    virtual void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) = 0;

    // 更新
    virtual void Update(SceneManager *sceneManager) = 0;

    // 描画
    virtual void Draw(const Matrix4x4 &viewProjectionMatrix) = 0;

    // セット用関数
    void SetSpriteCommon(SpriteCommon* spriteCommon) {
     // std::move で所有権を渡す
        spriteCommon_ = spriteCommon;
    }

    virtual void SetModelCommon(ModelCommon* modelCommon) {
        modelCommon_ = modelCommon;
    }

    virtual void SetParticleCommon(ParticleCommon *particleCommon) {
        particleCommon_ = particleCommon;
    }
protected:
    // 継承先(TitleSceneなど)で使えるようにする
    SpriteCommon *spriteCommon_ = nullptr;
    ModelCommon* modelCommon_ = nullptr;
    ParticleCommon *particleCommon_ = nullptr;
    
};