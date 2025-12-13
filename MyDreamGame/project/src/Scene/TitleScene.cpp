#include "TitleScene.h"
#include "SceneManager.h"
#include "Input/KeyboardInput.h"
#include "../externals/imgui/imgui.h"
#include "Sprite/SpriteCommon.h"
#include "Model/ModelCommon.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"
#include "StageSelectScene.h"
#include <wrl.h>

TitleScene::~TitleScene() {
}

void TitleScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList;

    // Deviceの取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // -------------------------------------------------
    // ■ パーティクルシステムの初期化 (GameSceneと同じ方式)
    // -------------------------------------------------

    // 1. ParticleCommonの生成と初期化
    particleCommon_ = std::make_unique<ParticleCommon>();
    particleCommon_->Initialize(device.Get());

    // 2. windowParticleの生成
    auto windowP = std::make_unique<windowParticle>();

    // 3. windowParticleの初期化
    // ※テクスチャパスは実際に使いたい画像に合わせてください
    // ※BlendModeは用途に合わせて kBlendModeAdd や kBlendModeNormal などを指定
    windowP->Initialize(commandList.Get(), particleCommon_.get(), 1000, "resources/circle.png", srvIndex_);

    // 4. Commonに描画登録
    particleCommon_->AddParticle(windowP.get());

    // 5. 操作用に生ポインタを保存
    windowParticle_ = windowP.get();

    // 6. リストに所有権を移動
    particles_.push_back(std::move(windowP));

    // 7. エミッタの設定
    windowEmitter_.count = 1;         // 1回に出る数
    windowEmitter_.frequency = 0.5f;  // 発生頻度(秒)
    windowEmitter_.transform.translate = { 0.0f, 0.0f, 0.0f }; // 発生位置

    // ■ 追加: カメラの初期位置設定
    cameraTransform_.scale = { 1.0f, 1.0f, 1.0f };
    cameraTransform_.rotate = { 0.0f, 0.0f, 0.0f };
    cameraTransform_.translate = { 0.0f, 0.0f, -10.0f }; // 少し手前に引く
}

void TitleScene::Update(SceneManager *sceneManager) {
    // シーン遷移処理
    if(KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(new StageSelectScene());
    }

    // -------------------------------------------------
    // ■ カメラのImGui操作
    // -------------------------------------------------
    ImGui::Begin("Title Scene Camera");

    // 回転 (Radian表記だと分かりにくい場合もありますが、通常はそのまま扱います)
    ImGui::DragFloat3("Camera Rotate", &cameraTransform_.rotate.x, 0.01f);

    // 位置
    ImGui::DragFloat3("Camera Translate", &cameraTransform_.translate.x, 0.1f);

    ImGui::End();


    // -------------------------------------------------
    // ■ 行列の更新 (数値が変わったら再計算)
    // -------------------------------------------------
    // 1. カメラのワールド行列を作る
    Matrix4x4 cameraMatrix = TransformFunctions::MakeAffineMatrix(
        cameraTransform_.scale,
        cameraTransform_.rotate,
        cameraTransform_.translate
    );

    // ビルボード計算用に、カメラ行列をParticleCommonに渡す
    if(particleCommon_) {
        particleCommon_->SetCamera(cameraMatrix);
    }

    // 2. ビュー行列 (カメラの逆行列)
    Matrix4x4 viewMatrix = TransformFunctions::Inverse(cameraMatrix);

    // 3. プロジェクション行列 (画角0.45, アスペクト比16:9, 範囲0.1~100)
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(
        0.45f, 1280.0f / 720.0f, 0.1f, 100.0f
    );

    // 4. 合成して描画用行列を作る
    viewProjection_ = TransformFunctions::Multiply(viewMatrix, projectionMatrix);

    // -------------------------------------------------
    // ■ パーティクルの更新
    // -------------------------------------------------
    
    // 全パーティクルの更新
    for(auto &particle : particles_) {
        particle->Update();

        particle->DrawImGui();
    }

}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 既存の描画
    // textureHandle_ = TextureManager::GetInstance()->Load("resources/uvChecker.png",commandList_); ...
    modelCommon_->DrawAll(viewProjectionMatrix);

    // -------------------------------------------------
    // ■ パーティクルの描画
    // -------------------------------------------------
    if(particleCommon_) {
        // 前処理
        particleCommon_->PreDraw(commandList_.Get());

        // 一括描画 (viewProjectionMatrixを渡す)
        particleCommon_->DrawAll(viewProjection_);
    }

    if(spriteCommon_) {
        spriteCommon_->PreDraw(commandList_.Get());
        spriteCommon_->DrawAll();
    }
}