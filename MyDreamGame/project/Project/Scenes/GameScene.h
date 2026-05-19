#pragma once
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/ParticleCommon.h"  // これが必要
#include "Effect/ParticleManager.h" // これが必要
#include "Scene/IScene.h"
#include "Core/Utility/TransformFunctions.h" // 行列計算用
#include <d3d12.h>
#include <memory>

// 2Dゲーム用クラス
#include "Game2D/Player2D.h"
#include "Game2D/MapChip2D.h"

class GameCamera;

#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"

class GameScene : public IScene {
public:
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override { return {}; }
    std::vector<ParticleManager *> GetParticles() override;
    std::vector<PrimitiveObject *> GetPrimitives() override;

private:
    // ---------------------------------------------------
    // 3D・パーティクル関連 (develop)
    // ---------------------------------------------------
    // パーティクル管理クラス
    SnowParticle *snowParticle_ = nullptr;
    std::vector<std::unique_ptr<ParticleManager>> particles_;
    std::vector<std::unique_ptr<PrimitiveObject>> primitives_;

    // エミッタ（発生装置）
    Emitter snowEmitter_;

    // SRVのインデックス (定数またはメンバ変数として管理)
    const int srvIndex_ = 10; // テクスチャ等と被らない場所を指定

    // カメラ用行列（Updateで必要なためメンバに追加）
    Matrix4x4 viewProjection_ = TransformFunctions::MakeIdentity4x4();
    Matrix4x4 cameraMatrix_ = TransformFunctions::MakeIdentity4x4();

    // ---------------------------------------------------
    // 2Dゲーム用オブジェクト (Game_develop)
    // ---------------------------------------------------
    std::unique_ptr<Player2D> player_;
    std::unique_ptr<MapChip2D> map_;

    // ---------------------------------------------------
    // 共通システム
    // ---------------------------------------------------
    // コマンドリストを覚えておくための変数
    ID3D12GraphicsCommandList *commandList_ = nullptr;
};