#include "StageSelectScene.h"
#include "../externals/imgui/imgui.h"
#include "Graphics/TextureManager.h"
#include "Scene/SceneManager.h"
#include "Core/Utility/ImGuiHelper.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Model/ModelCommon.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include "Scene/SceneFactory.h"
#include "GameScene.h"
#include "Core/TimeManager.h"
#include "Graphics/CameraManager.h"
#include "Renderer/Renderer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include "Component/TransformComponent.h"
#include "GameObject/Object3D.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Graphics/GameCamera.h"
#include <cmath>
#include <numbers>

StageSelectScene::~StageSelectScene() {}

void StageSelectScene::OnEnter(SceneManager* sceneManager) {
    // シーン開始時にカメラをリセットして正面中央に向ける
    if (gameCamera_) {
        gameCamera_->Reset();
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }
    CameraManager::GetInstance()->ClearCullingCameraInfo();
}

void StageSelectScene::OnExit(SceneManager* sceneManager) {
    // 次のシーンへ渡すデータをセットする（選択したステージのパスなど）
    if (!stageConfigs_.empty() && currentStageIndex_ < stageConfigs_.size()) {
        sceneManager->SetData("SelectedStagePath", std::string(stageConfigs_[currentStageIndex_].jsonPath));
    }
}

void StageSelectScene::Initialize() {
    // Deviceの取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();

    cameraTransform_.translate = {0.0f, 0.0f, -9.0f};
    cameraTransform_.rotate = {0.0f, 0.0f, 0.0f};

    // カメラの初期化リセット
    if (gameCamera_) {
        gameCamera_->Reset();
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }
    CameraManager::GetInstance()->ClearCullingCameraInfo();

    // PrimitiveManager の初期化とBoxプリミティブ取得
    PrimitiveManager::GetInstance()->Initialize(device.Get());
    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);

    // 3つのステージ用スプライト（テクスチャ）
    const std::string stageTextures[kMaxSelectableStages] = {
        "resources/Sprite/Original/UI/stage1.png",
        "resources/Sprite/Original/UI/stage2.png",
        "resources/Sprite/Original/UI/stage3.png"
    };

    stageBoxes_.clear();
    stageBoxes_.resize(kMaxSelectableStages);

    for (int i = 0; i < kMaxSelectableStages; ++i) {
        auto boxObj = std::make_shared<GameObject>("stage_box_" + std::to_string(i + 1));
        auto tc = boxObj->AddComponent<TransformComponent>();

        float initialX = (static_cast<float>(i) - static_cast<float>(currentStageIndex_)) * boxSpacing_;
        float initialZ = (i == currentStageIndex_) ? selectedZ_ : unselectedZ_;
        float initialScale = (i == currentStageIndex_) ? selectedScale_ : unselectedScale_;

        tc->SetPosition({initialX, 0.0f, initialZ});
        tc->SetScale({initialScale, initialScale, initialScale});
        tc->SetRotation({0.0f, 0.0f, 0.0f});

        uint32_t texIndex = TextureManager::GetInstance()->Load(stageTextures[i]);
        D3D12_GPU_DESCRIPTOR_HANDLE texHandle = TextureManager::GetInstance()->GetGpuHandle(texIndex);

        auto renderer = boxObj->AddComponent<PrimitiveRendererComponent>();
        renderer->Initialize(device.Get(), boxPrimitive);
        renderer->SetTextureHandle(texHandle);

        Material& mat = renderer->GetMaterial();
        mat.color = {1.0f, 1.0f, 1.0f, 1.0f};
        mat.lightingType = enableLighting_ ? 1 : 0;

        stageBoxes_[i].gameObject = boxObj;
        stageBoxes_[i].position = {initialX, 0.0f, initialZ};
        stageBoxes_[i].rotation = {0.0f, 0.0f, 0.0f};
        stageBoxes_[i].scale = initialScale;

        gameObjects_.push_back(boxObj);
    }



    // Skyboxの初期化
    uint32_t skyboxHandle = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxHandle);

    LoadConfig();
    RefreshAvailableMapFiles();
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));
}

void StageSelectScene::Update(SceneManager *sceneManager) {
    float dt = TimeManager::GetInstance().GetDeltaTime();

    // GameCamera（WindowsApplicationでactiveCamera_として使われるカメラ）の状態を同期
    if (gameCamera_) {
        gameCamera_->SetOrthographic(false);
        gameCamera_->SetFollowTarget(nullptr);
        gameCamera_->SetFollowEnabled(false);
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->SetFov(0.45f);
        gameCamera_->UpdateMatrix();
    }
    CameraManager::GetInstance()->ClearCullingCameraInfo();

    // 1. カメラのTransformからビュー行列を作成
    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);

    // 2. プロジェクション行列（透視投影行列）を作成
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);

    CameraManager::GetInstance()->SetCameraInfo(cameraTransform_.translate, viewMatrix, projectionMatrix);

    // 入力処理（遅延タイマー経過後）
    bool canInput = true;
    if (inputDelayTimer_ > 0.0f) {
        inputDelayTimer_ -= dt;
        canInput = false;
    }

    if (canInput) {
        auto keyboard = KeyboardInput::GetInstance();
        auto gamepad = GamepadInput::GetInstance();

        bool moveLeft = keyboard->IsKeyPressed(DIK_A) || keyboard->IsKeyPressed(DIK_LEFT) ||
                        gamepad->IsDPadPressedLeft() || gamepad->IsLeftStickPushedLeft() ||
                        gamepad->IsButtonPressed(GamepadButton::LB);
        if (moveLeft) {
            currentStageIndex_--;
            if (currentStageIndex_ < 0) {
                currentStageIndex_ = kMaxSelectableStages - 1;
            }
        }

        bool moveRight = keyboard->IsKeyPressed(DIK_D) || keyboard->IsKeyPressed(DIK_RIGHT) ||
                         gamepad->IsDPadPressedRight() || gamepad->IsLeftStickPushedRight() ||
                         gamepad->IsButtonPressed(GamepadButton::RB);
        if (moveRight) {
            currentStageIndex_++;
            if (currentStageIndex_ >= kMaxSelectableStages) {
                currentStageIndex_ = 0;
            }
        }

        // 決定（ゲーム開始）
        bool confirm = keyboard->IsKeyPressed(DIK_SPACE) || keyboard->IsKeyPressed(DIK_RETURN) ||
                       gamepad->IsButtonPressed(GamepadButton::A) || gamepad->IsButtonPressed(GamepadButton::Start);
        if (confirm) {
            std::string mapPath = "";
            if (currentStageIndex_ >= 0 && currentStageIndex_ < static_cast<int>(stageConfigs_.size())) {
                mapPath = stageConfigs_[currentStageIndex_].jsonPath;
            }
            if (mapPath.empty() || mapPath == "none") {
                if (currentStageIndex_ == 0) mapPath = "map_data.txt";
                else if (currentStageIndex_ == 1) mapPath = "map_data1.txt";
                else if (currentStageIndex_ == 2) mapPath = "map_data2.txt";
            }
            GameScene::s_TargetMapFilePath = "resources/json/shared/MapData/" + mapPath;
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
            return;
        }

        // 戻る（タイトル画面へ）
        bool cancel = keyboard->IsKeyPressed(DIK_ESCAPE) ||
                      gamepad->IsButtonPressed(GamepadButton::B) ||
                      gamepad->IsButtonPressed(GamepadButton::Back);
        if (cancel) {
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
            return;
        }
    }

    // 3つのStage Boxのアニメーション（中央揃えスライド・サイズ・回転）
    const float twoPi = 2.0f * 3.1415926535f;
    float posLerpFactor = 1.0f - std::exp(-8.0f * dt);
    float rotLerpFactor = 1.0f - std::exp(-6.0f * dt);

    for (int i = 0; i < kMaxSelectableStages && i < static_cast<int>(stageBoxes_.size()); ++i) {
        // 目標位置・スケール（現在選ばれているBoxが中央 X=0 になるように配置）
        float targetX = (static_cast<float>(i) - static_cast<float>(currentStageIndex_)) * boxSpacing_;
        float targetZ = (i == currentStageIndex_) ? selectedZ_ : unselectedZ_;
        float targetScale = (i == currentStageIndex_) ? selectedScale_ : unselectedScale_;

        // 位置のLerp
        stageBoxes_[i].position.x += (targetX - stageBoxes_[i].position.x) * posLerpFactor;
        stageBoxes_[i].position.y += (0.0f - stageBoxes_[i].position.y) * posLerpFactor;
        stageBoxes_[i].position.z += (targetZ - stageBoxes_[i].position.z) * posLerpFactor;

        // スケールのLerp
        stageBoxes_[i].scale += (targetScale - stageBoxes_[i].scale) * posLerpFactor;

        // 回転の処理
        if (i == currentStageIndex_) {
            // 選択されたBoxは回転する
            stageBoxes_[i].rotation.y += rotateSpeed_ * dt;
            if (stageBoxes_[i].rotation.y >= twoPi) {
                stageBoxes_[i].rotation.y = std::fmod(stageBoxes_[i].rotation.y, twoPi);
            }
            stageBoxes_[i].rotation.x += (0.0f - stageBoxes_[i].rotation.x) * rotLerpFactor;
            stageBoxes_[i].rotation.z += (0.0f - stageBoxes_[i].rotation.z) * rotLerpFactor;
        } else {
            // 非選択のBoxは正面（角度0）へスムーズに戻る
            while (stageBoxes_[i].rotation.y < 0.0f) stageBoxes_[i].rotation.y += twoPi;
            while (stageBoxes_[i].rotation.y >= twoPi) stageBoxes_[i].rotation.y -= twoPi;

            float targetAngle = (stageBoxes_[i].rotation.y > 3.1415926535f) ? twoPi : 0.0f;
            stageBoxes_[i].rotation.y += (targetAngle - stageBoxes_[i].rotation.y) * rotLerpFactor;
            if (stageBoxes_[i].rotation.y >= twoPi) stageBoxes_[i].rotation.y -= twoPi;

            stageBoxes_[i].rotation.x += (0.0f - stageBoxes_[i].rotation.x) * rotLerpFactor;
            stageBoxes_[i].rotation.z += (0.0f - stageBoxes_[i].rotation.z) * rotLerpFactor;
        }

        // TransformComponentへの反映
        if (stageBoxes_[i].gameObject) {
            if (auto tc = stageBoxes_[i].gameObject->GetComponent<TransformComponent>()) {
                tc->SetPosition(stageBoxes_[i].position);
                tc->SetRotation(stageBoxes_[i].rotation);
                tc->SetScale({stageBoxes_[i].scale, stageBoxes_[i].scale, stageBoxes_[i].scale});
            }
            if (auto prc = stageBoxes_[i].gameObject->GetComponent<PrimitiveRendererComponent>()) {
                prc->GetMaterial().lightingType = enableLighting_ ? 1 : 0;
            }
        }
    }

    // 全オブジェクトの更新（座標変換行列の計算など）
    for (auto &object : gameObjects_) {
        object->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }
}

void StageSelectScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // Skyboxの描画前にDescriptorHeapをセットさせるため、PreDrawを呼ぶ
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

    // 各オブジェクトに「自分の行列で描画して！」と頼む
    for (auto &object : gameObjects_) {
        object->Draw();
    }

    Renderer::GetInstance()->RenderComponents();
}

std::vector<Object3D *> StageSelectScene::GetObjects() {
    return {};
}

void StageSelectScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    // プレイヤー向けの現在の選択ステージ表示
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    ImVec2 windowPos;
    windowPos.x = workPos.x + workSize.x * 0.5f;
    windowPos.y = workPos.y + PAD;
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.45f); 
    if (ImGui::Begin("StageSelect Overlay", nullptr, windowFlags)) {
        ImGui::Text("選択中のステージ: ステージ %d", currentStageIndex_ + 1);
        if (GamepadInput::GetInstance()->IsConnected()) {
            ImGui::Text("十字キー/スティック/LB・RBで変更、[A]で開始、[B]で戻る");
        } else {
            ImGui::Text("A/D または 矢印キーで変更、SPACE/ENTERで開始、ESCで戻る");
        }
    }
    ImGui::End();

    // エディター向けの設定ウィンドウ
    ImGui::Begin("ステージセレクトエディター");
    if (ImGui::TreeNode("Box表示・アニメーション設定")) {
        ImGui::DragFloat("Box間隔", &boxSpacing_, 0.1f, 1.0f, 10.0f);
        ImGui::DragFloat("回転速度", &rotateSpeed_, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("選択時スケール", &selectedScale_, 0.05f, 0.5f, 5.0f);
        ImGui::DragFloat("非選択時スケール", &unselectedScale_, 0.05f, 0.5f, 5.0f);
        ImGui::DragFloat("選択時Z座標", &selectedZ_, 0.1f, -10.0f, 10.0f);
        ImGui::DragFloat("非選択時Z座標", &unselectedZ_, 0.1f, -10.0f, 10.0f);
        ImGui::Checkbox("ライティング有効", &enableLighting_);
        ImGui::TreePop();
    }

    if (ImGui::InputInt("ステージ数", &stageCount_)) {
        if (stageCount_ < 1) stageCount_ = 1;
        stageConfigs_.resize(stageCount_);
    }

    if (ImGui::Button("マップ一覧更新")) {
        RefreshAvailableMapFiles();
    }

    for (int i = 0; i < stageCount_; ++i) {
        ImGui::PushID(i);
        ImGui::Text("ステージ %d", i + 1);

        int currentIndex = -1;
        std::vector<const char*> items;
        for (size_t j = 0; j < availableMapFiles_.size(); ++j) {
            items.push_back(availableMapFiles_[j].c_str());
            if (availableMapFiles_[j] == stageConfigs_[i].jsonPath) {
                currentIndex = static_cast<int>(j);
            }
        }
        
        if (currentIndex == -1 && strlen(stageConfigs_[i].jsonPath) > 0) {
            availableMapFiles_.push_back(stageConfigs_[i].jsonPath);
            items.push_back(availableMapFiles_.back().c_str());
            currentIndex = static_cast<int>(items.size() - 1);
        }

        if (ImGui::Combo("マップファイル", &currentIndex, items.data(), static_cast<int>(items.size()))) {
            if (currentIndex >= 0 && currentIndex < static_cast<int>(availableMapFiles_.size())) {
                strcpy_s(stageConfigs_[i].jsonPath, availableMapFiles_[currentIndex].c_str());
            }
        }
        ImGui::PopID();
    }

    if (ImGui::Button("設定を保存")) {
        SaveConfig();
    }
    ImGui::End();
#endif
}

void StageSelectScene::RefreshAvailableMapFiles() {
    availableMapFiles_.clear();
    std::string path = "resources/json/shared/MapData";
    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".txt" || ext == ".json") {
                    std::string filePath = entry.path().filename().string();
                    availableMapFiles_.push_back(filePath);
                }
            }
        }
    }
}

void StageSelectScene::SaveConfig() {
    std::filesystem::create_directories("resources/json/shared");
    std::ofstream ofs("resources/json/shared/stage_config.txt");
    if (!ofs.is_open()) return;
    ofs << stageCount_ << "\n";
    for (int i = 0; i < stageCount_; ++i) {
        std::string path = stageConfigs_[i].jsonPath;
        if (path.empty()) path = "none";
        ofs << path << "\n";
    }
}

void StageSelectScene::LoadConfig() {
    std::ifstream ifs("resources/json/shared/stage_config.txt");
    if (!ifs.is_open()) {
        stageCount_ = kMaxSelectableStages;
        stageConfigs_.resize(kMaxSelectableStages);
        strcpy_s(stageConfigs_[0].jsonPath, "map_data1.txt");
        strcpy_s(stageConfigs_[1].jsonPath, "map_data2.txt");
        strcpy_s(stageConfigs_[2].jsonPath, "map_data.txt");
        return;
    }
    
    if (ifs >> stageCount_) {
        if (stageCount_ < 1) stageCount_ = 1;
        stageConfigs_.resize(stageCount_);
        std::string path;
        for (int i = 0; i < stageCount_; ++i) {
            if (ifs >> path) {
                if (path == "none") path = "";
                else path = std::filesystem::path(path).filename().string();
                strcpy_s(stageConfigs_[i].jsonPath, path.c_str());
            }
        }
    }

    if (stageCount_ < kMaxSelectableStages) {
        stageCount_ = kMaxSelectableStages;
        stageConfigs_.resize(kMaxSelectableStages);
        if (strlen(stageConfigs_[0].jsonPath) == 0) strcpy_s(stageConfigs_[0].jsonPath, "map_data1.txt");
        if (strlen(stageConfigs_[1].jsonPath) == 0) strcpy_s(stageConfigs_[1].jsonPath, "map_data2.txt");
        if (strlen(stageConfigs_[2].jsonPath) == 0) strcpy_s(stageConfigs_[2].jsonPath, "map_data.txt");
    }
}
