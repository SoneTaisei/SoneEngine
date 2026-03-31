#pragma once
#include <memory>
#include "IScene.h"

class SpriteCommon;
class ModelCommon;
class ParticleCommon;

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList);
    void Update();
    void Draw(const Matrix4x4 &viewProjectionMatrix);

    void ChangeScene(std::unique_ptr<IScene> nextScene);

    // SpriteCommonをセットする関数
    void SetSpriteCommon(SpriteCommon *spriteCommon) { spriteCommon_ = spriteCommon; }

    void SetModelCommon(ModelCommon *modelCommon) { modelCommon_ = modelCommon; }
    ParticleCommon *GetParticleCommon() const { return particleCommon_; }
    void SetParticleCommon(ParticleCommon* particleCommon) {
        particleCommon_ = particleCommon;
    }

    // SpriteCommonを取得する関数
    SpriteCommon *GetSpriteCommon() const { return spriteCommon_; }
    ModelCommon *GetModelCommon()const { return modelCommon_; }

private:
    std::unique_ptr<IScene> currentScene_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    SpriteCommon *spriteCommon_ = nullptr;
    ModelCommon *modelCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;

    // 次のシーンを予約しておく変数
    std::unique_ptr<IScene> nextScene_ = nullptr;
};
