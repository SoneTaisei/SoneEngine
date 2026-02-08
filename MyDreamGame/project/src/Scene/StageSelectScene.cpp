#include "StageSelectScene.h"
#include "../externals/imgui/imgui.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "SceneManager.h"
#include "Utility/ImGuiHelper.h"
#include "Scene/GameScene.h"

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
#ifdef USE_IMGUI
    // --- 既存のモデルデバッグ表示 ---
    ImGui::Begin("Debug Models");
    for (int i = 0; i < models_.size(); ++i) {
        std::string name = "Model " + std::to_string(i);
        ShowModelGui(name, models_[i].get());
    }
    ImGui::End();
#endif //

    // シーン遷移の処理
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(new GameScene());
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // ここにステージセレクトシーンの描画処理を記述する

    modelCommon_->DrawAll(viewProjectionMatrix);
}
