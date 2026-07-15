#include "StageSelectScene.h"
#include "../externals/imgui/imgui.h"
#include "Graphics/TextureManager.h"
#include "Scene/SceneManager.h"
#include "Core/Utility/ImGuiHelper.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Model/ModelCommon.h"
#include "Input/KeyboardInput.h"
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

StageSelectScene::~StageSelectScene() {}

void StageSelectScene::OnEnter(SceneManager* sceneManager) {
    // シーン開始時に、可能なら前回の選択ステージなどを復元する
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

    // AnimatedCubeの追加
    Model* animatedCubeModel = ModelManager::GetInstance()->GetModel("resources/Object/School/human", "walk.gltf");
    Animation cubeAnimation = LoadAnimationFile("resources/Object/School/human", "walk.gltf");
    
    auto animatedCubeObject = std::make_shared<GameObject>("human_walk");
    auto cubeTransform = animatedCubeObject->AddComponent<TransformComponent>();
    // プログラムのロード時にスケール等を調整するようにしたため、ここでは完全に基準値を設定する
    cubeTransform->SetPosition({0.0f, 0.0f, 0.0f});
    cubeTransform->SetScale({1.0f, 1.0f, 1.0f});
    cubeTransform->SetRotation({0.0f, 0.0f, 0.0f}); // ローテーションも完全に0にする
    
    uint32_t cubeTexIndex = TextureManager::GetInstance()->Load("resources/Object/School/human/white.png");
    D3D12_GPU_DESCRIPTOR_HANDLE cubeTH = TextureManager::GetInstance()->GetGpuHandle(cubeTexIndex);
    
    auto cubeRenderer = animatedCubeObject->AddComponent<MeshRendererComponent>();
    cubeRenderer->Initialize(device.Get(), animatedCubeModel);
    cubeRenderer->SetTextureHandle(cubeTH);
    animatedCubeModel->SetTextureHandle(cubeTH); // Modelの内部テクスチャハンドルを上書き
    
    auto cubeAnimator = animatedCubeObject->AddComponent<AnimatorComponent>();
    cubeAnimator->Initialize();
    cubeAnimator->SetModelData(animatedCubeModel->GetModelData()); // Skeletonの生成
    cubeAnimator->SetAnimation(cubeAnimation);
    cubeAnimator->SetTargetNodeName("AnimatedCube");
    cubeAnimator->Play();
    
    gameObjects_.push_back(animatedCubeObject);

    // Skyboxの初期化
    uint32_t skyboxHandle = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxHandle);

    LoadConfig();
    RefreshAvailableMapFiles();
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));
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
    if (keyboard->IsKeyPressed(DIK_A)) {
        currentStageIndex_--;
        if (currentStageIndex_ < 0) {
            currentStageIndex_ = stageCount_ - 1;
        }
    }
    if (keyboard->IsKeyPressed(DIK_D)) {
        currentStageIndex_++;
        if (currentStageIndex_ >= stageCount_) {
            currentStageIndex_ = 0;
        }
    }

    if (keyboard->IsKeyPressed(DIK_SPACE)) {
        if (currentStageIndex_ >= 0 && currentStageIndex_ < stageConfigs_.size()) {
            GameScene::s_TargetMapFilePath = "resources/json/MapData/" + std::string(stageConfigs_[currentStageIndex_].jsonPath);
        }
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
    std::string path = "resources/json/MapData";
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
    std::ofstream ofs("resources/json/stage_config.txt");
    if (!ofs.is_open()) return;
    ofs << stageCount_ << "\n";
    for (int i = 0; i < stageCount_; ++i) {
        std::string path = stageConfigs_[i].jsonPath;
        if (path.empty()) path = "none";
        ofs << path << "\n";
    }
}

void StageSelectScene::LoadConfig() {
    std::ifstream ifs("resources/json/stage_config.txt");
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
