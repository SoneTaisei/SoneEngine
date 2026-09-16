#include "GPUParticlePreviewScene.h"
#ifdef USE_IMGUI
#include "Graphics/TextureManager.h"
#include "Scene/SceneManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Renderer/Renderer.h"
#include "Effect/GPUParticle/GPUParticleSystem.h"

void GPUParticlePreviewScene::Initialize() {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

    // 1. スカイボックス（落ち着いたダークグレー背景）
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device, skyboxTextureHandle_);
    skybox_->SetColor(Vector4{ 0.05f, 0.05f, 0.06f, 1.0f });

    // 2. 床グリッドオブジェクトは非表示（不要なため生成しない）
    gridFloorObj_ = nullptr;
}

#include "Graphics/CameraManager.h"
#include "Resource/Model/ModelManager.h"

void GPUParticlePreviewScene::OnEnter(SceneManager* sceneManager) {
    sceneManager_ = sceneManager;
}

void GPUParticlePreviewScene::OnExit(SceneManager* /*sceneManager*/) {
}

void GPUParticlePreviewScene::Update(SceneManager* /*sceneManager*/) {
    if (gridFloorObj_) {
        gridFloorObj_->Update();
    }
}

void GPUParticlePreviewScene::UpdateEditor() {
    if (gridFloorObj_) {
        gridFloorObj_->Update();
    }
}

#include "Core/Utility/TransformFunctions.h"

void GPUParticlePreviewScene::Draw(const Matrix4x4& viewProjectionMatrix) {
    if (skybox_) {
        skybox_->Draw();
    }
    if (gridFloorObj_) {
        gridFloorObj_->Draw();
    }
    if (particleSystem_) {
        ParticleCommon* particleCommon = sceneManager_ ? sceneManager_->GetParticleCommon() : nullptr;
        if (particleCommon) {
            particleCommon->PreDraw();
            auto commandList = DirectXCommon::GetInstance()->GetCommandList();
            Matrix4x4 cameraMatrix = TransformFunctions::Inverse(CameraManager::GetInstance()->GetViewMatrix());
            ModelManager* modelManager = ModelManager::GetInstance();
            particleSystem_->Draw(commandList, viewProjectionMatrix, cameraMatrix, particleCommon, modelManager);
        }
    }
}

void GPUParticlePreviewScene::DisplayImGui(PrimitiveObject* /*selectedPrimitive*/) {
}

std::vector<PrimitiveObject*> GPUParticlePreviewScene::GetPrimitives() {
    std::vector<PrimitiveObject*> prims;
    if (gridFloorObj_) prims.push_back(gridFloorObj_.get());
    return prims;
}
#endif
