#include "GameScene.h"
#include "Scene/SceneManager.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/GameCamera.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

void GameScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList.Get();

    // Device取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // PrimitiveManagerの初期化（まだの場合）
    PrimitiveManager::GetInstance()->Initialize(device.Get());

    // マップの生成と初期化
    map_ = std::make_unique<MapChip2D>();
    map_->Initialize(commandList.Get());

    // プレイヤーの生成と初期化
    player_ = std::make_unique<Player2D>();
    player_->Initialize(commandList.Get());

    // GameCameraを正射影モード（2D表示）に切り替え
    if (gameCamera_) {
        gameCamera_->InitializeOrthographic(1280, 720, 20.0f, 11.25f);
        // プレイヤーの位置をカメラ追従ターゲットに設定
        gameCamera_->SetFollowTarget(&player_->GetPosition());
    }
}

void GameScene::Update(SceneManager *sceneManager) {
    // プレイヤーの更新（入力・物理・当たり判定）
    if (player_ && map_) {
        player_->Update(*map_);
    }

    // マップの更新
    if (map_) {
        map_->Update();
    }
}

void GameScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    if (player_ && player_->GetPrimitiveObject() == selectedPrimitive) {
        player_->DisplayImGui();
    }
#endif
}

void GameScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ModelCommonの描画前処理
    modelCommon_->PreDraw(commandList_);

    // マップの描画
    if (map_) {
        map_->Draw(commandList_);
    }

    // プレイヤーの描画
    if (player_) {
        player_->Draw(commandList_);
    }
}

std::vector<PrimitiveObject *> GameScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;

    // プレイヤー
    if (player_) {
        result.push_back(player_->GetPrimitiveObject());
    }

    // マップチップ
    if (map_) {
        auto mapPrims = map_->GetPrimitiveObjects();
        result.insert(result.end(), mapPrims.begin(), mapPrims.end());
    }

    return result;
}