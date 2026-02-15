#include "TitleScene.h"
#include "../externals/imgui/imgui.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Model/ModelCommon.h"
#include "SceneManager.h"
#include "Sprite/SpriteCommon.h"
#include "StageSelectScene.h"
#include <wrl.h>
#include "Utility/ImGuiHelper.h"

TitleScene::~TitleScene() {
}

void TitleScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList;

    // Deviceの取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

     uint32_t planeIndex = TextureManager::GetInstance()->Load("uvChecker.png", commandList_);
    D3D12_GPU_DESCRIPTOR_HANDLE planeTH = TextureManager::GetInstance()->GetGpuHandle(planeIndex);

     std::unique_ptr<Model> planeModel = std::make_unique<Model>();
    planeModel->Initialize(modelCommon_, "plane", "plane.gltf");
    planeModel->SetTextureHandle(planeTH);
    planeModel->SetRotation({0.0f, 0.0f, 0.0f});
    models_.push_back(std::move(planeModel));

}

void TitleScene::Update(SceneManager *sceneManager) {
    // シーン遷移処理
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
        sceneManager->ChangeScene(std::make_unique<StageSelectScene>());
    }

#ifdef USE_IMGUI
    // --- 既存のモデルデバッグ表示 ---
    ImGui::Begin("Debug Models");
    for (int i = 0; i < models_.size(); ++i) {
        std::string name = "Model " + std::to_string(i);
        ShowModelGui(name, models_[i].get());
    }
    ImGui::End();
    
#endif // USE_IMGUI

   
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 既存の描画
    modelCommon_->DrawAll(viewProjectionMatrix);

    // -------------------------------------------------
    // ■ パーティクルの描画
    // -------------------------------------------------
    if (particleCommon_) {
        // 前処理
        particleCommon_->PreDraw(commandList_.Get());

        // 一括描画 (viewProjectionMatrixを渡す)
        particleCommon_->DrawAll(viewProjection_);
    }

    if (spriteCommon_) {
        spriteCommon_->PreDraw(commandList_.Get());
        spriteCommon_->DrawAll();
    }
}