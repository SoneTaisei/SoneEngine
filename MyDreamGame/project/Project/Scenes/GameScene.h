#pragma once
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"  // これが必要
#include "Effect/ParticleManager.h" // これが必要
#include "Effect/SnowParticle.h"
#include "Scene/IScene.h"
#include "Core/Utility/TransformFunctions.h" // 行列計算用
#include <d3d12.h>

class GameScene : public IScene {
public:
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;

private:
    // パーティクル管理クラス
    SnowParticle *snowParticle_ = nullptr;

    std::vector<std::unique_ptr<ParticleManager>> particles_;

    // エミッタ（発生装置）
    Emitter snowEmitter_;

    // SRVのインデックス (定数またはメンバ変数として管理)
    const int srvIndex_ = 10; // テクスチャ等と被らない場所を指定

    // カメラ用行列（Updateで必要なためメンバに追加）
    Matrix4x4 viewProjection_ = TransformFunctions::MakeIdentity4x4();
    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();

    // コマンドリストを覚えておくための変数
    ID3D12GraphicsCommandList *commandList_ = nullptr;
};