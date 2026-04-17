#pragma once
#include "Scene/IScene.h"
#include <d3d12.h>
#include "Resource/Model/Model.h"
#include "Resource/Sprite/Sprite.h"
#include "Core/Utility/Utilityfunctions.h"
#include "Effect/ParticleManager.h"
#include <memory>
#include "Effect/ParticleCommon.h"
#include "Effect/windowParticle.h"
#include "GameObject/Object3D.h"
#include "Graphics/Skybox.h"

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
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_{};

    Model *playerModel_ = nullptr;

    std::vector<std::unique_ptr<Object3D>> objects_{};
    std::vector<std::unique_ptr<Sprite>> sprites_{};

    // ■ 追加: パーティクル管理用変数

    // 1. 共通基盤
    std::unique_ptr<ParticleCommon> particleCommon_{};

    // 2. パーティクルリスト (所有権管理用)
    std::vector<std::unique_ptr<ParticleManager>> particles_{};

    // 3. 個別のパーティクル操作用ポインタ (Emit呼び出し用)
    windowParticle *windowParticle_ = nullptr;

    // 4. エミッタ (発生設定)
    Emitter windowEmitter_{};

    // 5. SRVインデックス (他と被らない番号)
    const int srvIndex_ = 20;

    // ■ 追加: タイトルシーン専用カメラ
    Transform cameraTransform_{}; // カメラの座標・回転
    Matrix4x4 viewProjection_{};  // 描画に使う行列

    std::unique_ptr<Skybox> skybox_; // Skyboxのインスタンス
    uint32_t skyboxTextureHandle_ = 0;

};
