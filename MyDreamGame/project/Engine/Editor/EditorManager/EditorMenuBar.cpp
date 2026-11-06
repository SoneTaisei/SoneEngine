#ifdef USE_IMGUI
#include "EditorMenuBar.h"
#include "EditorManager.h"
#include "EditorLayoutManager.h"
#include "Core/TimeManager.h"
#include "Graphics/Camera/GameCamera.h"
#include "Graphics/Camera/DebugCamera.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneFactory.h"
#include "GameObject/WorldGif.h"
#include "Game2D/MapChip2D.h"
#include "Editor/Replay/ReplayManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstring>

void EditorMenuBar::Draw(EditorManager* em, SceneManager* sceneManager, ModelCommon* modelCommon,
                         GameCamera* gameCamera, DebugCamera* debugCamera) {
    (void)modelCommon;
    if (!em) return;

    if (ImGui::BeginMainMenuBar()) {
        // PLAY / STOP ボタン
        if (!em->isPlaying_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
            if (ImGui::Button("再生 (PLAY)")) {
                em->isPlaying_ = true;
                em->isPaused_ = false;
                em->useDebugCamera_ = false;
                ImGui::SetWindowFocus("ゲームビュー");

                // --- マップを一時保存（Stop時に復元するため） ---
                if (sceneManager && sceneManager->GetCurrentScene()) {
                    MapChip2D* mapChip = sceneManager->GetCurrentScene()->GetMapChip();
                    if (mapChip) {
                        em->mapDataStrToLoad_ = mapChip->GetMapDataAsString();
                        em->savedRoomsForPlay_ = mapChip->GetRooms();
                        mapChip->SaveInitialMapState();
                        mapChip->ResetBlocks();
                    }
                }
                // GIFの配置状態も自動保存
                StageGifManager::GetInstance()->SaveForStage(StageGifManager::GetInstance()->GetCurrentStageName());
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
            if (ImGui::Button("停止 (STOP)")) {
                em->isPlaying_ = false;
                em->isPaused_ = false;
                em->useDebugCamera_ = true;
                if (gameCamera && debugCamera) {
                    debugCamera->SetTranslation(gameCamera->GetTranslation());
                    debugCamera->SetRotation(gameCamera->GetRotation());
                }

                // リプレイ再生中であればそれも同時に停止させる
                if (ReplayManager::GetInstance()->IsPlaying()) {
                    ReplayManager::GetInstance()->StopPlayback();
                }

                if (sceneManager && sceneManager->GetCurrentScene()) {
                    MapChip2D* mapChip = sceneManager->GetCurrentScene()->GetMapChip();
                    if (mapChip) {
                        mapChip->ResetBlocks();
                    }
                }

                ImGui::SetWindowFocus("マップチップ画面");

                // --- Stop時に一時保存したマップを復元するフラグを立てる ---
                em->loadMapDataStrNextFrame_ = true;
                em->sceneJustReset_ = true;
            }
            ImGui::PopStyleColor(3);

            // プレイ中の一時停止 (PAUSE / RESUME) ボタン
            ImGui::SameLine();
            if (em->isPaused_) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.6f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.4f, 0.0f, 1.0f));
                if (ImGui::Button("再開 (RESUME)")) {
                    em->isPaused_ = false;
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.0f, 1.0f));
                if (ImGui::Button("一時停止 (PAUSE)")) {
                    em->isPaused_ = true;
                }
                ImGui::PopStyleColor(3);
            }
        }

        // マップエディター / ゲーム画面 切り替えボタン (F2)
        ImGui::SameLine();
        if (em->activeMainTab_ == "マップチップ画面") {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.7f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.55f, 0.8f, 1.0f));
            if (ImGui::Button("ゲーム画面へ (F2)")) {
                em->FocusGameView();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 0.35f, 1.0f));
            if (ImGui::Button("マップエディター (F2)")) {
                em->FocusMapEditor();
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // リプレイ用 PLAY / STOP ボタン
        ReplayManager* replayMgr = ReplayManager::GetInstance();
        if (!replayMgr->IsPlaying()) {
            bool hasData = (replayMgr->GetCurrentReplay().totalFrames > 0) || !replayMgr->GetHistory().empty();
            if (hasData) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.45f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.55f, 0.9f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.35f, 0.7f, 1.0f));
                if (ImGui::Button("リプレイ再生 (REPLAY)")) {
                    if (replayMgr->GetCurrentReplay().totalFrames == 0 && !replayMgr->GetHistory().empty()) {
                        replayMgr->StartPlayback(0);
                    } else {
                        replayMgr->StartPlayback();
                    }
                    em->useDebugCamera_ = false;
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("リプレイ再生 (REPLAY)");
                ImGui::EndDisabled();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.4f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.5f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.3f, 0.0f, 1.0f));
            if (ImGui::Button("リプレイ停止 (STOP REPLAY)")) {
                replayMgr->StopPlayback();
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));
            if (ImGui::Button("操作切替 (TAKEOVER)")) {
                replayMgr->TakeoverPlayback();
                em->takeoverCountdown_ = 1.0f;
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // Debug Camera チェックボックス
        ImGui::Checkbox("デバッグカメラ", &em->useDebugCamera_);

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // シーン切替コンボ
        ImGui::Text("シーン:");
        ImGui::SetNextItemWidth(130.0f);
        {
            int sceneIndex = static_cast<int>(em->currentSceneType_);
            if (ImGui::Combo("##Scene", &sceneIndex, kSceneTypeNames, static_cast<int>(SceneType::kCount))) {
                SceneType newType = static_cast<SceneType>(sceneIndex);
                if (newType != em->currentSceneType_) {
                    em->currentSceneType_ = newType;
                    sceneManager->ChangeScene(SceneFactory::CreateScene(em->currentSceneType_));
                    em->selectedGameObject_ = nullptr;
                    em->selectedObject_ = nullptr;
                    em->selectedParticle_ = nullptr;
                    em->selectedPrimitive_ = nullptr;
                    em->SaveSceneConfig();
                }
            }
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // Window開閉制御メニュー
        if (ImGui::BeginMenu("ウィンドウ")) {
            if (ImGui::MenuItem("インスペクター", nullptr, &em->showInspector_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("ヒエラルキー", nullptr, &em->showHierarchy_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("ゲームビュー", nullptr, &em->showGameView_)) {
                if (em->showGameView_) {
                    em->activeMainTab_ = "ゲームビュー";
                    em->focusActiveTabCountdown_ = 5;
                }
                em->SaveSceneConfig();
            }
            if (ImGui::MenuItem("ポストエフェクト", nullptr, &em->showPostEffect_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("スプライトアニメーション", nullptr, &em->showSpriteAnimation_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("マップチップ画面", nullptr, &em->showMapEditor_)) {
                if (em->showMapEditor_) {
                    em->activeMainTab_ = "マップチップ画面";
                    em->focusActiveTabCountdown_ = 5;
                }
                em->SaveSceneConfig();
            }
            if (ImGui::MenuItem("マップ設定", nullptr, &em->showMapSettings_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("リプレイエディター", nullptr, &em->showReplayEditor_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("アニメーションエディター", nullptr, &em->showAnimEditor_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("ライトエディター", nullptr, &em->showLightEditor_)) {
                if (em->showLightEditor_) {
                    em->activeMainTab_ = "ライトエディター";
                    em->currentMode_ = EditorManager::EditorMode::Light;
                    em->showSpotLightPanel_ = true;
                    em->focusActiveTabCountdown_ = 5;
                    em->focusSpotLightTabCountdown_ = 5;
                }
                em->SaveSceneConfig();
            }
            if (ImGui::MenuItem("スポットライト", nullptr, &em->showSpotLightPanel_)) {
                if (em->showSpotLightPanel_) {
                    em->focusSpotLightTabCountdown_ = 5;
                }
                em->SaveSceneConfig();
            }
            if (ImGui::MenuItem("3Dモデル配置", nullptr, &em->showModelPlacementEditor_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("3Dモデルパレット", nullptr, &em->showModelPalette_)) { em->SaveSceneConfig(); }
            if (ImGui::MenuItem("パラメータ調整 (ParameterManager)", nullptr, &em->showParameterManager_)) { em->SaveSceneConfig(); }
            ImGui::Separator();
            if (ImGui::BeginMenu("レイアウトプリセット")) {
                const auto& presets = em->layoutPresets_;
                if (presets.empty()) {
                    ImGui::TextDisabled("  (保存されたプリセットはありません)");
                } else {
                    for (const auto& preset : presets) {
                        if (ImGui::MenuItem(preset.name.c_str())) {
                            em->ApplyLayoutPreset(preset.name);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("現在のレイアウトを保存...")) {
                    em->showSavePresetWindow_ = true;
                    strcpy_s(em->newPresetNameBuf_, "");
                }
                if (ImGui::BeginMenu("プリセットを削除")) {
                    if (presets.empty()) {
                        ImGui::TextDisabled("  (削除できるプリセットはありません)");
                    } else {
                        std::string toDeleteFromMenu = "";
                        for (const auto& preset : presets) {
                            std::string delLabel = "削除: " + preset.name;
                            if (ImGui::MenuItem(delLabel.c_str())) {
                                toDeleteFromMenu = preset.name;
                            }
                        }
                        if (!toDeleteFromMenu.empty()) {
                            em->DeleteLayoutPreset(toDeleteFromMenu);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("レイアウトをリセット")) {
                em->ApplyDefaultLayout();
            }
            ImGui::EndMenu();
        }

        // インスペクターが閉じている場合に表示する復元ボタン
        if (!em->showInspector_) {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.45f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.35f, 0.7f, 1.0f));
            if (ImGui::Button("インスペクターを開く")) {
                em->showInspector_ = true;
            }
            ImGui::PopStyleColor(3);
        }

        // ステータスメッセージ表示（あれば右側に通知）
        if (em->presetStatusMessageTimer_ > 0.0f && !em->presetStatusMessage_.empty()) {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", em->presetStatusMessage_.c_str());
            float dt = ImGui::GetIO().DeltaTime > 0.0f ? ImGui::GetIO().DeltaTime : 0.016f;
            em->presetStatusMessageTimer_ -= dt;
            if (em->presetStatusMessageTimer_ <= 0.0f) {
                em->presetStatusMessage_.clear();
            }
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorMenuBar::SetupDockspace(EditorManager* em) {
    if (!em) return;

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    static bool first_time = true;
    if (first_time) {
        first_time = false;

        bool hasIniFile = false;
        bool resetLayout = false;
        if (ImGui::GetIO().IniFilename) {
            FILE* f = nullptr;
            fopen_s(&f, ImGui::GetIO().IniFilename, "r");
            if (f) {
                hasIniFile = true;
                bool hasReplayEditor = false;
                bool hasStageSelectEditor = false;
                bool hasDopeSheet = false;
                bool hasLightEditor = false;
                bool hasSpotLight = false;
                bool hasModelPlacement = false;
                bool hasModelPalette = false;
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), f)) {
                    if (strstr(buffer, "リプレイエディター")) hasReplayEditor = true;
                    if (strstr(buffer, "ステージセレクトエディター")) hasStageSelectEditor = true;
                    if (strstr(buffer, "ドープシート")) hasDopeSheet = true;
                    if (strstr(buffer, "ライトエディター")) hasLightEditor = true;
                    if (strstr(buffer, "スポットライト")) hasSpotLight = true;
                }
                if (!hasReplayEditor || !hasStageSelectEditor || !hasDopeSheet || !hasLightEditor || !hasSpotLight) {
                    if (strstr(buffer, "3Dモデル配置")) hasModelPlacement = true;
                    if (strstr(buffer, "3Dモデルパレット")) hasModelPalette = true;
                }
                if (!hasReplayEditor || !hasStageSelectEditor || !hasDopeSheet || !hasModelPlacement || !hasModelPalette) {
                    resetLayout = true;
                }
                fclose(f);
            }
        }

        if (resetLayout || !hasIniFile) {
            em->ApplyDefaultLayout();
        }
    }
}
#endif
