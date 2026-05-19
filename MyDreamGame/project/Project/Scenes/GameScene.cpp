#include "GameScene.h"
#include "Scene/SceneManager.h"
#include "Effect/SnowParticle.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/GameCamera.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

void GameScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList.Get();

    // 1. Device取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // 5. マップの生成と初期化
    map_ = std::make_unique<MapChip2D>();
    map_->Initialize(commandList.Get());

    // 6. プレイヤーの生成と初期化
    player_ = std::make_unique<Player2D>();
    player_->Initialize(commandList.Get());

    // 7. GameCameraを正射影モード（2D表示）に切り替え
    if (gameCamera_) {
        gameCamera_->InitializeOrthographic(1280, 720, 20.0f, 11.25f);
        // プレイヤーの位置をカメラ追従ターゲットに設定
        gameCamera_->SetFollowTarget(&player_->GetPosition());
    }
}

void GameScene::Update(SceneManager *sceneManager) {
    // 1. 雪を発生させる (個別のポインタを使う)
    if(snowParticle_) {
        snowParticle_->Emit(snowEmitter_);
    }

    // 2. 全パーティクルを更新する (リストを使って一括更新)
    for(auto &particle : particles_) {
        particle->Update();
    }

    // 3. 3Dプリミティブオブジェクトの回転と更新
    static float rotateTimer = 0.0f;
    rotateTimer += 1.0f / 60.0f;
    if (primitives_.size() >= 2) {
        primitives_[0]->SetRotation({0.0f, rotateTimer, 0.0f}); // 球体のY軸回転
        primitives_[1]->SetRotation({rotateTimer * 0.5f, rotateTimer, 0.0f}); // 箱の多軸回転
    }

    for (auto &primitive : primitives_) {
        primitive->Update();
    }

    // 4. プレイヤーの更新（入力・物理・当たり判定）
    if (player_ && map_) {
        player_->Update(*map_);
    }

    // 5. マップの更新
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
    // 1. 3Dプリミティブの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &primitive : primitives_) {
            primitive->Draw(commandList_);
        }
#ifdef USE_IMGUI
    }
#endif

    // 2. 2Dオブジェクト（マップ・プレイヤー）の描画
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

    // 3. パーティクルの描画
    // 描画前処理
    particleCommon_->PreDraw(commandList_);

    // 雪の描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects()) {
#endif
        particleCommon_->DrawAll(viewProjectionMatrix);
#ifdef USE_IMGUI
    }
#endif
}

std::vector<ParticleManager *> GameScene::GetParticles() {
    std::vector<ParticleManager *> result;
    for (auto &p : particles_) {
        result.push_back(p.get());
    }
    return result;
}

std::vector<PrimitiveObject *> GameScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;

    // 1. 3Dプリミティブオブジェクト
    for (auto &p : primitives_) {
        result.push_back(p.get());
    }

    // 2. プレイヤー
    if (player_) {
        result.push_back(player_->GetPrimitiveObject());
    }

    // 3. マップチップ
    if (map_) {
        auto mapPrims = map_->GetPrimitiveObjects();
        result.insert(result.end(), mapPrims.begin(), mapPrims.end());
    }

    return result;
}