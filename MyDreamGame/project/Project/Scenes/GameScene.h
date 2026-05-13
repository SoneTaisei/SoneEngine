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

class GameScene : public IScene {
public:
    void Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) override;
    void Update(SceneManager *sceneManager) override;
    void Draw(const Matrix4x4 &viewProjectionMatrix) override;
    void DisplayImGui(PrimitiveObject* selectedPrimitive = nullptr) override;

    // ヒエラルキー用
    std::vector<Object3D *> GetObjects() override { return {}; }
    std::vector<PrimitiveObject *> GetPrimitives() override;

private:
    // 2Dゲーム用オブジェクト
    std::unique_ptr<Player2D> player_;
    std::unique_ptr<MapChip2D> map_;

    // コマンドリストを覚えておくための変数
    ID3D12GraphicsCommandList *commandList_ = nullptr;
};