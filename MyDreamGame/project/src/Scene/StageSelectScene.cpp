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

    // ModelCommonからポイントライトのポインタをもらって直接書き換える
    PointLight *light = modelCommon_->GetPointLight();

    ImGui::Begin("Global Light Settings");
    ImGui::DragFloat3("PointLight Pos", &light->position.x, 0.1f);
    ImGui::DragFloat("Radius", &light->radius, 0.1f); // これが逆二乗則に効く！
    ImGui::DragFloat("Decay", &light->decay, 0.1f);
    ImGui::End();

    // スペースキーが押されたらゲームシーンへ
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(new GameScene());
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ここにステージセレクトシーンの描画処理を記述する

    modelCommon_->DrawAll(viewProjectionMatrix);
}
