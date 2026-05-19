#include "GameScene.h"
#include "Scene/SceneManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

void GameScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList.Get();

    // 1. Device取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // 3. SnowParticleの生成 (unique_ptrで作る)
    auto snowParticle = std::make_unique<SnowParticle>();

    // 4. 初期化
    snowParticle->Initialize(commandList.Get(), particleCommon_, 1000, "Sprite/School/circle.png", srvIndex_, BlendMode::kBlendModeAdd);
    snowParticle->SetName("Snow Particles");

    // Commonに描画登録する (Modelと同じ仕組みにする)
    particleCommon_->AddParticle(snowParticle.get());

    // 5. エミッタ用に生ポインタを保存しておく
    snowParticle_ = snowParticle.get();

    // 6. リストに所有権を移動 (push_back)
    particles_.push_back(std::move(snowParticle));

    // 7. プリミティブオブジェクトの作成
    // 橙色の球体（環境マップ・ライティング有効）
    {
        auto sphere = std::make_unique<PrimitiveObject>();
        sphere->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Sphere, 1.0f, 32));
        sphere->SetTranslation({2.0f, 0.0f, 0.0f});
        sphere->GetMaterial().color = {1.0f, 0.5f, 0.0f, 1.0f};
        sphere->GetMaterial().enableEnvironmentMap = 1;
        sphere->GetMaterial().environmentCoefficient = 0.5f;
        sphere->GetMaterial().lightingType = 1;
        sphere->SetName("Game Sphere");
        primitives_.push_back(std::move(sphere));
    }

    // 水色の箱（環境マップ・ライティング有効）
    {
        auto box = std::make_unique<PrimitiveObject>();
        box->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f));
        box->SetTranslation({-2.0f, 0.0f, 0.0f});
        box->GetMaterial().color = {0.0f, 0.8f, 1.0f, 1.0f};
        box->GetMaterial().enableEnvironmentMap = 1;
        box->GetMaterial().environmentCoefficient = 0.5f;
        box->GetMaterial().lightingType = 1;
        box->SetName("Game Box");
        primitives_.push_back(std::move(box));
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

    // 3. プリミティブオブジェクトの回転と更新
    static float rotateTimer = 0.0f;
    rotateTimer += 1.0f / 60.0f;
    if (primitives_.size() >= 2) {
        primitives_[0]->SetRotation({0.0f, rotateTimer, 0.0f}); // 球体のY軸回転
        primitives_[1]->SetRotation({rotateTimer * 0.5f, rotateTimer, 0.0f}); // 箱の多軸回転
    }

    for (auto &primitive : primitives_) {
        primitive->Update();
    }
}

void GameScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 1. プリミティブの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &primitive : primitives_) {
            primitive->Draw(commandList_);
        }
#ifdef USE_IMGUI
    }
#endif

    // 2. パーティクルの描画
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
    for (auto &p : primitives_) {
        result.push_back(p.get());
    }
    return result;
}