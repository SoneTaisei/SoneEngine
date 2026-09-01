#ifdef USE_IMGUI
#include "MapEditorSettings.h"
#include "MapEditorContext.h"
#include "MapEditorPalette.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Game2D/MapChip2D.h"
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
                        if (onSaveSceneConfig) {
                            onSaveSceneConfig();
                        }
                    }
                }

                ImGui::Spacing();

                // ルーム編集モード
                bool roomEdit = context_->IsRoomEditMode();
                if (ImGui::Checkbox("ルーム編集モード", &roomEdit)) {
                    context_->SetRoomEditMode(roomEdit);
                }
                if (context_->IsRoomEditMode()) {
                    ImGui::Text("左ドラッグ: マス目にスナップして作成・移動・リサイズ");
                    ImGui::Text("右ドラッグ: スナップなしで作成・移動・リサイズ");
                    ImGui::Text("Ctrl + クリック: ルームの削除");
                }

                ImGui::Separator();

                ImGui::Text("マップサイズ設定 (1画面＝ 幅:20, 高さ:11)");
                ImGui::TextDisabled("※ 画面を増やしたい場合はサイズを広げてください");

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputInt("Width", context_->GetInputWidthPtr());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputInt("Height", context_->GetInputHeightPtr());
                ImGui::SameLine();
                if (ImGui::Button("Apply Size")) {
                    context_->BeginMapHistoryCapture(mapChip);
                    int w = context_->GetInputWidth();
                    int h = context_->GetInputHeight();
                    if (w < 1) w = 1;
                    if (h < 1) h = 1;
                    context_->SetInputSize(w, h);
                    mapChip->Resize(w, h);
                    context_->EndMapHistoryCapture(mapChip);
                }

                ImGui::Separator();

                // 操作ボタン
                if (ImGui::Button("保存")) {
                    mapChip->SaveToFile(context_->GetFullFilePath(context_->GetStageFilename()));
                    if (onSaveSceneConfig) {
                        onSaveSceneConfig();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("クリア")) {
                    mapChip->ClearMap();
                }
                ImGui::SameLine();
                if (ImGui::Button("初期化")) {
                    mapChip->ResetMap();
                    context_->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                    context_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
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
