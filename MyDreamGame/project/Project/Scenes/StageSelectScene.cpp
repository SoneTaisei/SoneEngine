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
#include "Resource/Audio/AudioManager.h"
#include "GameScene.h"
#include "Game2D/Blocks/SavePoint.h"
#include "Core/TimeManager.h"
#include "Graphics/CameraManager.h"
#include "Renderer/Renderer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <numbers>
#include "Component/TransformComponent.h"
#include "GameObject/Object3D.h"

StageSelectScene::~StageSelectScene() {}

void StageSelectScene::OnEnter(SceneManager* sceneManager) {
    // シーン開始時に、可能なら前回の選択ステージなどを復元する
    AudioManager::StopAllBGM();
    AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Title.mp3", true, 0.4f);
    AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Select.mp3", true, 0.4f);
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

    // 1. マネージャから素材を借りる（頂点バッファを重複させない！）
    // Model *skydomeModelResource = ModelManager::GetInstance()->GetModel("resources/Object/Original/sphere", "sphere.gltf");
    // uint32_t skydomeIndex = TextureManager::GetInstance()->Load("resources/Sprite/School/monsterBall.png");
    // D3D12_GPU_DESCRIPTOR_HANDLE skydomeTH...

    // 2. GameObjectを作る
    // auto skydomeObject = ...
    // auto transform = ...
    // transform->SetRotation({0.0f, 0.0f, 0.0f});

    // 3. 描画コンポーネントのアタッチとテクスチャの設定
    // auto skydomeRenderer = ...
    // skydomeRenderer->Initialize...
    // skydomeRenderer->SetTextureHandle...
    // skydomeModelResource->SetTextureHandle...

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    // gameObjects_.push_back(skydomeObject);

    // プレイヤーモデルの追加
    Model* playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/player", "Player.gltf");
    
    auto playerObject = std::make_shared<GameObject>("player_model");
    auto playerTransform = playerObject->AddComponent<TransformComponent>();
    playerTransform->SetPosition({0.0f, -1.0f, 0.0f});
    playerTransform->SetScale({2.0f, 2.0f, 2.0f});
    playerTransform->SetRotation({0.0f, 3.14159265f, 0.0f}); // 正面（手前）を向かせる
    
    auto playerRenderer = playerObject->AddComponent<MeshRendererComponent>();
    playerRenderer->Initialize(device.Get(), playerModel);
    
    gameObjects_.push_back(playerObject);

    // Skyboxの初期化
    uint32_t skyboxHandle = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxHandle);

    LoadConfig();
    RefreshAvailableMapFiles();
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    // ステージガイド看板モデル（plan.obj）の初期化
    LoadStageGuideTextures();
    Model* guideModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/plan", "plan.obj");
    if (guideModel) {
        guideObject_ = std::make_shared<GameObject>("StageGuideBanner");
        guideTransform_ = guideObject_->AddComponent<TransformComponent>();
        // plan.objはYZ平面（厚みX方向、法線+X/-X）のため、Y軸まわりに-90度回転させて正面（+X法線面）をカメラ（-Z方向）に向ける
        guideTransform_->SetRotation({0.0f, -std::numbers::pi_v<float> / 2.0f, 0.0f});
        // プレイヤー頭上に見やすく配置
        guideTransform_->SetPosition({0.0f, 1.8f, 0.0f});
        // 画像アスペクト比 800:200 (4:1) に合わせてスケール調整（幅6.0、高さ1.5）
        guideTransform_->SetScale({1.0f, 0.75f, 3.0f});

        guideRenderer_ = guideObject_->AddComponent<MeshRendererComponent>();
        guideRenderer_->Initialize(device.Get(), guideModel);
        guideRenderer_->SetIsDoubleSided(true);
        // UI/ガイド看板用のためライティングを受けずに鮮明に発色させる
        guideRenderer_->GetMaterial().lightingType = 0;

        gameObjects_.push_back(guideObject_);
    }
    UpdateGuideBanner();
}

void StageSelectScene::Update(SceneManager *sceneManager) {
    // 1. カメラのTransformからビュー行列を作成
    // cameraTransform_ は TitleScene.h で宣言されているものを使います
    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);

    // 2. プロジェクション行列（透視投影行列）を作成
    // 一般的な設定：視野角0.45rad, アスペクト比16:9, 近平面0.1, 遠平面1000.0
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);

    CameraManager::GetInstance()->SetCameraInfo(cameraTransform_.translate, viewMatrix, projectionMatrix);

    // 全オブジェクトの更新（座標変換行列の計算など）
    for (auto &object : gameObjects_) {
        object->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    if (inputDelayTimer_ > 0.0f) {
        inputDelayTimer_ -= TimeManager::GetInstance().GetDeltaTime();
        return;
    }

    auto keyboard = KeyboardInput::GetInstance();
    auto pad = GamepadInput::GetInstance();

    static float s_stageSelectPadCooldown = 0.0f;
    if (s_stageSelectPadCooldown > 0.0f) {
        s_stageSelectPadCooldown -= TimeManager::GetInstance().GetDeltaTime();
    }

    bool movePrev = keyboard->IsKeyPressed(DIK_A) || keyboard->IsKeyPressed(DIK_LEFT);
    bool moveNext = keyboard->IsKeyPressed(DIK_D) || keyboard->IsKeyPressed(DIK_RIGHT);

    if (pad && pad->IsConnected() && s_stageSelectPadCooldown <= 0.0f) {
        float stickX = pad->GetLeftStick().x;
        if (pad->IsDPadLeft() || stickX < -0.5f) {
            movePrev = true;
            s_stageSelectPadCooldown = 0.25f;
        } else if (pad->IsDPadRight() || stickX > 0.5f) {
            moveNext = true;
            s_stageSelectPadCooldown = 0.25f;
        }
    }

    if (movePrev) {
        currentStageIndex_--;
        if (currentStageIndex_ < 0) {
            currentStageIndex_ = stageCount_ - 1;
        }
        AudioManager::Play("resources/Sound/10Dyas/SE/SelectMove.mp3", 0.7f);
        UpdateGuideBanner();
    }
    if (moveNext) {
        currentStageIndex_++;
        if (currentStageIndex_ >= stageCount_) {
            currentStageIndex_ = 0;
        }
        AudioManager::Play("resources/Sound/10Dyas/SE/SelectMove.mp3", 0.7f);
        UpdateGuideBanner();
    }

    bool isDecision = keyboard->IsKeyPressed(DIK_SPACE) || keyboard->IsKeyPressed(DIK_RETURN) ||
                      (pad && (pad->IsButtonPressed(GamepadButton::A) || pad->IsButtonPressed(0)));

    if (isDecision) {
        AudioManager::Play("resources/Sound/10Dyas/SE/Select.mp3", 0.8f);
        if (currentStageIndex_ >= 0 && currentStageIndex_ < stageConfigs_.size()) {
            GameScene::s_TargetMapFilePath = "resources/json/shared/MapData/" + std::string(stageConfigs_[currentStageIndex_].jsonPath);
        }
        SavePoint::Clear(GameScene::s_TargetMapFilePath);
        sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
        return;
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
    // ガイド看板のTransform調整ウィンドウ
    if (guideTransform_) {
        ImGui::Begin("ステージガイド看板 調整");
        Vector3 pos = guideTransform_->GetPosition();
        Vector3 rot = guideTransform_->GetRotation();
        Vector3 scale = guideTransform_->GetScale();

        bool changed = false;
        if (ImGui::DragFloat3("位置 (Position)", &pos.x, 0.05f)) changed = true;
        if (ImGui::DragFloat3("回転 (Rotation)", &rot.x, 0.05f)) changed = true;
        if (ImGui::DragFloat3("拡大縮小 (Scale)", &scale.x, 0.05f)) changed = true;

        if (changed) {
            guideTransform_->SetPosition(pos);
            guideTransform_->SetRotation(rot);
            guideTransform_->SetScale(scale);
        }
        if (ImGui::Button("初期位置にリセット")) {
            guideTransform_->SetPosition({0.0f, 1.8f, 0.0f});
            guideTransform_->SetRotation({0.0f, -std::numbers::pi_v<float> / 2.0f, 0.0f});
            guideTransform_->SetScale({1.0f, 0.75f, 3.0f});
        }
        ImGui::End();
    }

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
    ImGui::SetNextWindowBgAlpha(0.35f); 
    if (ImGui::Begin("StageSelect Overlay", nullptr, windowFlags)) {
        ImGui::Text("選択中のステージ: %d", currentStageIndex_ + 1);
        ImGui::Text("A/Dで変更、SPACEで開始");
    }
    ImGui::End();

    // エディター向けの設定ウィンドウ
    ImGui::Begin("ステージセレクトエディター");
    if (ImGui::InputInt("ステージ数", &stageCount_)) {
        if (stageCount_ < 1) stageCount_ = 1;
        stageConfigs_.resize(stageCount_);
        LoadStageGuideTextures();
        UpdateGuideBanner();
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
            if (currentIndex >= 0 && currentIndex < availableMapFiles_.size()) {
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
        stageCount_ = 1;
        stageConfigs_.resize(1);
        strcpy_s(stageConfigs_[0].jsonPath, "map_data.txt");
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
}

void StageSelectScene::LoadStageGuideTextures() {
    auto texMgr = TextureManager::GetInstance();
    fallbackGuideTexture_ = texMgr->Load("resources/Sprite/Original/UI/stage1.png");

    stageGuideTextures_.clear();
    int count = (std::max)(stageCount_, 10);
    for (int i = 0; i < count; ++i) {
        std::string path = "resources/Sprite/Original/UI/stage" + std::to_string(i + 1) + ".png";
        if (std::filesystem::exists(path)) {
            stageGuideTextures_.push_back(texMgr->Load(path));
        } else {
            // stage{N}.png が見つからない場合は直前の有効画像または fallback
            if (!stageGuideTextures_.empty() && stageGuideTextures_.back() != 0) {
                stageGuideTextures_.push_back(stageGuideTextures_.back());
            } else {
                stageGuideTextures_.push_back(fallbackGuideTexture_);
            }
        }
    }
}

void StageSelectScene::UpdateGuideBanner() {
    if (!guideRenderer_) return;
    uint32_t handle = fallbackGuideTexture_;
    if (currentStageIndex_ >= 0 && currentStageIndex_ < static_cast<int>(stageGuideTextures_.size())) {
        handle = stageGuideTextures_[currentStageIndex_];
    }
    if (handle != 0) {
        guideRenderer_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(handle));
    }
}

