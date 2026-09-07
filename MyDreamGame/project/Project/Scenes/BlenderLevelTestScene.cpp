#include "BlenderLevelTestScene.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Graphics/CameraManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Renderer/Renderer.h"
#include "Resource/Model/ModelManager.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/LogManager.h"
#include <filesystem>

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

    // 自キャラ (Player) オブジェクトの生成 (resources/Object/Original/player を使用)
    Model* playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/player", "Player.gltf");
    if (!playerModel) {
        playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/gaikotu", "scene.gltf");
    }
    if (playerModel) {
        player_ = std::make_unique<Object3D>();
        player_->Initialize(device, playerModel);
        player_->SetName("Player");
    }

    // 2. レベルローダーの初期化とファイルロード
    levelDataLoader_ = std::make_unique<LevelDataLoader>();
    
    // ../tools/TL.json -> tools/TL.json -> TL.json の順でロードを試行
    if (!levelDataLoader_->LoadFile("../tools/TL.json")) {
        if (!levelDataLoader_->LoadFile("tools/TL.json")) {
            if (!levelDataLoader_->LoadFile("TL.json")) {
                levelDataLoader_->LoadFile("TL.scene");
            }
        }
    }

    // オブジェクトの生成
    levelObjects_ = levelDataLoader_->CreateObjects(device, modelCommon_);

    // 自キャラ配置データから自キャラに座標と回転を反映 (資料 02_04.自キャラSpawnPoint 参照)
    const auto& levelData = levelDataLoader_->GetLevelData();
    if (player_ && !levelData.players.empty()) {
        const auto& playerData = levelData.players[0];
        player_->SetTranslation(playerData.translation);
        constexpr float degToRad = 3.14159265358979323846f / 180.0f;
        Vector3 radRotation = {
            playerData.rotation.x * degToRad,
            playerData.rotation.y * degToRad,
            playerData.rotation.z * degToRad
        };
        player_->SetRotation(radRotation);
    }

    // 敵キャラ配置データから敵キャラを生成・配置 (資料 02_05.敵キャラSpawnPoint 参照)
    enemies_.clear();
    for (const auto& enemyData : levelData.enemies) {
        // 敵オブジェクト専用独立モデル (resources/Object/Original/enemy/enemy.obj) を使用
        Model* enemyModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/enemy", "enemy.obj");
        if (!enemyModel && !enemyData.fileName.empty()) {
            enemyModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/" + enemyData.fileName, enemyData.fileName);
            if (!enemyModel) {
                enemyModel = ModelManager::GetInstance()->GetModel("resources/Object/School/" + enemyData.fileName, enemyData.fileName);
            }
        }
        if (!enemyModel) {
            enemyModel = ModelManager::GetInstance()->GetModel("resources/Object/School/multiMesh", "multiMesh.obj");
        }

        if (enemyModel) {
            auto enemy = std::make_unique<Object3D>();
            enemy->Initialize(device, enemyModel);

            enemy->SetTranslation(enemyData.translation);
            constexpr float degToRad = 3.14159265358979323846f / 180.0f;
            Vector3 radRotation = {
                enemyData.rotation.x * degToRad,
                enemyData.rotation.y * degToRad,
                enemyData.rotation.z * degToRad
            };
            enemy->SetRotation(radRotation);
            enemy->SetName("EnemySpawn (" + (enemyData.fileName.empty() ? "default" : enemyData.fileName) + ")");
            enemy->Update();
            enemies_.push_back(std::move(enemy));
        }
    }

    if (player_) {
        player_->Update();
    }

    // 初回トランスフォーム同期
    for (auto& obj : levelObjects_) {
        if (obj) {
            obj->Update();
        }
    }

    std::string countLog = "[BlenderLevelTestScene] Created " + std::to_string(levelObjects_.size()) + " level objects, " + std::to_string(enemies_.size()) + " enemies.";
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
    if (player_) {
        player_->Update();
    }
    for (auto& enemy : enemies_) {
        if (enemy) {
            enemy->Update();
        }
    }
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

    if (player_) {
        player_->Update();
    }
    for (auto& enemy : enemies_) {
        if (enemy) {
            enemy->Update();
        }
    }
    for (auto& obj : levelObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void BlenderLevelTestScene::Draw(const Matrix4x4& viewProjectionMatrix) {
    // 自キャラの描画
    if (player_) {
        player_->Draw();
    }
    // 敵キャラの描画
    for (auto& enemy : enemies_) {
        if (enemy) {
            enemy->Draw();
        }
    }
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
    if (player_) {
        result.push_back(player_.get());
    }
    for (auto& enemy : enemies_) {
        if (enemy) {
            result.push_back(enemy.get());
        }
    }
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

    if (ImGui::TreeNode("Player Info (Spawned)")) {
        if (player_) {
            Vector3 pos = player_->GetTranslation();
            Vector3 rot = player_->GetRotation();
            Vector3 scale = player_->GetScale();
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                player_->SetTranslation(pos);
            }
            if (ImGui::DragFloat3("Rotation (Rad)", &rot.x, 0.01f)) {
                player_->SetRotation(rot);
            }
            if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                player_->SetScale(scale);
            }
        } else {
            ImGui::Text("Player object not spawned/initialized.");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Enemy Info (Spawned)")) {
        ImGui::Text("Enemies Count: %d", (int)enemies_.size());
        for (size_t i = 0; i < enemies_.size(); ++i) {
            if (enemies_[i]) {
                std::string label = "Enemy [" + std::to_string(i) + "] " + enemies_[i]->GetName();
                if (ImGui::TreeNode(label.c_str())) {
                    Vector3 pos = enemies_[i]->GetTranslation();
                    Vector3 rot = enemies_[i]->GetRotation();
                    Vector3 scale = enemies_[i]->GetScale();
                    if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
                        enemies_[i]->SetTranslation(pos);
                    }
                    if (ImGui::DragFloat3("Rotation (Rad)", &rot.x, 0.01f)) {
                        enemies_[i]->SetRotation(rot);
                    }
                    if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                        enemies_[i]->SetScale(scale);
                    }
                    ImGui::TreePop();
                }
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
