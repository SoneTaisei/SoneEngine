#ifdef USE_IMGUI
#include "EditorConfigManager.h"
#include "Collision/CollisionManager.h"
#include "Editor/Model3DEditor/Model3DEditorContext.h"
#include "EditorLayoutManager.h"
#include "EditorManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/IScene.h"
#include "Scene/SceneManager.h"
#include "Graphics/Camera/GameCamera.h"
#include "Graphics/Camera/DebugCamera.h"
#include "GameObject/WorldGif.h"
#include "Game2D/MapChip2D.h"
#include "Editor/MapEditor/MapEditor.h"
#include "Editor/MapEditor/MapEditorContext.h"
#include "Editor/Animation/AnimationEditor.h"
#include "Editor/GPUParticle/GPUParticleEditor.h"
#include "Scene/SceneFactory.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

void EditorConfigManager::SaveSceneConfig(EditorManager *editor) {
    if (!editor)
        return;

    std::filesystem::create_directories("resources/json/local");
    std::ofstream ofs("resources/json/local/editor_config.json");
    if (ofs.is_open()) {
        auto dxCommon = DirectXCommon::GetInstance();
        nlohmann::json j;
        j["currentScene"] = SceneFactory::GetSceneTypeName(editor->GetCurrentSceneType());
        j["isOutlineEnabled"] = dxCommon ? dxCommon->IsOutlineEnabled() : false;
        j["isDepthBasedOutlineEnabled"] = dxCommon ? dxCommon->IsDepthBasedOutlineEnabled() : true;
        j["outlineThickness"] = dxCommon ? dxCommon->GetOutlineThickness() : 0.015f;
        j["isColliderWireEnabled"] = CollisionManager::GetInstance()->IsDebugDrawEnabled();
        j["isColliderDepthTestEnabled"] = CollisionManager::GetInstance()->IsDepthTestEnabled();

        // アクティブメインタブ
        j["activeMainTab"] = editor->GetActiveMainTab();

        // 現在のマップファイル名
        auto mapEditor = editor->GetMapEditor();
        if (mapEditor && strlen(mapEditor->GetStageFilename()) > 0) {
            j["currentMapFile"] = mapEditor->GetStageFilename();
        } else {
            j["currentMapFile"] = editor->GetStageFilename();
        }

        // 現在の3Dモデル配置JSONファイルパス
        auto model3DEditor = editor->GetModel3DEditor();
        if (model3DEditor && model3DEditor->GetContext()) {
            j["currentPlacedModelsFile"] = model3DEditor->GetContext()->GetCurrentFilePath();
        }

        // 現在のライティングJSONファイルパス
        auto lightEditor = editor->GetLightEditor();
        if (lightEditor) {
            j["currentLightingFile"] = lightEditor->GetCurrentFilePath();
        }

        // 現在のポストエフェクトJSONファイルパス
        auto postEffectEditor = editor->GetPostEffectEditor();
        if (postEffectEditor) {
            j["currentPostEffectFile"] = postEffectEditor->GetCurrentFilePath();
        }

        // 各ウィンドウの開閉状態 (EditorLayoutManager から取得)
        auto layoutManager = editor->GetLayoutManager();
        if (layoutManager) {
            const auto &vis = layoutManager->GetVisibility();
            nlohmann::json winObj;
            winObj["showInspector"] = vis.showInspector;
            winObj["showHierarchy"] = vis.showHierarchy;
            winObj["showGameView"] = vis.showGameView;
            winObj["showPostEffect"] = vis.showPostEffect;
            winObj["showSpriteAnimation"] = vis.showSpriteAnimation;
            winObj["showMapEditor"] = vis.showMapEditor;
            winObj["showMapSettings"] = vis.showMapSettings;
            winObj["showReplayEditor"] = vis.showReplayEditor;
            winObj["showAnimEditor"] = vis.showAnimEditor;
            winObj["showLightEditor"] = vis.showLightEditor;
            winObj["showSpotLightPanel"] = vis.showSpotLightPanel;
            winObj["showModelPlacementEditor"] = vis.showModelPlacement;
            winObj["showModelPalette"] = vis.showModelPalette;
            if (mapEditor && mapEditor->GetContext()) {
                winObj["showMapGrid"] = mapEditor->GetContext()->IsShowGrid();
            }
            j["windows"] = winObj;
        }

        ofs << j.dump(4) << std::endl;
        ofs.close();
    }
}

void EditorConfigManager::LoadSceneConfig(EditorManager *editor) {
    if (!editor)
        return;

    std::ifstream ifs("resources/json/local/editor_config.json");
    if (!ifs.is_open())
        return;

    try {
        nlohmann::json j;
        ifs >> j;
        ifs.close();

        if (j.contains("currentScene") && j["currentScene"].is_string()) {
            std::string sceneName = j["currentScene"].get<std::string>();
            editor->SetCurrentSceneType(SceneFactory::GetSceneTypeFromName(sceneName));
        }

        auto dxCommon = DirectXCommon::GetInstance();
        if (dxCommon) {
            if (j.contains("isOutlineEnabled") && j["isOutlineEnabled"].is_boolean()) {
                dxCommon->SetOutlineEnabled(j["isOutlineEnabled"].get<bool>());
            }
            if (j.contains("isDepthBasedOutlineEnabled") && j["isDepthBasedOutlineEnabled"].is_boolean()) {
                dxCommon->SetDepthBasedOutlineEnabled(j["isDepthBasedOutlineEnabled"].get<bool>());
            } else {
                dxCommon->SetDepthBasedOutlineEnabled(true);
            }
            if (j.contains("outlineThickness") && j["outlineThickness"].is_number()) {
                dxCommon->SetOutlineThickness(j["outlineThickness"].get<float>());
            }
        }

        if (j.contains("isColliderWireEnabled") && j["isColliderWireEnabled"].is_boolean()) {
            CollisionManager::GetInstance()->SetDebugDrawEnabled(j["isColliderWireEnabled"].get<bool>());
        }
        if (j.contains("isColliderDepthTestEnabled") && j["isColliderDepthTestEnabled"].is_boolean()) {
            CollisionManager::GetInstance()->SetDepthTestEnabled(j["isColliderDepthTestEnabled"].get<bool>());
        }

        if (j.contains("activeMainTab") && j["activeMainTab"].is_string()) {
            editor->SetActiveMainTab(j["activeMainTab"].get<std::string>());
        }

        if (j.contains("currentMapFile") && j["currentMapFile"].is_string()) {
            std::string mapFile = j["currentMapFile"].get<std::string>();
            if (!mapFile.empty()) {
                editor->SetStageFilename(mapFile.c_str());
                if (editor->GetMapEditor()) {
                    editor->GetMapEditor()->SetStageFilename(mapFile.c_str());
                }
            }
        }

        if (j.contains("currentPlacedModelsFile") && j["currentPlacedModelsFile"].is_string()) {
            std::string modelFile = j["currentPlacedModelsFile"].get<std::string>();
            auto model3DEditor = editor->GetModel3DEditor();
            if (!modelFile.empty() && model3DEditor && model3DEditor->GetContext()) {
                model3DEditor->GetContext()->SetCurrentFilePath(modelFile);
                if (std::filesystem::exists(model3DEditor->GetContext()->GetCurrentFilePath())) {
                    model3DEditor->GetContext()->LoadFromFile(model3DEditor->GetContext()->GetCurrentFilePath());
                }
            }
        }

        if (j.contains("currentLightingFile") && j["currentLightingFile"].is_string()) {
            std::string lightFile = j["currentLightingFile"].get<std::string>();
            auto lightEditor = editor->GetLightEditor();
            if (!lightFile.empty() && lightEditor) {
                lightEditor->SetCurrentFilePath(lightFile);
                if (std::filesystem::exists(lightEditor->GetCurrentFilePath())) {
                    lightEditor->LoadFromFile(lightEditor->GetCurrentFilePath());
                }
            }
        }

        if (j.contains("currentPostEffectFile") && j["currentPostEffectFile"].is_string()) {
            std::string peFile = j["currentPostEffectFile"].get<std::string>();
            auto postEffectEditor = editor->GetPostEffectEditor();
            if (!peFile.empty() && postEffectEditor) {
                postEffectEditor->SetCurrentFilePath(peFile);
                if (std::filesystem::exists(postEffectEditor->GetCurrentFilePath())) {
                    postEffectEditor->LoadFromFile(postEffectEditor->GetCurrentFilePath());
                }
            }
        }

        auto layoutManager = editor->GetLayoutManager();
        if (layoutManager && j.contains("windows") && j["windows"].is_object()) {
            const auto &winObj = j["windows"];
            auto &vis = layoutManager->GetVisibility();
            if (winObj.contains("showInspector") && winObj["showInspector"].is_boolean())
                vis.showInspector = winObj["showInspector"].get<bool>();
            if (winObj.contains("showHierarchy") && winObj["showHierarchy"].is_boolean())
                vis.showHierarchy = winObj["showHierarchy"].get<bool>();
            if (winObj.contains("showGameView") && winObj["showGameView"].is_boolean())
                vis.showGameView = winObj["showGameView"].get<bool>();
            if (winObj.contains("showPostEffect") && winObj["showPostEffect"].is_boolean())
                vis.showPostEffect = winObj["showPostEffect"].get<bool>();
            if (winObj.contains("showSpriteAnimation") && winObj["showSpriteAnimation"].is_boolean())
                vis.showSpriteAnimation = winObj["showSpriteAnimation"].get<bool>();
            if (winObj.contains("showMapEditor") && winObj["showMapEditor"].is_boolean())
                vis.showMapEditor = winObj["showMapEditor"].get<bool>();
            if (winObj.contains("showMapSettings") && winObj["showMapSettings"].is_boolean())
                vis.showMapSettings = winObj["showMapSettings"].get<bool>();
            if (winObj.contains("showReplayEditor") && winObj["showReplayEditor"].is_boolean())
                vis.showReplayEditor = winObj["showReplayEditor"].get<bool>();
            if (winObj.contains("showAnimEditor") && winObj["showAnimEditor"].is_boolean())
                vis.showAnimEditor = winObj["showAnimEditor"].get<bool>();
            if (winObj.contains("showLightEditor") && winObj["showLightEditor"].is_boolean())
                vis.showLightEditor = winObj["showLightEditor"].get<bool>();
            if (winObj.contains("showSpotLightPanel") && winObj["showSpotLightPanel"].is_boolean())
                vis.showSpotLightPanel = winObj["showSpotLightPanel"].get<bool>();
            if (winObj.contains("showModelPlacementEditor") && winObj["showModelPlacementEditor"].is_boolean())
                vis.showModelPlacement = winObj["showModelPlacementEditor"].get<bool>();
            if (winObj.contains("showModelPalette") && winObj["showModelPalette"].is_boolean())
                vis.showModelPalette = winObj["showModelPalette"].get<bool>();

            auto mapEditor = editor->GetMapEditor();
            if (winObj.contains("showMapGrid") && winObj["showMapGrid"].is_boolean() && mapEditor && mapEditor->GetContext()) {
                mapEditor->GetContext()->SetShowGrid(winObj["showMapGrid"].get<bool>());
            }
        }
    } catch (...) {
        // パースエラー時はスキップ
    }
}

void EditorConfigManager::SaveLightingConfig(ModelCommon *modelCommon, EditorManager *editor) {
    if (editor && editor->GetLightEditor()) {
        editor->GetLightEditor()->SaveLightingConfig(modelCommon);
    }
}

void EditorConfigManager::LoadLightingConfig(ModelCommon *modelCommon, EditorManager *editor) {
    if (editor && editor->GetLightEditor()) {
        editor->GetLightEditor()->LoadLightingConfig(modelCommon);
    }
}

void EditorConfigManager::LoadPlacedModelsForScene(IScene *scene) {
    if (!scene)
        return;
    Model3DEditorContext::GetInstance()->LoadLevelData(scene->GetLevelDataJsonPath());
}

void EditorConfigManager::InitializeMapOnStart(EditorManager *editor, SceneManager *sceneManager, GameCamera *gameCamera, DebugCamera *debugCamera) {
    if (!editor || !sceneManager || editor->isAStarPosInitialized_) return;

    if (editor->animationEditor_ && editor->animationEditor_->IsAnimScenePushed()) {
        sceneManager->PopScene();
        editor->animationEditor_->SetAnimScenePushed(false);
    }
    if (editor->gpuParticleEditor_ && editor->gpuParticleEditor_->IsParticleScenePushed()) {
        sceneManager->PopScene();
        editor->gpuParticleEditor_->SetParticleScenePushed(false);
    }

    IScene* activeScene = sceneManager->GetCurrentScene();
    if (!activeScene || !activeScene->GetMapChip()) {
        if (editor->currentSceneType_ == SceneType::kGame) {
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
            sceneManager->ProcessSceneTransition();
            activeScene = sceneManager->GetCurrentScene();
        }
    }

    if (activeScene && activeScene->GetMapChip()) {
        MapChip2D* mapChip = activeScene->GetMapChip();
        const std::string& sceneMapPath = mapChip->GetCurrentFilePath();
        bool loaded = !sceneMapPath.empty() &&
                      sceneMapPath.find("temp_play_map") == std::string::npos &&
                      std::filesystem::exists(sceneMapPath);
        if (!loaded) {
            const char* currentStageName = editor->mapEditor_ ? editor->mapEditor_->GetStageFilename() : "map_data.txt";
            loaded = mapChip->LoadFromStageName(currentStageName);
            if (loaded) {
                StageGifManager::GetInstance()->LoadForStage(currentStageName);
            }
        }
        if (!loaded) {
            if (editor->mapEditor_) editor->mapEditor_->SetStageFilename("map_data.txt");
            loaded = mapChip->LoadFromStageName("map_data.txt");
            if (!loaded) {
                if (!mapChip->LoadFromFile("resources/json/shared/Map/map_data.json")) {
                    mapChip->BuildMap();
                    mapChip->GenerateDefaultRooms();
                }
            }
            StageGifManager::GetInstance()->LoadForStage(editor->mapEditor_ ? editor->mapEditor_->GetStageFilename() : "map_data.txt");
            editor->SaveSceneConfig();
        }
        if (editor->mapEditor_) {
            editor->mapEditor_->GetContext()->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
            editor->mapEditor_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
        }
        if (activeScene) {
            activeScene->UpdateEditor();
        }
        if (gameCamera && debugCamera) {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
        }
        editor->isAStarPosInitialized_ = true;
    }
}

void EditorConfigManager::RestoreMapOnReset(EditorManager *editor, SceneManager *sceneManager, GameCamera *gameCamera, DebugCamera *debugCamera) {
    if (!editor || !sceneManager || !editor->sceneJustReset_) return;

    IScene* activeScene = sceneManager->GetCurrentScene();
    if (activeScene) {
        MapChip2D* mapChip = activeScene->GetMapChip();
        if (mapChip) {
            if (editor->loadMapDataStrNextFrame_) {
                mapChip->LoadFromString(editor->mapDataStrToLoad_);
                mapChip->SetRooms(editor->savedRoomsForPlay_);
                editor->loadMapDataStrNextFrame_ = false;
            } else {
                const char* curStage = editor->mapEditor_ ? editor->mapEditor_->GetStageFilename() : "map_data.txt";
                if (!mapChip->LoadFromStageName(curStage)) {
                    if (editor->mapEditor_) editor->mapEditor_->SetStageFilename("map_data.txt");
                    if (!mapChip->LoadFromStageName("map_data.txt")) {
                        if (!mapChip->LoadFromFile("resources/json/shared/Map/map_data.json")) {
                            mapChip->BuildMap();
                            mapChip->GenerateDefaultRooms();
                        }
                    }
                }
            }
            if (editor->mapEditor_) {
                editor->mapEditor_->GetContext()->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                editor->mapEditor_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
                StageGifManager::GetInstance()->LoadForStage(editor->mapEditor_->GetStageFilename());
            }
            if (activeScene) {
                activeScene->UpdateEditor();
            }
            if (gameCamera && debugCamera) {
                debugCamera->SetTranslation(gameCamera->GetTranslation());
                debugCamera->SetRotation(gameCamera->GetRotation());
            }
        }
    }
    editor->sceneJustReset_ = false;
}

#endif // USE_IMGUI