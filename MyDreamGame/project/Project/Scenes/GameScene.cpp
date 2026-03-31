#include "GameScene.h"
#include "Scene/SceneManager.h"
#include "TitleScene.h" // ← これを追加
#include "Input/KeyboardInput.h"
#include "../externals/imgui/imgui.h"

void GameScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList.Get();

    // 1. Device取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // 3. SnowParticleの生成 (unique_ptrで作る)
    auto snowParticle = std::make_unique<SnowParticle>();

    // 4. 初期化
    snowParticle->Initialize(commandList.Get(), particleCommon_, 1000, "Sprite/circle.png", srvIndex_, BlendMode::kBlendModeAdd);

    // Commonに描画登録する (Modelと同じ仕組みにする)
    particleCommon_->AddParticle(snowParticle.get());

    // 5. エミッタ用に生ポインタを保存しておく
    snowParticle_ = snowParticle.get();

    // 6. リストに所有権を移動 (push_back)
    particles_.push_back(std::move(snowParticle));
}

void GameScene::Update(SceneManager *sceneManager) {
// 1. 雪を発生させる (個別のポインタを使う)
    if(snowParticle_) {
        snowParticle_->Emit(snowEmitter_);
    }

    // 2. 全パーティクルを更新する (リストを使って一括更新)
    // これにより、パーティクルの種類が増えてもループ一つで済みます
    for(auto &particle : particles_) {
        particle->Update();
    }

    // スペースキーが押されたらタイトルシーンへ戻る
    if(KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(std::make_unique<TitleScene>());
    }
}

void GameScene::Draw(const Matrix4x4 &viewProjectionMatrix) {

   // Draw
    // 描画前処理 (ParticleCommonのPreDrawが必要なら呼ぶ)
    particleCommon_->PreDraw(commandList_); // ※commandListを保持している場合

    // 雪の描画
    particleCommon_->DrawAll(viewProjectionMatrix);
}