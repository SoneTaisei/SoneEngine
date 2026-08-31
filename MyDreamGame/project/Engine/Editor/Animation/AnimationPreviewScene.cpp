#include "AnimationPreviewScene.h"
#ifdef USE_IMGUI
#include "Graphics/TextureManager.h"
#include "Scene/SceneManager.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Component/TransformComponent.h"
#include "Renderer/Renderer.h"
#include "Core/Utility/Animation.h"

void AnimationPreviewScene::Initialize() {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

    // 1. スカイボックスの生成（明るさを抑えてBlender風ダークグレー背景にする）
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device, skyboxTextureHandle_);
    skybox_->SetColor(Vector4{ 0.08f, 0.08f, 0.10f, 1.0f });

    // 2. 床グリッドオブジェクト（グレーの基準床）の生成
    Primitive* boxPrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box);
    if (boxPrim) {
        gridFloorObj_ = std::make_unique<PrimitiveObject>();
        gridFloorObj_->Initialize(device, boxPrim);
        gridFloorObj_->SetName("GridFloor");
        gridFloorObj_->SetTranslation({ 0.0f, -0.05f, 0.0f });
        gridFloorObj_->SetScale({ 40.0f, 0.1f, 40.0f });
        gridFloorObj_->SetIsDoubleSided(true);
        gridFloorObj_->GetMaterial().lightingType = 0; // 均一ライティングでどの角度からも一定の明るさを維持
        gridFloorObj_->GetMaterial().color = { 0.18f, 0.18f, 0.20f, 1.0f };
        gridFloorObj_->GetMaterial().enableBoxMapping = 2.0f; // プロシージャル3D床グリッドを有効化
        gridFloorObj_->Update();
    }

    // 3. アニメーション編集対象モデル（プレイヤー / Gaikotu）の生成
    Model* playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/gaikotu", "scene.gltf");
    if (playerModel) {
        playerObject_ = std::make_shared<GameObject>("Player");
        auto playerTransform = playerObject_->AddComponent<TransformComponent>();
        playerTransform->SetPosition({ 0.0f, 0.0f, 0.0f });
        playerTransform->SetScale({ 1.5f, 1.5f, 1.5f });
        playerTransform->SetRotation({ 0.0f, 3.14159265f, 0.0f });

        uint32_t playerTexIndex = TextureManager::GetInstance()->Load("resources/Object/Original/gaikotu/textures/mini_simple_material_primary_baseColor.png");
        D3D12_GPU_DESCRIPTOR_HANDLE playerTH = TextureManager::GetInstance()->GetGpuHandle(playerTexIndex);

        auto playerRenderer = playerObject_->AddComponent<MeshRendererComponent>();
        playerRenderer->Initialize(device, playerModel);
        playerRenderer->SetTextureHandle(playerTH);
        playerModel->SetTextureHandle(playerTH);
        playerRenderer->GetMaterial().color = { 0.85f, 0.85f, 0.88f, 1.0f };
        playerRenderer->GetMaterial().lightingType = 1;
        playerRenderer->GetMaterial().shininess = 40.0f;

        auto animator = playerObject_->AddComponent<AnimatorComponent>();
        animator->Initialize();
        animator->SetModelData(playerModel->GetModelData());

        // 初期状態は自然なTポーズ（オーバーライドなし）で待機
        animator->ClearJointOverrides();

        gameObjects_.push_back(playerObject_);
    }

    // 4. スタジオライティングを自然なスタジオ照明（キーライト・フィルライト）に設定
    if (auto* mc = ModelManager::GetInstance()->GetModelCommon()) {
        if (auto* d = mc->GetDirectionalLight()) {
            d->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            d->direction = TransformFunctions::Normalize({ -0.35f, -0.55f, 0.70f });
            d->intensity = 0.70f;
            d->enableFlatShading = 0;
        }
        if (auto* p = mc->GetPointLight()) {
            p->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            p->position = { -1.5f, 2.5f, 2.0f };
            p->radius = 15.0f;
            p->intensity = 0.25f;
            p->decay = 1.0f;
        }
        if (auto* s = mc->GetSpotLight()) {
            s->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            s->position = { 0.0f, 4.0f, 3.5f };
            s->direction = TransformFunctions::Normalize({ 0.0f, -3.0f, -3.5f });
            s->distance = 12.0f;
            s->intensity = 0.0f;
        }
    }
}

void AnimationPreviewScene::OnEnter(SceneManager *sceneManager) {
    if (gridFloorObj_) gridFloorObj_->Update();
    if (playerObject_) playerObject_->Update();

    // シーン進入時にもライティングを自然なスタジオ照明にセット
    if (auto* mc = ModelManager::GetInstance()->GetModelCommon()) {
        if (auto* d = mc->GetDirectionalLight()) {
            d->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            d->direction = TransformFunctions::Normalize({ -0.35f, -0.55f, 0.70f });
            d->intensity = 0.70f;
            d->enableFlatShading = 0;
        }
        if (auto* p = mc->GetPointLight()) {
            p->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            p->position = { -1.5f, 2.5f, 2.0f };
            p->radius = 15.0f;
            p->intensity = 0.25f;
            p->decay = 1.0f;
        }
        if (auto* s = mc->GetSpotLight()) {
            s->intensity = 0.0f;
        }
    }
}

void AnimationPreviewScene::OnExit(SceneManager *sceneManager) {
}

void AnimationPreviewScene::Update(SceneManager *sceneManager) {
    if (skybox_) {
        skybox_->Update();
    }
    if (gridFloorObj_) {
        gridFloorObj_->Update();
    }
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void AnimationPreviewScene::UpdateEditor() {
    if (skybox_) {
        skybox_->Update();
    }
    if (gridFloorObj_) {
        gridFloorObj_->Update();
    }
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void AnimationPreviewScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    if (skybox_) {
        skybox_->Draw();
        
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }

    if (gridFloorObj_) {
        gridFloorObj_->Draw();
    }

    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Draw();
        }
    }

    Renderer::GetInstance()->RenderComponents();
}

void AnimationPreviewScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
}

std::vector<Object3D *> AnimationPreviewScene::GetObjects() {
    return {};
}

std::vector<PrimitiveObject *> AnimationPreviewScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;
    if (gridFloorObj_) {
        result.push_back(gridFloorObj_.get());
    }
    return result;
}
#endif
