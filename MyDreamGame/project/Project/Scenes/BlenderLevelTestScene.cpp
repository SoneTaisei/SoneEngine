#include "BlenderLevelTestScene.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Graphics/CameraManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Renderer/Renderer.h"
#include "Core/Utility/LogManager.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

void BlenderLevelTestScene::Initialize() {
    LogManager::GetInstance()->AddLog(LogLevel::Info, "[BlenderLevelTestScene] Initializing 3D Level Scene...");

    auto device = DirectXCommon::GetInstance()->GetDevice();

    // 1. 3Dカメラの初期化（透視投影 Perspective）
    camera_ = std::make_unique<Camera>();
    camera_->Initialize(1280, 720);
    camera_->SetTranslation({ 0.0f, 5.0f, -15.0f });
    camera_->SetRotation({ 0.2f, 0.0f, 0.0f });
    camera_->UpdateMatrix();

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(1280, 720);
    debugCamera_->SetTranslation({ 0.0f, 5.0f, -15.0f });
    debugCamera_->SetRotation({ 0.2f, 0.0f, 0.0f });
    debugCamera_->Update();

    // 3Dカメラ行列を CameraManager に設定
    CameraManager::GetInstance()->SetCameraInfo(
        camera_->GetTranslation(),
        camera_->GetViewMatrix(),
        camera_->GetProjectionMatrix()
    );

    // 2. レベルローダーの初期化とファイルロード
    levelDataLoader_ = std::make_unique<LevelDataLoader>();
    
    // 絶対パス・相対パスの順でテストロード
    if (!levelDataLoader_->LoadFile("C:/1_授業/学年/3年前期/TL1/TL.json")) {
        if (!levelDataLoader_->LoadFile("TL.json")) {
            levelDataLoader_->LoadFile("TL.scene");
        }
    }

    // オブジェクトの生成
    levelObjects_ = levelDataLoader_->CreateObjects(device, modelCommon_);

    // 初回トランスフォーム同期
    for (auto& obj : levelObjects_) {
        if (obj) {
            obj->Update();
        }
    }

    std::string countLog = "[BlenderLevelTestScene] Created " + std::to_string(levelObjects_.size()) + " 3D level objects.";
    LogManager::GetInstance()->AddLog(LogLevel::Info, countLog);
}

void BlenderLevelTestScene::OnEnter(SceneManager* sceneManager) {
    if (camera_) {
        camera_->UpdateMatrix();
        CameraManager::GetInstance()->SetCameraInfo(
            camera_->GetTranslation(),
            camera_->GetViewMatrix(),
            camera_->GetProjectionMatrix()
        );
    }
}

void BlenderLevelTestScene::OnExit(SceneManager* sceneManager) {
}

void BlenderLevelTestScene::Update(SceneManager* sceneManager) {
    // 1. カメラの更新と CameraManager への伝達
    if (useDebugCamera_ && debugCamera_) {
        debugCamera_->Update();
        CameraManager::GetInstance()->SetCameraInfo(
            debugCamera_->GetTranslation(),
            debugCamera_->GetViewMatrix(),
            debugCamera_->GetProjectionMatrix()
        );
    } else if (camera_) {
        camera_->UpdateMatrix();
        CameraManager::GetInstance()->SetCameraInfo(
            camera_->GetTranslation(),
            camera_->GetViewMatrix(),
            camera_->GetProjectionMatrix()
        );
    }

    // 2. 3Dレベルオブジェクト群の更新
    for (auto& obj : levelObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void BlenderLevelTestScene::UpdateEditor() {
    // エディタ停止中（Pause中）でもカメラと3Dオブジェクトを同期更新する
    if (useDebugCamera_ && debugCamera_) {
        debugCamera_->Update();
        CameraManager::GetInstance()->SetCameraInfo(
            debugCamera_->GetTranslation(),
            debugCamera_->GetViewMatrix(),
            debugCamera_->GetProjectionMatrix()
        );
    } else if (camera_) {
        camera_->UpdateMatrix();
        CameraManager::GetInstance()->SetCameraInfo(
            camera_->GetTranslation(),
            camera_->GetViewMatrix(),
            camera_->GetProjectionMatrix()
        );
    }

    for (auto& obj : levelObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void BlenderLevelTestScene::Draw(const Matrix4x4& viewProjectionMatrix) {
    // 3Dモデルオブジェクト群の描画
    for (auto& obj : levelObjects_) {
        if (obj) {
            obj->Draw();
        }
    }

    // レンダラーコンポーネントの描画
    Renderer::GetInstance()->RenderComponents();
}

std::vector<Object3D*> BlenderLevelTestScene::GetObjects() {
    std::vector<Object3D*> result;
    for (auto& obj : levelObjects_) {
        if (obj) {
            result.push_back(obj.get());
        }
    }
    return result;
}

void BlenderLevelTestScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    ImGui::Begin("Blender 3D Level Test Controls");

    ImGui::Checkbox("Use Debug Camera", &useDebugCamera_);

    if (ImGui::TreeNode("Camera Info")) {
        if (camera_) {
            Vector3 pos = camera_->GetTranslation();
            Vector3 rot = camera_->GetRotation();
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                camera_->SetTranslation(pos);
                camera_->UpdateMatrix();
            }
            if (ImGui::DragFloat3("Rotation", &rot.x, 0.01f)) {
                camera_->SetRotation(rot);
                camera_->UpdateMatrix();
            }
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // レベルローダーのUI（JSON/Scene ドロップダウン＆リロード）
    if (levelDataLoader_) {
        auto device = DirectXCommon::GetInstance()->GetDevice();
        levelDataLoader_->DisplayImGui(device, modelCommon_, levelObjects_);
    }

    ImGui::End();
#endif
}
