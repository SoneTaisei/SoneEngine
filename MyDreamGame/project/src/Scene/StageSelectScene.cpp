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

    uint32_t backGroundIndex = TextureManager::GetInstance()->Load("resources/monsterBall.png", commandList_);
    D3D12_GPU_DESCRIPTOR_HANDLE backGroundTH = TextureManager::GetInstance()->GetGpuHandle(backGroundIndex);

    std::unique_ptr<Model> backGroundModel = std::make_unique<Model>();
    backGroundModel->Initialize(modelCommon_, "resources/plane", "plane.obj");
    backGroundModel->SetTextureHandle(backGroundTH);
    backGroundModel->SetTranslation({0.0f, 0.0f, 1.0f});
    backGroundModel->SetScale({5.0f, 5.0f, 5.0f});
    backGroundModel->SetRotation({0.0f, 0.0f, 0.0f});
    //models_.push_back(std::move(backGroundModel));

    std::unique_ptr<Model> skydomeModel = std::make_unique<Model>();
    skydomeModel->Initialize(modelCommon_, "resources/newsphere", "newsphere.obj");
    skydomeModel->SetTextureHandle(skydomeTH);
    skydomeModel->SetRotation({0.0f, 0.0f, 0.0f});
    models_.push_back(std::move(skydomeModel));

    
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

    // スペースキーが押されたらゲームシーンへ
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(new GameScene());
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ここにステージセレクトシーンの描画処理を記述する

    modelCommon_->DrawAll(viewProjectionMatrix);

    for (auto &obj : objects_) {
        // テクスチャ設定が Object3D::Draw 内で正しく行われていれば、これで描画されます
        obj->Draw(commandList_.Get(), nullptr);
    }
}
