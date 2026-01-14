#include "StageSelectScene.h"
#include "../externals/imgui/imgui.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "SceneManager.h"

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
}

void StageSelectScene::Update(SceneManager *sceneManager) {

    ImGui::Text("a");

    // スペースキーが押されたらゲームシーンへ
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(new GameScene());
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ここにステージセレクトシーンの描画処理を記述する

    modelCommon_->DrawAll(viewProjectionMatrix);
}
