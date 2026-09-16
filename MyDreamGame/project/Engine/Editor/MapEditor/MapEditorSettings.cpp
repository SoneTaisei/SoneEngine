#ifdef USE_IMGUI
#include "MapEditorSettings.h"
#include "MapEditorContext.h"
#include "MapEditorPalette.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Game2D/MapChip2D.h"
#include "Editor/EditorManager.h"
#include <filesystem>
#include <vector>
#include <string>

MapEditorSettings::MapEditorSettings(MapEditorContext* context, MapEditorPalette* palette)
    : context_(context), palette_(palette) {
}

void MapEditorSettings::Draw(
    SceneManager* sceneManager,
    bool& showMapSettings,
    const std::function<void()>& onSaveSceneConfig,
    const std::function<void()>& onSelectionCleared
) {
    if (!context_) return;

    if (ImGui::Begin("マップ設定", &showMapSettings)) {
        IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
        if (activeScene) {
            MapChip2D* mapChip = activeScene->GetMapChip();
            if (mapChip) {
                if (context_->GetInputWidth() == -1) {
                    context_->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                }

                // json ディレクトリ内の .txt ファイルを自動走査
                std::vector<std::string> stageFiles;
                try {
                    if (std::filesystem::exists("resources/json/shared/MapData")) {
                        for (const auto& entry : std::filesystem::directory_iterator("resources/json/shared/MapData")) {
                            if (entry.is_regular_file()) {
                                std::string filename = entry.path().filename().string();
                                if (filename.length() >= 4) {
                                    std::string ext = filename.substr(filename.length() - 4);
                                    if (ext == ".txt" || ext == ".TXT") {
                                        // _bounds.txt は除外
                                        if (filename.length() < 11 || (filename.substr(filename.length() - 11) != "_bounds.txt" && filename.substr(filename.length() - 11) != "_bounds.TXT")) {
                                            stageFiles.push_back(filename);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } catch (...) {}

                // 既存のマップファイルを選択するコンボボックス
                if (!stageFiles.empty()) {
                    static int selectedFileIndex = -1;
                    std::string currentFile = context_->GetStageFilename();
                    if (currentFile.length() < 4 || (currentFile.compare(currentFile.length() - 4, 4, ".txt") != 0 && currentFile.compare(currentFile.length() - 4, 4, ".TXT") != 0)) {
                        currentFile += ".txt";
                    }

                    selectedFileIndex = -1;
                    for (int i = 0; i < static_cast<int>(stageFiles.size()); ++i) {
                        if (stageFiles[i] == currentFile) {
                            selectedFileIndex = i;
                            break;
                        }
                    }

                    std::string comboPreview = (selectedFileIndex != -1) ? stageFiles[selectedFileIndex] : "既存のマップを選択...";
                    if (ImGui::BeginCombo("マップファイルを選択", comboPreview.c_str())) {
                        for (int i = 0; i < static_cast<int>(stageFiles.size()); ++i) {
                            bool isSelected = (selectedFileIndex == i);
                            if (ImGui::Selectable(stageFiles[i].c_str(), isSelected)) {
                                context_->SetStageFilename(stageFiles[i]);
                                selectedFileIndex = i;

                                if (mapChip->LoadFromFile(context_->GetFullFilePath(context_->GetStageFilename()))) {
                                    context_->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                                    context_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
                                    if (EditorManager::IsPlaying()) {
                                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                                    }
                                    if (onSaveSceneConfig) {
                                        onSaveSceneConfig();
                                    }
                                }
                            }
                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                // ファイル名入力 (Enterキーでロード)
                if (ImGui::InputText("ファイル名", context_->GetStageFilenameBuffer(), 128, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (mapChip->LoadFromFile(context_->GetFullFilePath(context_->GetStageFilename()))) {
                        context_->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                        context_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
                        if (EditorManager::IsPlaying()) {
                            EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                        }
                        if (onSaveSceneConfig) {
                            onSaveSceneConfig();
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ルーム編集モード
                bool roomEdit = context_->IsRoomEditMode();
                if (ImGui::Checkbox("ルーム編集モード (カメラ・ステージ範囲)", &roomEdit)) {
                    context_->SetRoomEditMode(roomEdit);
                }
                if (context_->IsRoomEditMode()) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "【ルーム編集の操作方法】");
                    ImGui::BulletText("左ドラッグ: マス目にスナップして作成・移動・リサイズ");
                    ImGui::BulletText("右ドラッグ: スナップなしで作成・移動・リサイズ");
                    ImGui::BulletText("Ctrl + クリック: ルームの削除");
                    ImGui::Spacing();
                }

                ImGui::Separator();

                // マップサイズ設定
                ImGui::Text("マップサイズ設定 (1画面 ＝ 幅:20, 高さ:11)");
                ImGui::TextDisabled("※ ステージを広げたい場合はサイズを拡張してください (Ctrl+ZでUndo可能)");

                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputInt("幅 (Width)", context_->GetInputWidthPtr());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputInt("高さ (Height)", context_->GetInputHeightPtr());
                ImGui::SameLine();
                if (ImGui::Button("サイズ適用 (Apply)")) {
                    context_->BeginMapHistoryCapture(mapChip);
                    int w = context_->GetInputWidth();
                    int h = context_->GetInputHeight();
                    if (w < 1) w = 1;
                    if (h < 1) h = 1;
                    context_->SetInputSize(w, h);
                    mapChip->Resize(w, h);
                    context_->EndMapHistoryCapture(mapChip);
                    if (EditorManager::IsPlaying()) {
                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                    }
                }

                // クイック拡張ボタン
                ImGui::Text("クイック拡張:");
                ImGui::SameLine();
                if (ImGui::Button("+1画面 右へ (幅+20)")) {
                    context_->BeginMapHistoryCapture(mapChip);
                    int w = mapChip->GetWidth() + 20;
                    int h = mapChip->GetHeight();
                    context_->SetInputSize(w, h);
                    mapChip->Resize(w, h);
                    context_->EndMapHistoryCapture(mapChip);
                    if (EditorManager::IsPlaying()) {
                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("+1画面 上へ (高さ+11)")) {
                    context_->BeginMapHistoryCapture(mapChip);
                    int w = mapChip->GetWidth();
                    int h = mapChip->GetHeight() + 11;
                    context_->SetInputSize(w, h);
                    mapChip->Resize(w, h);
                    context_->EndMapHistoryCapture(mapChip);
                    if (EditorManager::IsPlaying()) {
                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                    }
                }

                // ルームをマップ全体に合わせるボタン
                if (ImGui::Button("ルームを現在のマップ全体に合わせる")) {
                    context_->BeginRoomHistoryCapture(mapChip);
                    mapChip->GenerateDefaultRooms();
                    context_->EndRoomHistoryCapture(mapChip);
                    if (EditorManager::IsPlaying()) {
                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("マップサイズを広げた際、カメラのスクロール・追従範囲もマップ全体に拡大します");
                }

                ImGui::Separator();
                ImGui::Spacing();

                // 表示設定
                bool showGrid = context_->IsShowGrid();
                if (ImGui::Checkbox("グリッド線を表示", &showGrid)) {
                    context_->SetShowGrid(showGrid);
                }

                ImGui::Spacing();

                // 操作ボタン
                if (ImGui::Button("保存")) {
                    mapChip->SaveToFile(context_->GetFullFilePath(context_->GetStageFilename()));
                    if (onSaveSceneConfig) {
                        onSaveSceneConfig();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("クリア")) {
                    context_->BeginMapHistoryCapture(mapChip);
                    mapChip->ClearMap();
                    context_->EndMapHistoryCapture(mapChip);
                    if (EditorManager::IsPlaying()) {
                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("初期化")) {
                    context_->BeginMapHistoryCapture(mapChip);
                    context_->BeginRoomHistoryCapture(mapChip);
                    mapChip->ResetMap();
                    context_->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                    context_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
                    context_->EndMapHistoryCapture(mapChip);
                    context_->EndRoomHistoryCapture(mapChip);
                    if (EditorManager::IsPlaying()) {
                        EditorManager::GetInstance()->SyncPlayMapData(mapChip);
                    }
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("マップを削除")) {
                    ImGui::OpenPopup("DeleteMapConfirmPopup");
                }
                ImGui::PopStyleColor(3);

                // マップ削除確認ポップアップ
                if (ImGui::BeginPopupModal("DeleteMapConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    std::string targetFilePath = context_->GetFullFilePath(context_->GetStageFilename());
                    ImGui::Text("本当にマップファイル '%s' を削除しますか？", context_->GetStageFilename());
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "※この操作は取り消せません。");
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Button("削除", ImVec2(120, 0))) {
                        std::error_code ec;
                        std::filesystem::remove(targetFilePath, ec);

                        std::filesystem::path mapPath(targetFilePath);
                        std::string stem = mapPath.stem().string();
                        std::string boundsPath = "resources/json/shared/MapBounds/" + stem + "_bounds.txt";
                        std::filesystem::remove(boundsPath, ec);

                        // 旧パスの bounds ファイルも存在すれば削除
                        std::string oldBoundsPath = targetFilePath;
                        size_t lastDot = oldBoundsPath.find_last_of(".");
                        if (lastDot != std::string::npos) {
                            oldBoundsPath = oldBoundsPath.substr(0, lastDot) + "_bounds.txt";
                        } else {
                            oldBoundsPath += "_bounds.txt";
                        }
                        std::filesystem::remove(oldBoundsPath, ec);

                        context_->SetStageFilename("map_data.txt");
                        if (!mapChip->LoadFromStageName(context_->GetStageFilename())) {
                            if (!mapChip->LoadFromFile("resources/json/shared/Map/map_data.json")) {
                                mapChip->ResetMap();
                            }
                        }
                        context_->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                        context_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
                        if (onSaveSceneConfig) {
                            onSaveSceneConfig();
                        }

                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::Spacing();
                ImGui::Separator();

                // パレットの描画
                if (palette_) {
                    palette_->Draw(sceneManager, onSelectionCleared);
                }

            } else {
                ImGui::Text("現在のアクティブシーンは2Dマップ編集をサポートしていません。");
            }
        } else {
            ImGui::Text("アクティブなシーンがありません。");
        }
    }
    ImGui::End();
}
#endif
