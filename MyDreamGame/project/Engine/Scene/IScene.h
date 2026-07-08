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
class Object3D;
class ParticleManager;
class PrimitiveObject;
class GameCamera;
class MapChip2D;

class IScene {
public:
    virtual ~IScene() = default;

    // 初期化
    virtual void Initialize() = 0;

    // 更新
    virtual void Update(SceneManager *sceneManager) = 0;

    // エディター停止中のトランスフォーム行列等の再計算用
    virtual void UpdateEditor() {}

    // 描画
    virtual void Draw(const Matrix4x4 &viewProjectionMatrix) = 0;

    // シーン固有のImGui表示（インスペクター用など）
    virtual void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) {}

    // ヒエラルキー用: オブジェクトリストの取得 (デフォルトは空)
    virtual std::vector<Object3D *> GetObjects() { return {}; }
    virtual std::vector<ParticleManager *> GetParticles() { return {}; }
    virtual std::vector<PrimitiveObject *> GetPrimitives() { return {}; }

    // マップチップの取得 (デフォルトはnullptr)
    virtual MapChip2D* GetMapChip() { return nullptr; }

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

    // GameCameraのセッター（2Dシーンなどでカメラモードを切り替える用）
    virtual void SetGameCamera(GameCamera* gameCamera) {
        gameCamera_ = gameCamera;
    }

protected:
    // 継承先(TitleSceneなど)で使えるようにする
    SpriteCommon *spriteCommon_ = nullptr;
    ModelCommon* modelCommon_ = nullptr;
    ParticleCommon *particleCommon_ = nullptr;
    GameCamera *gameCamera_ = nullptr;
    
};