#include "StageSelectScene.h"
#include "../externals/imgui/imgui.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "SceneManager.h"
#include "Utility/ImGuiHelper.h"

StageSelectScene::~StageSelectScene() {
}

void StageSelectScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {

    commandList_ = commandList;

    // Deviceの取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    uint32_t skydomeIndex = TextureManager::GetInstance()->Load("resources/monsterBall.png", commandList_);
    D3D12_GPU_DESCRIPTOR_HANDLE skydomeTH = TextureManager::GetInstance()->GetGpuHandle(skydomeIndex);

    std::unique_ptr<Model> skydomeModel = std::make_unique<Model>();
    skydomeModel->Initialize(modelCommon_, "resources/sphere", "sphere.obj");
    skydomeModel->SetTextureHandle(skydomeTH);
    skydomeModel->SetRotation({0.0f, 0.0f, 0.0f});
    models_.push_back(std::move(skydomeModel));

    uint32_t terrainIndex = TextureManager::GetInstance()->Load("resources/terrain/grass.png", commandList_);
    D3D12_GPU_DESCRIPTOR_HANDLE terrainTH = TextureManager::GetInstance()->GetGpuHandle(terrainIndex);

    std::unique_ptr<Model> terrainModel = std::make_unique<Model>();
    terrainModel->Initialize(modelCommon_, "resources/terrain", "terrain.obj");
    terrainModel->SetTextureHandle(terrainTH);
    terrainModel->SetRotation({0.0f, 0.0f, 0.0f});
    models_.push_back(std::move(terrainModel));

    // ■ 追加: カメラの初期位置設定
    cameraTransform_.scale = {1.0f, 1.0f, 1.0f};
    cameraTransform_.rotate = {1.0f, 0.0f, 0.0f};
    cameraTransform_.translate = {0.0f, 0.0f, -50.0f}; // 少し手前に引く
}

void StageSelectScene::Update(SceneManager *sceneManager) {

    ImGui::Begin("Debug Models");

    for (int i = 0; i < models_.size(); ++i) {
        // 名前を自動生成（Model 0, Model 1...）
        std::string name = "Model " + std::to_string(i);

        // 便利関数にポイっと渡すだけ！
        ShowModelGui(name, models_[i].get());
    }

    ImGui::End();

    ImGui::Begin("Title Scene Camera");

    // 回転 (Radian表記だと分かりにくい場合もありますが、通常はそのまま扱います)
    ImGui::DragFloat3("Camera Rotate", &cameraTransform_.rotate.x, 0.01f);

    // 位置
    ImGui::DragFloat3("Camera Translate", &cameraTransform_.translate.x, 0.1f);

    ImGui::End();

    // 1. カメラのワールド行列を作る
    Matrix4x4 cameraMatrix = TransformFunctions::MakeAffineMatrix(
        cameraTransform_.scale,
        cameraTransform_.rotate,
        cameraTransform_.translate);

    // ビルボード計算用に、カメラ行列をParticleCommonに渡す
    if (modelCommon_) {
        modelCommon_->SetCamera(cameraMatrix);
    }

    // 2. ビュー行列 (カメラの逆行列)
    Matrix4x4 viewMatrix = TransformFunctions::Inverse(cameraMatrix);

    // 3. プロジェクション行列 (画角0.45, アスペクト比16:9, 範囲0.1~100)
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(
        0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);

    // 4. 合成して描画用行列を作る
    viewProjection_ = TransformFunctions::Multiply(viewMatrix, projectionMatrix);

    // スペースキーが押されたらゲームシーンへ
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(new GameScene());
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ここにステージセレクトシーンの描画処理を記述する

    modelCommon_->DrawAll(viewProjectionMatrix);
}
