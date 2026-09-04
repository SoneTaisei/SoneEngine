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
#include "Graphics/GameCamera.h"
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
    phase_ = Phase::kTitle;
    titleLogoAlpha_ = 1.0f;
    searchlightAlpha_ = 1.0f;
    cameraTransform_.translate = { 0.0f, 1.2f, -8.5f };
    cameraTransform_.rotate = { 0.06f, 0.0f, 0.0f };
    if (titleLogoSprite_) {
        titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
    if (searchlightObjects_.size() >= 2) {
        if (auto prc1 = searchlightObjects_[0]->GetComponent<PrimitiveRendererComponent>()) {
            prc1->GetMaterial().color.w = 0.22f;
        }
        if (auto prc2 = searchlightObjects_[1]->GetComponent<PrimitiveRendererComponent>()) {
            prc2->GetMaterial().color.w = 0.18f;
        }
    }
}

void TitleScene::OnExit(SceneManager* sceneManager) {
    // シーン遷移時の終了処理
}

void TitleScene::Initialize() {

    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};



    // 1. マネージャからモデル（素材）を取得（なければロードされる）
    Model *planeModel = ModelManager::GetInstance()->GetModel("resources/Object/School/plane", "plane.gltf");

    // 2. GameObject（実体）を生成
    auto planeObject = std::make_shared<GameObject>("Ground Plane");
    auto transform = planeObject->AddComponent<TransformComponent>();
    transform->SetRotation({0.0f, 0.0f, 0.0f});

    // 3. 描画コンポーネントのアタッチとテクスチャの設定
    auto planeRenderer = planeObject->AddComponent<MeshRendererComponent>();
    planeRenderer->Initialize(device.Get(), planeModel);
    uint32_t planeIndex = TextureManager::GetInstance()->Load("resources/Sprite/School/uvChecker.png");
    D3D12_GPU_DESCRIPTOR_HANDLE planeTH = TextureManager::GetInstance()->GetGpuHandle(planeIndex);
    planeRenderer->SetTextureHandle(planeTH);
    planeModel->SetTextureHandle(planeTH);

    gameObjects_.push_back(planeObject);

    // ② Spriteのインスタンスを生成
    auto sprite = std::make_unique<Sprite>();

    // ③ 初期化 (spriteCommon_はIScene等で定義されている前提)
    sprite->Initialize(spriteCommon_, planeIndex);

    // ④ 位置やサイズなどのパラメータを設定
    // 画面中央に中心が来るよう配置（1280x720の中央 = (640, 360)、サイズ 200x200）
    sprite->SetPosition({640.0f - 100.0f, 360.0f - 100.0f});
    sprite->SetSize({200.0f, 200.0f});


    // -------------------------------------------------------------
    // 1. カメラ初期設定 (夜空を見上げるシネマティックアングル)
    // -------------------------------------------------------------
    cameraTransform_.translate = { 0.0f, 1.2f, -8.5f };
    cameraTransform_.rotate = { 0.06f, 0.0f, 0.0f };

    // -------------------------------------------------------------
    // 2. Skybox初期化 (qwantani_dusk)
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
}

void TitleScene::Update(SceneManager *sceneManager) {
    float dt = TimeManager::GetInstance().GetDeltaTime();
    titleTimer_ += dt;

    // シーン遷移直後の同一フレームでの入力誤爆防止
    if (isFirstFrame_) {
        isFirstFrame_ = false;
    } else {
        if (phase_ == Phase::kTitle) {
            // タイトル画面で決定ボタン押下時: ステージ選択カメラへの移動フェーズを開始
            if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) || 
                KeyboardInput::GetInstance()->IsKeyPressed(DIK_RETURN)) {
                phase_ = Phase::kTransitionToSelect;
                transitionStartPos_ = cameraTransform_.translate;
                transitionStartRot_ = cameraTransform_.rotate;
                transitionTimer_ = 0.0f;
            }
        } else if (phase_ == Phase::kStageSelect) {
            // ステージ選択画面で決定ボタン押下時: ゲームシーンへ移行
            if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) || 
                KeyboardInput::GetInstance()->IsKeyPressed(DIK_RETURN)) {
                sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
                return;
            }
        }
    }

    // -------------------------------------------------------------
    // カメラの更新とフェーズ別補間
    // -------------------------------------------------------------
    Vector3 camPos = cameraTransform_.translate;
    Vector3 camRot = cameraTransform_.rotate;

    if (phase_ == Phase::kTitle) {
        // タイトル画面: シネマティック揺れ演出
        if (enableCinematicSway_) {
            camPos.x += sinf(titleTimer_ * 0.4f) * 0.25f;
            camPos.y += sinf(titleTimer_ * 0.7f) * 0.12f;
            camPos.z += cosf(titleTimer_ * 0.3f) * 0.15f;
            camRot.x += sinf(titleTimer_ * 0.5f) * 0.015f;
            camRot.y += sinf(titleTimer_ * 0.3f) * 0.02f;
        }
        titleLogoAlpha_ = 1.0f;
        searchlightAlpha_ = 1.0f;
    } else if (phase_ == Phase::kTransitionToSelect) {
        // ステージ選択位置へ滑らかにカメラを補間移動 (Smoothstep)
        transitionTimer_ += dt;
        float t = std::clamp(transitionTimer_ / transitionDuration_, 0.0f, 1.0f);
        float ease = t * t * (3.0f - 2.0f * t); // Smoothstep

        camPos = {
            transitionStartPos_.x + (targetSelectPos_.x - transitionStartPos_.x) * ease,
            transitionStartPos_.y + (targetSelectPos_.y - transitionStartPos_.y) * ease,
            transitionStartPos_.z + (targetSelectPos_.z - transitionStartPos_.z) * ease
        };
        camRot = {
            transitionStartRot_.x + (targetSelectRot_.x - transitionStartRot_.x) * ease,
            transitionStartRot_.y + (targetSelectRot_.y - transitionStartRot_.y) * ease,
            transitionStartRot_.z + (targetSelectRot_.z - transitionStartRot_.z) * ease
        };

        cameraTransform_.translate = camPos;
        cameraTransform_.rotate = camRot;

        // タイトルロゴとサーチライトを素早く自然にフェードアウト (logoFadeDuration_ 秒で完了)
        float fadeT = std::clamp(transitionTimer_ / logoFadeDuration_, 0.0f, 1.0f);
        float remain = 1.0f - fadeT;
        float fadeAlpha = remain * remain; // スッと自然に消える2乗減衰
        if (fadeAlpha < 0.001f) {
            fadeAlpha = 0.0f;
        }
        titleLogoAlpha_ = fadeAlpha;
        searchlightAlpha_ = fadeAlpha;

        if (t >= 1.0f) {
            phase_ = Phase::kStageSelect;
            cameraTransform_.translate = targetSelectPos_;
            cameraTransform_.rotate = targetSelectRot_;
            camPos = targetSelectPos_;
            camRot = targetSelectRot_;
            titleLogoAlpha_ = 0.0f;
            searchlightAlpha_ = 0.0f;
        }
    } else if (phase_ == Phase::kStageSelect) {
        // ステージ選択画面: 目標位置に固定
        camPos = targetSelectPos_;
        camRot = targetSelectRot_;
        cameraTransform_.translate = targetSelectPos_;
        cameraTransform_.rotate = targetSelectRot_;
        titleLogoAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;
    }

    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(camRot, camPos);
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    CameraManager::GetInstance()->SetCameraInfo(camPos, viewMatrix, projectionMatrix);

    if (gameCamera_) {
        gameCamera_->SetTranslation(camPos);
        gameCamera_->SetRotation(camRot);
        gameCamera_->UpdateMatrix();
    }

    // -------------------------------------------------------------
    // タイトルロゴの浮遊アニメーション (フワフワと微小に上下)
    // -------------------------------------------------------------
    if (titleLogoSprite_) {
        float floatOffsetY = sinf(titleTimer_ * 1.6f) * 8.0f;
        const float logoW = 600.0f;
        titleLogoSprite_->SetPosition({ (1280.0f - logoW) * 0.5f, 70.0f + floatOffsetY });
        titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, titleLogoAlpha_ });
        titleLogoSprite_->Update();
    }

    // -------------------------------------------------------------
    // サーチライトの首振り・スイングアニメーション & 移行時消去
    // -------------------------------------------------------------
    if (searchlightObjects_.size() >= 2) {
        // サーチライト1 (黄色): 左右にゆっくり首振り
        if (auto tc1 = searchlightObjects_[0]->GetComponent<TransformComponent>()) {
            float rotZ1 = sinf(titleTimer_ * 0.85f) * 0.35f + 0.18f;
            float posX1 = -4.5f + sinf(titleTimer_ * 0.85f) * 0.5f;
            tc1->SetPosition({ posX1, 6.0f, 4.0f });
            tc1->SetRotation({ 0.0f, 0.0f, rotZ1 });
        }
        if (auto prc1 = searchlightObjects_[0]->GetComponent<PrimitiveRendererComponent>()) {
            prc1->GetMaterial().color.w = 0.22f * searchlightAlpha_;
        }

        // サーチライト2 (シアン): 逆位相で首振り
        if (auto tc2 = searchlightObjects_[1]->GetComponent<TransformComponent>()) {
            float rotZ2 = sinf(titleTimer_ * 0.65f + 2.0f) * 0.4f - 0.22f;
            float posX2 = 4.0f + cosf(titleTimer_ * 0.65f + 2.0f) * 0.6f;
            tc2->SetPosition({ posX2, 6.5f, 6.0f });
            tc2->SetRotation({ 0.0f, 0.0f, rotZ2 });
        }
        if (auto prc2 = searchlightObjects_[1]->GetComponent<PrimitiveRendererComponent>()) {
            prc2->GetMaterial().color.w = 0.18f * searchlightAlpha_;
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
        if (titleLogoSprite_ && titleLogoAlpha_ > 0.001f) {
            titleLogoSprite_->Draw();
        }
        for (auto &sprite : sprites_) {
            sprite->Draw();
        }
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
    // エディタ停止中もカメラの位置・角度をゲームカメラおよびCameraManagerに反映
    Vector3 camPos = cameraTransform_.translate;
    Vector3 camRot = cameraTransform_.rotate;

    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(camRot, camPos);
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    CameraManager::GetInstance()->SetCameraInfo(camPos, viewMatrix, projectionMatrix);

    if (gameCamera_) {
        gameCamera_->SetTranslation(camPos);
        gameCamera_->SetRotation(camRot);
        gameCamera_->UpdateMatrix();
    }

    for (auto &object : gameObjects_) {
        object->Update();
    }
    if (searchlightObjects_.size() >= 2) {
        if (auto prc1 = searchlightObjects_[0]->GetComponent<PrimitiveRendererComponent>()) {
            prc1->GetMaterial().color.w = 0.22f * searchlightAlpha_;
        }
        if (auto prc2 = searchlightObjects_[1]->GetComponent<PrimitiveRendererComponent>()) {
            prc2->GetMaterial().color.w = 0.18f * searchlightAlpha_;
        }
    }
    if (titleLogoSprite_) {
        titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, titleLogoAlpha_ });
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
    ImGui::Begin("タイトル/ステージ選択カメラ調整");

    // フェーズ状態の表示
    const char* phaseStr = "タイトル画面 (待機中)";
    if (phase_ == Phase::kTransitionToSelect) phaseStr = "ステージ選択カメラへ移動中...";
    else if (phase_ == Phase::kStageSelect) phaseStr = "ステージ選択画面 (待機中)";
    ImGui::Text("【現在の状態】: %s", phaseStr);
    ImGui::Separator();

    // 1. タイトルカメラ現在値
    ImGui::Text("【タイトルカメラ (現在値)】");
    ImGui::Checkbox("シネマティック微動 (自動揺れ)", &enableCinematicSway_);
    
    bool changed = false;
    if (ImGui::DragFloat3("位置 (Translate)", &cameraTransform_.translate.x, 0.1f)) {
        changed = true;
    }
    
    // 角度（ラジアンと度数法）
    float rotDeg[3] = {
        cameraTransform_.rotate.x * 180.0f / 3.14159265f,
        cameraTransform_.rotate.y * 180.0f / 3.14159265f,
        cameraTransform_.rotate.z * 180.0f / 3.14159265f
    };
    if (ImGui::DragFloat3("角度 (Deg)", rotDeg, 0.5f, -180.0f, 180.0f, "%.1f°")) {
        cameraTransform_.rotate.x = rotDeg[0] * 3.14159265f / 180.0f;
        cameraTransform_.rotate.y = rotDeg[1] * 3.14159265f / 180.0f;
        cameraTransform_.rotate.z = rotDeg[2] * 3.14159265f / 180.0f;
        changed = true;
    }

    if (changed && gameCamera_) {
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }

    ImGui::Separator();

    // 2. ステージ選択カメラ（目標値）
    ImGui::Text("【ステージ選択カメラ (目標アングル)】");
    ImGui::DragFloat3("選択時 位置", &targetSelectPos_.x, 0.1f);
    float selRotDeg[3] = {
        targetSelectRot_.x * 180.0f / 3.14159265f,
        targetSelectRot_.y * 180.0f / 3.14159265f,
        targetSelectRot_.z * 180.0f / 3.14159265f
    };
    if (ImGui::DragFloat3("選択時 角度 (Deg)", selRotDeg, 0.5f, -180.0f, 180.0f, "%.1f°")) {
        targetSelectRot_.x = selRotDeg[0] * 3.14159265f / 180.0f;
        targetSelectRot_.y = selRotDeg[1] * 3.14159265f / 180.0f;
        targetSelectRot_.z = selRotDeg[2] * 3.14159265f / 180.0f;
    }
    ImGui::DragFloat("カメラ移動時間 (秒)", &transitionDuration_, 0.1f, 0.5f, 5.0f, "%.1f秒");
    ImGui::DragFloat("ロゴ/ライト消去時間 (秒)", &logoFadeDuration_, 0.05f, 0.1f, 2.0f, "%.2f秒");

    if (ImGui::Button("ステージ選択位置に即座に配置")) {
        phase_ = Phase::kStageSelect;
        cameraTransform_.translate = targetSelectPos_;
        cameraTransform_.rotate = targetSelectRot_;
        titleLogoAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;
        if (gameCamera_) {
            gameCamera_->SetTranslation(targetSelectPos_);
            gameCamera_->SetRotation(targetSelectRot_);
            gameCamera_->UpdateMatrix();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("タイトル位置に戻す")) {
        phase_ = Phase::kTitle;
        cameraTransform_.translate = { 0.0f, 1.2f, -8.5f };
        cameraTransform_.rotate = { 0.06f, 0.0f, 0.0f };
        titleLogoAlpha_ = 1.0f;
        searchlightAlpha_ = 1.0f;
        if (gameCamera_) {
            gameCamera_->SetTranslation(cameraTransform_.translate);
            gameCamera_->SetRotation(cameraTransform_.rotate);
            gameCamera_->UpdateMatrix();
        }
    }

    if (ImGui::Button("▶ 決定演出 (ステージ選択カメラへ移動) をテスト再生")) {
        phase_ = Phase::kTransitionToSelect;
        transitionStartPos_ = cameraTransform_.translate;
        transitionStartRot_ = cameraTransform_.rotate;
        transitionTimer_ = 0.0f;
    }

    ImGui::Separator();
    DebugCamera* liveDebugCam = EditorManager::GetInstance() ? EditorManager::GetInstance()->GetDebugCamera() : nullptr;
    bool isDebugCamActive = EditorManager::GetInstance() ? EditorManager::GetInstance()->UseDebugCamera() : false;

    if (liveDebugCam) {
        Vector3 dbgPos = liveDebugCam->GetTranslation();
        Vector3 dbgRot = liveDebugCam->GetRotation();
        float dbgDeg[3] = {
            dbgRot.x * 180.0f / 3.14159265f,
            dbgRot.y * 180.0f / 3.14159265f,
            dbgRot.z * 180.0f / 3.14159265f
        };

        ImGui::Text("【デバッグカメラ (マウス操作) の現在値】");
        if (isDebugCamActive) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "状態: デバッグカメラ操作中 (ON)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "状態: ゲームカメラ表示中 (エディタ上部でONにしてください)");
        }

        ImGui::Text("位置: (%.2f, %.2f, %.2f)", dbgPos.x, dbgPos.y, dbgPos.z);
        ImGui::Text("角度(ラジアン): (%.3f, %.3f, %.3f)", dbgRot.x, dbgRot.y, dbgRot.z);
        ImGui::Text("角度(度数法): (%.1f°, %.1f°, %.1f°)", dbgDeg[0], dbgDeg[1], dbgDeg[2]);

        if (ImGui::Button("デバッグカメラの値をタイトルカメラにコピー")) {
            cameraTransform_.translate = dbgPos;
            cameraTransform_.rotate = dbgRot;
            if (gameCamera_) {
                gameCamera_->SetTranslation(dbgPos);
                gameCamera_->SetRotation(dbgRot);
                gameCamera_->UpdateMatrix();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("ステージ選択目標値にコピー")) {
            targetSelectPos_ = dbgPos;
            targetSelectRot_ = dbgRot;
        }

        ImGui::Spacing();
        ImGui::Text("C++コード用形式 (ステージ選択カメラ用):");
        ImGui::TextDisabled("pos: { %.2ff, %.2ff, %.2ff }, rot: { %.3ff, %.3ff, %.3ff }",
            dbgPos.x, dbgPos.y, dbgPos.z, dbgRot.x, dbgRot.y, dbgRot.z);
    } else {
        ImGui::TextDisabled("※デバッグカメラを取得できませんでした。");
    }

    ImGui::End();
#endif
}