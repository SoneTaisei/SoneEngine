#include "TitleScene.h"
#include "../externals/imgui/imgui.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneManager.h"
#include "Resource/Sprite/SpriteCommon.h"
#include <wrl.h>
#include "Resource/Model/ModelManager.h"
#include "Graphics/CameraManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Scene/SceneFactory.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/Renderer.h"
#include "Component/TransformComponent.h"
#include "Component/MeshRendererComponent.h"
#include "Component/PrimitiveRendererComponent.h"
#include "GameObject/Object3D.h"
#include <cmath>

TitleScene::~TitleScene() {
}

void TitleScene::OnEnter(SceneManager* sceneManager) {
    // シーン遷移時の開始処理
    isFirstFrame_ = true;
    titleTimer_ = 0.0f;
}

void TitleScene::OnExit(SceneManager* sceneManager) {
    // シーン遷移時の終了処理
}

void TitleScene::Initialize() {
    Microsoft::WRL::ComPtr<ID3D12Device> device = DirectXCommon::GetInstance()->GetDevice();
    PrimitiveManager::GetInstance()->Initialize(device.Get());

    // -------------------------------------------------------------
    // 1. カメラ初期設定 (夜空と屋根の怪盗を見上げるシネマティックアングル)
    // -------------------------------------------------------------
    cameraTransform_.translate = { 0.0f, 1.2f, -8.5f };
    cameraTransform_.rotate = { 0.06f, 0.0f, 0.0f };

    // -------------------------------------------------------------
    // 2. Skybox初期化 (以前のqwantani_duskに戻す)
    // -------------------------------------------------------------
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    // -------------------------------------------------------------
    // 3. サーチライト演出 (警察・警備員が夜空を走査する光の筋)
    // -------------------------------------------------------------
    searchlightObjects_.clear();
    Primitive* boxPrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box);
    
    // サーチライト1 (黄色・左奥から右へスイング)
    {
        auto light1 = std::make_shared<GameObject>("Searchlight_Yellow");
        auto lt1 = light1->AddComponent<TransformComponent>();
        lt1->SetPosition({ -4.5f, 6.0f, 4.0f });
        lt1->SetScale({ 0.9f, 22.0f, 0.9f });
        lt1->SetRotation({ 0.0f, 0.0f, 0.35f });

        auto lr1 = light1->AddComponent<PrimitiveRendererComponent>();
        lr1->Initialize(device.Get(), boxPrim);
        lr1->GetMaterial().color = { 1.0f, 0.92f, 0.4f, 0.22f }; // 半透明の光線イエロー
        lr1->GetMaterial().lightingType = 0; // 自己発光

        gameObjects_.push_back(light1);
        searchlightObjects_.push_back(light1);
    }

    // サーチライト2 (シアンブルー・右奥から左へスイング)
    {
        auto light2 = std::make_shared<GameObject>("Searchlight_Cyan");
        auto lt2 = light2->AddComponent<TransformComponent>();
        lt2->SetPosition({ 4.0f, 6.5f, 6.0f });
        lt2->SetScale({ 0.8f, 24.0f, 0.8f });
        lt2->SetRotation({ 0.0f, 0.0f, -0.4f });

        auto lr2 = light2->AddComponent<PrimitiveRendererComponent>();
        lr2->Initialize(device.Get(), boxPrim);
        lr2->GetMaterial().color = { 0.3f, 0.85f, 1.0f, 0.18f }; // 半透明のサイバーシアン
        lr2->GetMaterial().lightingType = 0; // 自己発光

        gameObjects_.push_back(light2);
        searchlightObjects_.push_back(light2);
    }

    // -------------------------------------------------------------
    // 6. タイトルロゴ スプライト (title.png) - 画面中央に配置
    // -------------------------------------------------------------
    titleLogoTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/title.png");
    titleLogoSprite_ = std::make_unique<Sprite>();
    titleLogoSprite_->Initialize(spriteCommon_, titleLogoTextureHandle_);
    
    // 画面解像度 1280x720 の中央上部に配置 (幅600, 高さ266, X=(1280-600)/2=340)
    const float logoW = 600.0f;
    const float logoH = 266.0f;
    titleLogoSprite_->SetSize({ logoW, logoH });
    titleLogoSprite_->SetPosition({ (1280.0f - logoW) * 0.5f, 70.0f });

    // デバッグカメラ初期化
    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(1280, 720);
}

void TitleScene::Update(SceneManager *sceneManager) {
    float dt = TimeManager::GetInstance().GetDeltaTime();
    titleTimer_ += dt;

    // シーン遷移直後の同一フレームでのSPACEキー入力を拾わないようにする
    if (isFirstFrame_) {
        isFirstFrame_ = false;
    } else {
        if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) || 
            KeyboardInput::GetInstance()->IsKeyPressed(DIK_RETURN)) {
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kStageSelect));
            return;
        }
    }

    // -------------------------------------------------------------
    // カメラのシネマティック浮遊アニメーション
    // -------------------------------------------------------------
    Vector3 camPos = {
        sinf(titleTimer_ * 0.4f) * 0.25f,
        1.2f + sinf(titleTimer_ * 0.7f) * 0.12f,
        -8.5f + cosf(titleTimer_ * 0.3f) * 0.15f
    };
    Vector3 camRot = {
        0.06f + sinf(titleTimer_ * 0.5f) * 0.015f,
        sinf(titleTimer_ * 0.3f) * 0.02f,
        0.0f
    };
    cameraTransform_.translate = camPos;
    cameraTransform_.rotate = camRot;

    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    CameraManager::GetInstance()->SetCameraInfo(cameraTransform_.translate, viewMatrix, projectionMatrix);

    if (debugCamera_) {
        debugCamera_->Update();
    }

    // -------------------------------------------------------------
    // タイトルロゴの浮遊アニメーション (フワフワと微小に上下・中央維持)
    // -------------------------------------------------------------
    if (titleLogoSprite_) {
        float floatOffsetY = sinf(titleTimer_ * 1.6f) * 8.0f;
        const float logoW = 600.0f;
        titleLogoSprite_->SetPosition({ (1280.0f - logoW) * 0.5f, 70.0f + floatOffsetY });
        titleLogoSprite_->Update();
    }

    // -------------------------------------------------------------
    // サーチライトの首振り・スイングアニメーション
    // -------------------------------------------------------------
    if (searchlightObjects_.size() >= 2) {
        // サーチライト1 (黄色): 左右にゆっくり首振り
        if (auto tc1 = searchlightObjects_[0]->GetComponent<TransformComponent>()) {
            float rotZ1 = sinf(titleTimer_ * 0.85f) * 0.35f + 0.18f;
            float posX1 = -4.5f + sinf(titleTimer_ * 0.85f) * 0.5f;
            tc1->SetPosition({ posX1, 6.0f, 4.0f });
            tc1->SetRotation({ 0.0f, 0.0f, rotZ1 });
        }
        // サーチライト2 (シアン): 逆位相で首振り
        if (auto tc2 = searchlightObjects_[1]->GetComponent<TransformComponent>()) {
            float rotZ2 = sinf(titleTimer_ * 0.65f + 2.0f) * 0.4f - 0.22f;
            float posX2 = 4.0f + cosf(titleTimer_ * 0.65f + 2.0f) * 0.6f;
            tc2->SetPosition({ posX2, 6.5f, 6.0f });
            tc2->SetRotation({ 0.0f, 0.0f, rotZ2 });
        }
    }

    // 全オブジェクトの更新
    for (auto &object : gameObjects_) {
        object->Update();
    }

    for (auto &sprite : sprites_) {
        sprite->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 1. モデル描画の前準備
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    // 2. Skyboxを描画
    if (skybox_) {
        skybox_->Draw();
        
        // Skybox描画後はPSOが切り替わるため、再度モデル用の設定を呼び出す
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }

    // 3. 3Dモデル・GameObjectの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &object : gameObjects_) {
            object->Draw();
        }
#ifdef USE_IMGUI
    }
#endif

    // コンポーネントの描画を実行
    Renderer::GetInstance()->RenderComponents();

    // 4. パーティクルの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects()) {
#endif
        if (particleCommon_) {
            particleCommon_->PreDraw();
            particleCommon_->DrawAll(viewProjectionMatrix);
        }
#ifdef USE_IMGUI
    }
#endif

    // 5. 2Dスプライト（タイトルロゴ）の描画
    if (spriteCommon_) {
        spriteCommon_->PreDraw();
        if (titleLogoSprite_) {
            titleLogoSprite_->Draw();
        }
        spriteCommon_->DrawAll();
    }
}

std::vector<Object3D *> TitleScene::GetObjects() {
    return {};
}

std::vector<ParticleManager *> TitleScene::GetParticles() {
    std::vector<ParticleManager *> result;
    for (auto &p : particles_) {
        result.push_back(p.get());
    }
    return result;
}

std::vector<PrimitiveObject *> TitleScene::GetPrimitives() {
    return {};
}

void TitleScene::UpdateEditor() {
    for (auto &object : gameObjects_) {
        object->Update();
    }
    if (titleLogoSprite_) {
        titleLogoSprite_->Update();
    }
    for (auto &sprite : sprites_) {
        sprite->Update();
    }
    if (skybox_) {
        skybox_->Update();
    }
}

void TitleScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    // 新しいプロンプトUI用（後ほど差し替え）
#endif
}