#ifdef USE_IMGUI
#include "AnimationDopeSheet.h"
#include "AnimationEditorContext.h"
#include "Editor/EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/LogManager.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "GameObject/GameObject.h"
#include "Input/KeyboardInput.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/SrvManager.h"
#include "Scene/IScene.h"
#include "Scene/SceneManager.h"
#include "Scenes/AnimationPreviewScene.h"
#include "Component/TransformComponent.h"
#include "Component/AnimatorComponent.h"
#include "Core/Utility/Animation.h"
#include "Resource/Model/Model.h"
#include "Game2D/Player/Player2D.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

AnimationDopeSheet::AnimationDopeSheet() {
    Initialize();
}
void AnimationDopeSheet::Initialize() {
}

void AnimationDopeSheet::DrawDopeSheetUI(SceneManager* sceneManager, AnimationEditorContext* context) {
    if (!context->GetAnimEditorInitialized()) {
        context->ScanAnimationFiles();
        if (!LoadAnimationFromJsonFile(context->GetEditingAnimation(), context->GetCurrentAnimFilePath())) {
            if (context->GetCurrentAnimFilePath().find("wall_climb") != std::string::npos) {
                context->GetEditingAnimation() = CreateDefaultWallClimbAnimation();
            } else if (context->GetCurrentAnimFilePath().find("air_dash") != std::string::npos) {
                context->GetEditingAnimation() = CreateDefaultAirDashAnimation();
            }
        }
        context->GetAnimEditorInitialized() = true;
    }

    if (context->GetAvailableAnimationFiles().empty()) {
        context->ScanAnimationFiles();
    }

    if (ImGui::Begin("ドープシート (タイムライン)", &context->GetShowAnimEditor(), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowAppearing()) {
            // Animation mode
            if (!context->GetIsAnimScenePushed()) {
                sceneManager->PushScene(std::make_unique<AnimationPreviewScene>());
                context->GetIsAnimScenePushed() = true;
                context->RefreshAnimationJointList(sceneManager);
            }
        }

        // ========================================================
        // 1. ヘッダーバー (Playback Controls & Actions)
        // ========================================================
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        // アクション / アニメーション切替 (ファイル一覧から選択 & 自動ロード)
        std::string currentStem = std::filesystem::path(context->GetCurrentAnimFilePath()).stem().string();
        std::string currentDisplay = currentStem;
        if (currentStem == "wall_climb_animation") currentDisplay = "壁つかまり (wall_climb)";
        else if (currentStem == "air_dash_animation") currentDisplay = "空中ダッシュ (air_dash)";

        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("##AnimSelectCombo", currentDisplay.c_str())) {
            for (const auto& filePath : context->GetAvailableAnimationFiles()) {
                std::string stem = std::filesystem::path(filePath).stem().string();
                std::string displayName = stem;
                if (stem == "wall_climb_animation") displayName = "壁つかまり (wall_climb)";
                else if (stem == "air_dash_animation") displayName = "空中ダッシュ (air_dash)";

                bool isSel = (context->GetCurrentAnimFilePath() == filePath);
                if (ImGui::Selectable(displayName.c_str(), isSel)) {
                    if (context->GetCurrentAnimFilePath() != filePath) {
                        context->GetCurrentAnimFilePath() = filePath;
                        if (!LoadAnimationFromJsonFile(context->GetEditingAnimation(), context->GetCurrentAnimFilePath())) {
                            if (context->GetCurrentAnimFilePath().find("wall_climb") != std::string::npos) {
                                context->GetEditingAnimation() = CreateDefaultWallClimbAnimation();
                            } else if (context->GetCurrentAnimFilePath().find("air_dash") != std::string::npos) {
                                context->GetEditingAnimation() = CreateDefaultAirDashAnimation();
                            }
                        }
                        context->GetAnimEditorTime() = 0.0f;
                        context->GetSelectedKeyIndex() = -1;
                        context->ClearAnimUndoRedo();
                        context->UpdateAnimationPosePreview(sceneManager);
                        LogManager::GetInstance()->AddLog(LogLevel::Info, "アニメーション読込: " + context->GetCurrentAnimFilePath());
                    }
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("編集対象のアニメーションを選択（切り替え時に自動読込）");

        ImGui::SameLine();
        // 上書き保存ボタン
        if (ImGui::Button("[Save] 上書き保存")) {
            SaveAnimationToJsonFile(context->GetEditingAnimation(), context->GetCurrentAnimFilePath());
            LogManager::GetInstance()->AddLog(LogLevel::Info, "アニメーション上書き保存: " + context->GetCurrentAnimFilePath());
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("現在のアニメーションファイル (%s) に上書き保存", context->GetCurrentAnimFilePath().c_str());

        ImGui::SameLine();
        // 名前をつけて保存ボタン
        if (ImGui::Button("[+] 名前をつけて保存")) {
            snprintf(context->GetNewAnimSaveNameBuf(), context->GetNewAnimSaveNameBufSize(), "%s_copy", currentStem.c_str());
            context->GetOpenSaveAnimModal() = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("新しい名前をつけてJSONファイルとして新規保存");

        // 名前をつけて保存モーダルダイアログ
        if (context->GetOpenSaveAnimModal()) {
            ImGui::OpenPopup("名前をつけて保存##AnimSaveModal");
            context->GetOpenSaveAnimModal() = false;
        }

        if (ImGui::BeginPopupModal("名前をつけて保存##AnimSaveModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("新しいアニメーションのファイル名を入力してください:");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(380.0f);
            ImGui::InputText("##NewAnimFileName", context->GetNewAnimSaveNameBuf(), context->GetNewAnimSaveNameBufSize());
            ImGui::TextDisabled("保存先: resources/json/shared/Player/%s.json", context->GetNewAnimSaveNameBuf());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("保存", ImVec2(120, 0))) {
                std::string inputName = context->GetNewAnimSaveNameBuf();
                if (!inputName.empty()) {
                    if (inputName.size() < 5 || inputName.substr(inputName.size() - 5) != ".json") {
                        inputName += ".json";
                    }
                    std::string fullPath = "resources/json/shared/Player/" + inputName;
                    SaveAnimationToJsonFile(context->GetEditingAnimation(), fullPath);
                    context->ScanAnimationFiles();
                    context->GetCurrentAnimFilePath() = fullPath;
                    LogManager::GetInstance()->AddLog(LogLevel::Info, "新規アニメーション保存: " + fullPath);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        // 削除ボタン
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("[-] 削除")) {
            context->GetOpenDeleteAnimModal() = true;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("現在選択されているアニメーションファイル (%s) を削除", context->GetCurrentAnimFilePath().c_str());

        // アニメーション削除確認モーダルダイアログ
        if (context->GetOpenDeleteAnimModal()) {
            ImGui::OpenPopup("アニメーションの削除確認##AnimDeleteModal");
            context->GetOpenDeleteAnimModal() = false;
        }

        if (ImGui::BeginPopupModal("アニメーションの削除確認##AnimDeleteModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("本当にこのアニメーションを削除しますか？");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "ファイル: %s", context->GetCurrentAnimFilePath().c_str());
            ImGui::TextDisabled("※ 削除したファイルは元に戻せません。");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
            if (ImGui::Button("削除", ImVec2(120, 0))) {
                std::string deletedFilePath = context->GetCurrentAnimFilePath();
                if (std::filesystem::exists(deletedFilePath)) {
                    std::error_code ec;
                    std::filesystem::remove(deletedFilePath, ec);
                }
                LogManager::GetInstance()->AddLog(LogLevel::Info, "アニメーション削除: " + deletedFilePath);

                context->ScanAnimationFiles();

                // 新しく利用可能なファイルから読み込み
                if (!context->GetAvailableAnimationFiles().empty()) {
                    context->GetCurrentAnimFilePath() = context->GetAvailableAnimationFiles()[0];
                    if (!LoadAnimationFromJsonFile(context->GetEditingAnimation(), context->GetCurrentAnimFilePath())) {
                        if (context->GetCurrentAnimFilePath().find("wall_climb") != std::string::npos) {
                            context->GetEditingAnimation() = CreateDefaultWallClimbAnimation();
                        } else if (context->GetCurrentAnimFilePath().find("air_dash") != std::string::npos) {
                            context->GetEditingAnimation() = CreateDefaultAirDashAnimation();
                        }
                    }
                } else {
                    context->GetEditingAnimation() = Animation{};
                }

                context->GetAnimEditorTime() = 0.0f;
                context->GetSelectedKeyIndex() = -1;
                context->ClearAnimUndoRedo();
                context->UpdateAnimationPosePreview(sceneManager);

                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 再生コントロールボタン群
        // 先頭へ (|<<)
        if (ImGui::Button("|<<", ImVec2(32, 0))) {
            context->GetAnimEditorTime() = 0.0f;
            context->GetAnimEditorPlaying() = false;
            context->GetTempOverrides().clear();
            context->UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("先頭フレームへ");

        ImGui::SameLine();
        // 1フレーム戻る (<)
        if (ImGui::Button("<", ImVec2(28, 0))) {
            context->GetAnimEditorTime() = (std::max)(0.0f, context->GetAnimEditorTime() - 1.0f / context->GetAnimEditorFps());
            context->GetTempOverrides().clear();
            context->UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム戻る");

        // 再生 / 一時停止 ([>] 再生 / [||] 停止)
        ImGui::SameLine();
        if (context->GetAnimEditorPlaying()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("[||] 停止", ImVec2(68, 0))) {
                context->GetAnimEditorPlaying() = false;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("[>] 再生", ImVec2(68, 0))) {
                context->GetAnimEditorPlaying() = true;
                context->GetTempOverrides().clear();
            }
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生 / 一時停止 (Space)");

        ImGui::SameLine();
        // 1フレーム進む (>)
        if (ImGui::Button(">", ImVec2(28, 0))) {
            context->GetAnimEditorTime() = (std::min)(context->GetEditingAnimation().duration, context->GetAnimEditorTime() + 1.0f / context->GetAnimEditorFps());
            context->GetTempOverrides().clear();
            context->UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム進む");

        ImGui::SameLine();
        // 末尾へ (>>|)
        if (ImGui::Button(">>|", ImVec2(32, 0))) {
            context->GetAnimEditorTime() = context->GetEditingAnimation().duration;
            context->GetAnimEditorPlaying() = false;
            context->GetTempOverrides().clear();
            context->UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("末尾フレームへ");

        ImGui::SameLine();
        // ループ再生トグル
        if (context->GetAnimEditorLoop()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("Loop: ON")) {
                context->GetAnimEditorLoop() = false;
            }
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Loop: OFF")) {
                context->GetAnimEditorLoop() = true;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("ループ再生の切り替え");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // キー挿入ボタン群
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.55f, 0.3f, 1.0f));
        if (ImGui::Button("[+] 全ボーンキー (Shift+I)")) {
            context->InsertAllJointsSRTKey(sceneManager);
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("全ボーンの現在のSRTポーズを一括キーフレーム登録 (Shift+I)");

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.65f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.45f, 0.05f, 1.0f));
        if (ImGui::Button("[+] 選択キー (I)")) {
            context->InsertSelectedJointSRTKey(sceneManager);
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("選択中ボーンのSRTキーフレームを登録 (I)");

        ImGui::PopStyleVar(2);

        ImGui::Separator();

        // ショートカットキー判定
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            auto io = ImGui::GetIO();
            if (io.KeyCtrl && !io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io.KeyShift) context->PerformAnimRedo(sceneManager);
                    else context->PerformAnimUndo(sceneManager);
                } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    context->PerformAnimRedo(sceneManager);
                }
            } else if (!io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                    context->GetAnimEditorPlaying() = !context->GetAnimEditorPlaying();
                    if (context->GetAnimEditorPlaying()) context->GetTempOverrides().clear();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_T, false)) context->GetGizmoMode() = 0; // Translate (移動)
                if (ImGui::IsKeyPressed(ImGuiKey_R, false)) context->GetGizmoMode() = 1; // Rotate (回転)
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) context->GetGizmoMode() = 2; // Scale (拡縮)
                if (ImGui::IsKeyPressed(ImGuiKey_L, false)) context->GetIsAnimLocked() = !context->GetIsAnimLocked(); // Lock (ロック)
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) context->GetIsAnimHudMinimized() = !context->GetIsAnimHudMinimized(); // HUD Minimize toggle

                if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
                    if (io.KeyShift) {
                        context->InsertAllJointsSRTKey(sceneManager);
                    } else {
                        context->InsertSelectedJointSRTKey(sceneManager);
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                    if (io.KeyShift || context->GetSelectedKeyIndex() < 0) {
                        // Shift+Del または サマリー/全体選択時は全ボーンのキーを一括削除
                        context->PushAnimUndoState("全ボーンキー削除 (Del)");
                        float delTime = context->GetAnimEditorTime();
                        for (auto& [nName, nAnim] : context->GetEditingAnimation().nodeAnimations) {
                            nAnim.rotate.erase(
                                std::remove_if(nAnim.rotate.begin(), nAnim.rotate.end(),
                                    [delTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                                nAnim.rotate.end()
                            );
                            nAnim.translate.erase(
                                std::remove_if(nAnim.translate.begin(), nAnim.translate.end(),
                                    [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                                nAnim.translate.end()
                            );
                            nAnim.scale.erase(
                                std::remove_if(nAnim.scale.begin(), nAnim.scale.end(),
                                    [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                                nAnim.scale.end()
                            );
                        }
                        context->GetSelectedKeyIndex() = -1;
                        context->UpdateAnimationPosePreview(sceneManager);
                    } else {
                        // 選択中ボーンの個別キー削除
                        context->PushAnimUndoState("キー削除 (Del)");
                        NodeAnimation& nodeAnim = context->GetEditingAnimation().nodeAnimations[context->GetSelectedJointName()];
                        float delTime = context->GetAnimEditorTime();
                        if (context->GetSelectedKeyIndex() >= 0 && context->GetSelectedKeyIndex() < static_cast<int>(nodeAnim.rotate.size())) {
                            delTime = nodeAnim.rotate[context->GetSelectedKeyIndex()].time;
                        }

                        nodeAnim.rotate.erase(
                            std::remove_if(nodeAnim.rotate.begin(), nodeAnim.rotate.end(),
                                [delTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                            nodeAnim.rotate.end()
                        );

                        nodeAnim.translate.erase(
                            std::remove_if(nodeAnim.translate.begin(), nodeAnim.translate.end(),
                                [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                            nodeAnim.translate.end()
                        );

                        nodeAnim.scale.erase(
                            std::remove_if(nodeAnim.scale.begin(), nodeAnim.scale.end(),
                                [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                            nodeAnim.scale.end()
                        );

                        context->GetSelectedKeyIndex() = -1;
                        context->UpdateAnimationPosePreview(sceneManager);
                    }
                }
            }
        }

        // ========================================================
        // 2. ドープシート タイムライン本体 (Canvas & Tracks)
        // ========================================================
        if (context->GetCurrentJointList().empty() || context->GetAnimJointTreeNodes().empty()) {
            context->RefreshAnimationJointList(sceneManager);
        }

        // 可視トラックの収集 (開いている親の子孫のみ再帰的に追加)
        struct VisibleAnimTrack {
            std::string name;
            int32_t jointIndex = -1;
            int depth = 0;
            bool hasChildren = false;
            bool isOpen = false;
        };
        std::vector<VisibleAnimTrack> visibleTracks;

        std::function<void(int32_t, int)> collectVisible = [&](int32_t nodeIdx, int depth) {
            if (nodeIdx < 0 || nodeIdx >= static_cast<int32_t>(context->GetAnimJointTreeNodes().size())) return;
            const auto& node = context->GetAnimJointTreeNodes()[nodeIdx];
            bool hasChildren = !node.children.empty();
            bool isOpen = false;
            if (hasChildren) {
                auto it = context->GetAnimJointExpanded().find(node.name);
                isOpen = (it != context->GetAnimJointExpanded().end() && it->second);
            }

            visibleTracks.push_back({ node.name, node.jointIndex, depth, hasChildren, isOpen });

            if (hasChildren && isOpen) {
                for (int32_t childIdx : node.children) {
                    collectVisible(childIdx, depth + 1);
                }
            }
        };

        for (int32_t rootIdx : context->GetAnimJointRootIndices()) {
            collectVisible(rootIdx, 0);
        }

        if (visibleTracks.empty()) {
            for (const auto& name : context->GetCurrentJointList()) {
                visibleTracks.push_back({ name, -1, 0, false, false });
            }
        }

        const float trackListWidth = 220.0f;
        const float rulerHeight = 26.0f;
        const float trackHeight = 22.0f;
        const float summaryHeight = 24.0f;
        int numVisibleTracks = static_cast<int>(visibleTracks.size());
        float totalHeight = rulerHeight + summaryHeight + numVisibleTracks * trackHeight + 50.0f;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
        float canvasWidth = (std::max)(canvasAvail.x, 300.0f);

        // ホイールによるズームと横スクロール
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            if (io.KeyCtrl && io.MouseWheel != 0.0f) {
                float zoomFactor = (io.MouseWheel > 0.0f) ? 1.15f : 0.87f;
                animTimelineZoom_ = std::clamp(animTimelineZoom_ * zoomFactor, 40.0f, 800.0f);
            } else if (io.MouseWheel != 0.0f && !io.KeyCtrl) {
                animTimelineScrollX_ = (std::max)(0.0f, animTimelineScrollX_ - io.MouseWheel * 40.0f);
            }
        }

        ImGui::BeginChild("##DopeSheetScrollArea", ImVec2(canvasWidth, canvasAvail.y), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + canvasWidth, p0.y + totalHeight);
        float contentBottomY = p0.y + totalHeight;

        // タイムライン描画領域
        float timelineStartX = p0.x + trackListWidth;
        float timelineEndX = p0.x + canvasWidth;
        float summaryY = p0.y + rulerHeight;

        // ----------------------------------------------------
        // 1. 背景描画
        // ----------------------------------------------------
        // 全体背景
        drawList->AddRectFilled(p0, p1, IM_COL32(26, 26, 30, 255));

        // 各トラック行の背景（ストライプ＆選択ハイライト）
        float curTrackY = summaryY + summaryHeight;
        for (int i = 0; i < numVisibleTracks; ++i) {
            const auto& item = visibleTracks[i];
            bool isSelected = (context->GetSelectedJointName() == item.name);
            ImU32 rowBg = isSelected ? IM_COL32(38, 62, 92, 255) : (i % 2 == 0 ? IM_COL32(32, 32, 36, 255) : IM_COL32(26, 26, 30, 255));
            drawList->AddRectFilled(ImVec2(timelineStartX, curTrackY), ImVec2(p1.x, curTrackY + trackHeight), rowBg);
            drawList->AddLine(ImVec2(timelineStartX, curTrackY + trackHeight), ImVec2(p1.x, curTrackY + trackHeight), IM_COL32(45, 45, 52, 255), 1.0f);
            curTrackY += trackHeight;
        }

        // サマリー行のタイムライン背景
        drawList->AddRectFilled(ImVec2(timelineStartX, summaryY), ImVec2(p1.x, summaryY + summaryHeight), IM_COL32(46, 42, 36, 255));
        drawList->AddLine(ImVec2(timelineStartX, summaryY + summaryHeight), ImVec2(p1.x, summaryY + summaryHeight), IM_COL32(70, 64, 55, 255), 1.0f);

        // ----------------------------------------------------
        // 2. タイムライン縦グリッド線（背景の上に描画して完全に貫通させる）
        // ----------------------------------------------------
        float maxDuration = (std::max)(context->GetEditingAnimation().duration, 1.0f) + 1.0f;
        int maxFrames = static_cast<int>(std::ceil(maxDuration * context->GetAnimEditorFps()));
        int fpsInt = static_cast<int>(std::round(context->GetAnimEditorFps()));
        if (fpsInt <= 0) fpsInt = 60;
        int stepF = (animTimelineZoom_ > 250.0f) ? 5 : (animTimelineZoom_ > 100.0f ? 10 : 30);

        // (a) マイナーグリッド線（1フレームごと、ズーム時）
        if (animTimelineZoom_ > 140.0f) {
            for (int f = 0; f <= maxFrames; ++f) {
                if (f % 5 == 0) continue;
                float t = f / context->GetAnimEditorFps();
                float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
                if (x >= timelineStartX && x <= timelineEndX) {
                    drawList->AddLine(ImVec2(x, p0.y + rulerHeight), ImVec2(x, contentBottomY), IM_COL32(40, 42, 48, 220), 1.0f);
                }
            }
        }

        // (b) 中グリッド線（5F / 10F / 30Fごと）
        for (int f = 0; f <= maxFrames; f += stepF) {
            if (f % fpsInt == 0) continue;
            float t = f / context->GetAnimEditorFps();
            float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
            if (x >= timelineStartX && x <= timelineEndX) {
                drawList->AddLine(ImVec2(x, p0.y + rulerHeight), ImVec2(x, contentBottomY), IM_COL32(58, 62, 72, 230), 1.0f);
            }
        }

        // (c) メジャーグリッド線（1秒ごと / FPSの倍数）
        for (int f = 0; f <= maxFrames; f += fpsInt) {
            float t = f / context->GetAnimEditorFps();
            float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
            if (x >= timelineStartX && x <= timelineEndX) {
                drawList->AddLine(ImVec2(x, p0.y + rulerHeight), ImVec2(x, contentBottomY), IM_COL32(90, 95, 110, 255), 1.5f);
            }
        }

        // (d) アニメーション終了（Duration）境界線
        float endX = timelineStartX + context->GetEditingAnimation().duration * animTimelineZoom_ - animTimelineScrollX_;
        if (endX >= timelineStartX && endX <= timelineEndX) {
            drawList->AddLine(ImVec2(endX, p0.y + rulerHeight), ImVec2(endX, contentBottomY), IM_COL32(235, 150, 40, 255), 2.0f);
        }

        // ----------------------------------------------------
        // 3. ルーラー（上部目盛りバー）
        // ----------------------------------------------------
        drawList->AddRectFilled(ImVec2(timelineStartX, p0.y), ImVec2(p1.x, p0.y + rulerHeight), IM_COL32(42, 44, 50, 255));
        drawList->AddLine(ImVec2(timelineStartX, p0.y + rulerHeight), ImVec2(p1.x, p0.y + rulerHeight), IM_COL32(70, 74, 84, 255), 1.0f);

        for (int f = 0; f <= maxFrames; f += stepF) {
            float t = f / context->GetAnimEditorFps();
            float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
            if (x < timelineStartX || x > timelineEndX) continue;

            bool isSec = (f % fpsInt == 0);
            float tickH = isSec ? 12.0f : 6.0f;
            ImU32 tickCol = isSec ? IM_COL32(220, 225, 235, 255) : IM_COL32(160, 165, 175, 255);
            drawList->AddLine(ImVec2(x, p0.y + rulerHeight - tickH), ImVec2(x, p0.y + rulerHeight), tickCol, isSec ? 1.5f : 1.0f);

            char fBuf[32];
            snprintf(fBuf, sizeof(fBuf), "%d", f);
            drawList->AddText(ImVec2(x + 3, p0.y + 4), isSec ? IM_COL32(230, 235, 245, 255) : IM_COL32(170, 175, 185, 255), fBuf);
        }

        // ルーラーおよびタイムライン全領域でのスクラブ（時間シーク）操作
        ImVec2 mousePos = io.MousePos;
        bool isHoverTimeline = (mousePos.x >= timelineStartX && mousePos.x <= timelineEndX && mousePos.y >= p0.y && mousePos.y <= contentBottomY);

        if (isHoverTimeline && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl) {
            isAnimRulerScrubbing_ = true;
            context->GetTempOverrides().clear();
        }
        if (isAnimRulerScrubbing_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float newTime = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                context->GetAnimEditorTime() = std::clamp(newTime, 0.0f, context->GetEditingAnimation().duration);
                context->UpdateAnimationPosePreview(sceneManager);
            } else {
                isAnimRulerScrubbing_ = false;
            }
        }

        // ----------------------------------------------------
        // 4. サマリーキー（概要）の描画 & 操作
        // ----------------------------------------------------
        std::set<float> summaryKeyTimes;
        for (const auto& [nName, nAnim] : context->GetEditingAnimation().nodeAnimations) {
            for (const auto& k : nAnim.rotate) summaryKeyTimes.insert(k.time);
            for (const auto& k : nAnim.translate) summaryKeyTimes.insert(k.time);
            for (const auto& k : nAnim.scale) summaryKeyTimes.insert(k.time);
        }

        float deleteSummaryTime = -1.0f;
        for (float sTime : summaryKeyTimes) {
            float sX = timelineStartX + sTime * animTimelineZoom_ - animTimelineScrollX_;
            if (sX >= timelineStartX && sX <= timelineEndX) {
                float sCenterY = summaryY + summaryHeight * 0.5f;
                ImVec2 dP[4] = {
                    ImVec2(sX, sCenterY - 5.0f),
                    ImVec2(sX + 5.0f, sCenterY),
                    ImVec2(sX, sCenterY + 5.0f),
                    ImVec2(sX - 5.0f, sCenterY)
                };
                bool isNearCurTime = std::abs(sTime - context->GetAnimEditorTime()) < 0.01f;
                ImU32 dCol = isNearCurTime ? IM_COL32(255, 220, 60, 255) : IM_COL32(230, 160, 40, 255);
                drawList->AddConvexPolyFilled(dP, 4, dCol);
                drawList->AddPolyline(dP, 4, IM_COL32(20, 20, 20, 255), ImDrawFlags_Closed, 1.0f);

                // 左クリックでサマリーキー選択 & 時間シーク
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && std::abs(mousePos.x - sX) <= 6.0f && std::abs(mousePos.y - sCenterY) <= 6.0f) {
                    context->GetAnimEditorTime() = sTime;
                    context->GetSelectedKeyIndex() = -1;
                    context->GetTempOverrides().clear();
                    context->UpdateAnimationPosePreview(sceneManager);
                }

                // Ctrlキーを押しながらドラッグした場合のみキー移動を許可（通常ドラッグでの誤移動を完全防止）
                if (io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f) && std::abs(io.MouseClickedPos[0].x - sX) <= 6.0f && std::abs(io.MouseClickedPos[0].y - sCenterY) <= 6.0f) {
                    if (!isSummaryKeyDrag_) {
                        isSummaryKeyDrag_ = true;
                        dragSummaryOriginalTime_ = sTime;
                        context->BeginDragSnapshot("サマリーキー移動");
                    }
                }

                // 右クリックでサマリーキー（全ボーンの該当フレームキー）を一括削除
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && std::abs(mousePos.x - sX) <= 6.0f && std::abs(mousePos.y - sCenterY) <= 6.0f) {
                    deleteSummaryTime = sTime;
                }
            }
        }

        if (deleteSummaryTime >= 0.0f) {
            context->PushAnimUndoState("全ボーンキー削除");
            for (auto& [nName, nAnim] : context->GetEditingAnimation().nodeAnimations) {
                nAnim.rotate.erase(
                    std::remove_if(nAnim.rotate.begin(), nAnim.rotate.end(),
                        [deleteSummaryTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - deleteSummaryTime) < 0.005f; }),
                    nAnim.rotate.end()
                );
                nAnim.translate.erase(
                    std::remove_if(nAnim.translate.begin(), nAnim.translate.end(),
                        [deleteSummaryTime](const KeyframeVector3& kf) { return std::abs(kf.time - deleteSummaryTime) < 0.005f; }),
                    nAnim.translate.end()
                );
                nAnim.scale.erase(
                    std::remove_if(nAnim.scale.begin(), nAnim.scale.end(),
                        [deleteSummaryTime](const KeyframeVector3& kf) { return std::abs(kf.time - deleteSummaryTime) < 0.005f; }),
                    nAnim.scale.end()
                );
            }
            context->GetSelectedKeyIndex() = -1;
            context->UpdateAnimationPosePreview(sceneManager);
        }

        if (isSummaryKeyDrag_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float newT = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                newT = std::clamp(newT, 0.0f, context->GetEditingAnimation().duration);
                float dt = newT - dragSummaryOriginalTime_;
                if (std::abs(dt) > 0.001f) {
                    for (auto& [nName, nAnim] : context->GetEditingAnimation().nodeAnimations) {
                        for (auto& k : nAnim.rotate) {
                            if (std::abs(k.time - dragSummaryOriginalTime_) < 0.005f) {
                                k.time = newT;
                            }
                        }
                        for (auto& k : nAnim.translate) {
                            if (std::abs(k.time - dragSummaryOriginalTime_) < 0.005f) {
                                k.time = newT;
                            }
                        }
                        for (auto& k : nAnim.scale) {
                            if (std::abs(k.time - dragSummaryOriginalTime_) < 0.005f) {
                                k.time = newT;
                            }
                        }
                    }
                    dragSummaryOriginalTime_ = newT;
                    context->GetAnimEditorTime() = newT;
                    context->UpdateAnimationPosePreview(sceneManager);
                }
            } else {
                if (context->GetHasAnimDragPreSnapshot()) {
                    context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
                    if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
                    context->GetRedoStack().clear();
                    context->GetHasAnimDragPreSnapshot() = false;
                }
                isSummaryKeyDrag_ = false;
            }
        }

        // ----------------------------------------------------
        // 5. 各可視トラックのキーフレーム（◆）描画
        // ----------------------------------------------------
        curTrackY = summaryY + summaryHeight;
        for (int i = 0; i < numVisibleTracks; ++i) {
            const auto& item = visibleTracks[i];
            const std::string& jointName = item.name;
            bool isSelected = (context->GetSelectedJointName() == jointName);

            if (context->GetEditingAnimation().nodeAnimations.find(jointName) != context->GetEditingAnimation().nodeAnimations.end()) {
                auto& nodeAnim = context->GetEditingAnimation().nodeAnimations[jointName];
                for (size_t k = 0; k < nodeAnim.rotate.size(); ++k) {
                    float kTime = nodeAnim.rotate[k].time;
                    float kX = timelineStartX + kTime * animTimelineZoom_ - animTimelineScrollX_;
                    if (kX >= timelineStartX - 10.0f && kX <= timelineEndX + 10.0f) {
                        float kCenterY = curTrackY + trackHeight * 0.5f;
                        bool isKfSelected = (isSelected && context->GetSelectedKeyIndex() == static_cast<int>(k));
                        
                        ImVec2 kdP[4] = {
                            ImVec2(kX, kCenterY - 4.5f),
                            ImVec2(kX + 4.5f, kCenterY),
                            ImVec2(kX, kCenterY + 4.5f),
                            ImVec2(kX - 4.5f, kCenterY)
                        };
                        ImU32 kCol = isKfSelected ? IM_COL32(255, 215, 50, 255) : IM_COL32(225, 225, 230, 255);
                        drawList->AddConvexPolyFilled(kdP, 4, kCol);
                        drawList->AddPolyline(kdP, 4, IM_COL32(10, 10, 10, 255), ImDrawFlags_Closed, 1.0f);

                        // 左クリックでキーフレーム選択 & 時間シーク
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            if (std::abs(mousePos.x - kX) <= 6.0f && std::abs(mousePos.y - kCenterY) <= 6.0f) {
                                context->GetSelectedJointName() = jointName;
                                context->GetSelectedKeyIndex() = static_cast<int>(k);
                                context->GetAnimEditorTime() = kTime;
                                context->GetTempOverrides().clear();
                                context->UpdateAnimationPosePreview(sceneManager);
                            }
                        }

                        // Ctrlキーを押しながらドラッグした場合のみキー移動を許可（通常ドラッグでの誤移動を完全防止）
                        if (io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f) && isSelected) {
                            if (std::abs(io.MouseClickedPos[0].x - kX) <= 6.0f && std::abs(io.MouseClickedPos[0].y - kCenterY) <= 6.0f) {
                                if (!isDraggingAnimKeyframe_) {
                                    isDraggingAnimKeyframe_ = true;
                                    context->GetSelectedJointName() = jointName;
                                    context->GetSelectedKeyIndex() = static_cast<int>(k);
                                    dragAnimKeyOriginalTime_ = kTime;
                                    context->BeginDragSnapshot("キーフレーム移動");
                                }
                            }
                        }
                    }
                }
            }

            curTrackY += trackHeight;
        }

        if (isDraggingAnimKeyframe_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float newT = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                newT = std::clamp(newT, 0.0f, context->GetEditingAnimation().duration);
                auto& nodeAnim = context->GetEditingAnimation().nodeAnimations[context->GetSelectedJointName()];
                if (context->GetSelectedKeyIndex() >= 0 && context->GetSelectedKeyIndex() < static_cast<int>(nodeAnim.rotate.size())) {
                    float oldT = nodeAnim.rotate[context->GetSelectedKeyIndex()].time;
                    nodeAnim.rotate[context->GetSelectedKeyIndex()].time = newT;
                    for (auto& kf : nodeAnim.translate) {
                        if (std::abs(kf.time - oldT) < 0.005f) kf.time = newT;
                    }
                    for (auto& kf : nodeAnim.scale) {
                        if (std::abs(kf.time - oldT) < 0.005f) kf.time = newT;
                    }
                    context->GetAnimEditorTime() = newT;
                    context->UpdateAnimationPosePreview(sceneManager);
                }
            } else {
                auto& nodeAnim = context->GetEditingAnimation().nodeAnimations[context->GetSelectedJointName()];
                std::sort(nodeAnim.rotate.begin(), nodeAnim.rotate.end(), [](const KeyframeQuaternion& a, const KeyframeQuaternion& b) {
                    return a.time < b.time;
                });
                if (context->GetHasAnimDragPreSnapshot()) {
                    context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
                    if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
                    context->GetRedoStack().clear();
                    context->GetHasAnimDragPreSnapshot() = false;
                }
                isDraggingAnimKeyframe_ = false;
            }
        }

        // ----------------------------------------------------
        // 6. 左カラム（トラックリスト / 階層ツリー & 折りたたみ）の描画
        // ----------------------------------------------------
        // 左カラム全体背景
        drawList->AddRectFilled(p0, ImVec2(p0.x + trackListWidth, contentBottomY), IM_COL32(32, 33, 37, 255));
        // ルーラー部左カラム背景
        drawList->AddRectFilled(p0, ImVec2(p0.x + trackListWidth, p0.y + rulerHeight), IM_COL32(40, 42, 48, 255));
        drawList->AddLine(ImVec2(p0.x, p0.y + rulerHeight), ImVec2(p0.x + trackListWidth, p0.y + rulerHeight), IM_COL32(65, 68, 76, 255), 1.0f);
        // サマリー行左カラム背景
        drawList->AddRectFilled(ImVec2(p0.x, summaryY), ImVec2(p0.x + trackListWidth, summaryY + summaryHeight), IM_COL32(46, 42, 36, 255));
        drawList->AddLine(ImVec2(p0.x, summaryY + summaryHeight), ImVec2(p0.x + trackListWidth, summaryY + summaryHeight), IM_COL32(70, 64, 55, 255), 1.0f);
        
        // 縦境界線
        drawList->AddLine(ImVec2(p0.x + trackListWidth, p0.y), ImVec2(p0.x + trackListWidth, contentBottomY), IM_COL32(65, 68, 76, 255), 1.5f);

        // ヘッダー行テキスト
        drawList->AddText(ImVec2(p0.x + 8, p0.y + 5), IM_COL32(200, 205, 215, 255), "チャネル / 関節");

        // [+] 全展開 / [-] 全閉じる ボタン
        float btnY = p0.y + 3.0f;
        float btnExpandX = p0.x + trackListWidth - 52.0f;
        float btnCollapseX = p0.x + trackListWidth - 26.0f;

        // 全展開ボタン [+]
        ImVec2 expMin(btnExpandX, btnY);
        ImVec2 expMax(btnExpandX + 22.0f, btnY + 19.0f);
        bool hoverExp = (mousePos.x >= expMin.x && mousePos.x <= expMax.x && mousePos.y >= expMin.y && mousePos.y <= expMax.y);
        drawList->AddRectFilled(expMin, expMax, hoverExp ? IM_COL32(70, 80, 100, 255) : IM_COL32(48, 52, 60, 255), 3.0f);
        drawList->AddRect(expMin, expMax, IM_COL32(85, 92, 105, 255), 3.0f);
        drawList->AddText(ImVec2(expMin.x + 4, expMin.y + 2), IM_COL32(220, 225, 235, 255), "[+]");
        if (hoverExp) {
            ImGui::SetTooltip("すべての階層を展開");
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                for (const auto& node : context->GetAnimJointTreeNodes()) {
                    if (!node.children.empty()) {
                        context->GetAnimJointExpanded()[node.name] = true;
                    }
                }
            }
        }

        // 全閉じるボタン [-]
        ImVec2 colMin(btnCollapseX, btnY);
        ImVec2 colMax(btnCollapseX + 22.0f, btnY + 19.0f);
        bool hoverCol = (mousePos.x >= colMin.x && mousePos.x <= colMax.x && mousePos.y >= colMin.y && mousePos.y <= colMax.y);
        drawList->AddRectFilled(colMin, colMax, hoverCol ? IM_COL32(70, 80, 100, 255) : IM_COL32(48, 52, 60, 255), 3.0f);
        drawList->AddRect(colMin, colMax, IM_COL32(85, 92, 105, 255), 3.0f);
        drawList->AddText(ImVec2(colMin.x + 5, colMin.y + 2), IM_COL32(220, 225, 235, 255), "[-]");
        if (hoverCol) {
            ImGui::SetTooltip("すべての階層を閉じる");
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                for (const auto& node : context->GetAnimJointTreeNodes()) {
                    context->GetAnimJointExpanded()[node.name] = false;
                }
            }
        }

        // サマリー行ラベル
        drawList->AddText(ImVec2(p0.x + 8, summaryY + 4), IM_COL32(245, 185, 85, 255), "[Summary] 概要");

        // 各可視トラック行（左カラム）のツリー描画
        curTrackY = summaryY + summaryHeight;
        for (int i = 0; i < numVisibleTracks; ++i) {
            const auto& item = visibleTracks[i];
            const std::string& jointName = item.name;
            bool isSelected = (context->GetSelectedJointName() == jointName);

            // 行背景
            ImU32 rowBg = isSelected ? IM_COL32(38, 62, 92, 255) : (i % 2 == 0 ? IM_COL32(34, 35, 39, 255) : IM_COL32(28, 29, 33, 255));
            drawList->AddRectFilled(ImVec2(p0.x, curTrackY), ImVec2(p0.x + trackListWidth, curTrackY + trackHeight), rowBg);
            drawList->AddLine(ImVec2(p0.x, curTrackY + trackHeight), ImVec2(p0.x + trackListWidth, curTrackY + trackHeight), IM_COL32(48, 50, 56, 255), 1.0f);

            float indentX = p0.x + 8.0f + item.depth * 14.0f;
            float rowMidY = curTrackY + trackHeight * 0.5f;

            // 階層接続線（ツリー線）
            if (item.depth > 0) {
                float lineX = indentX - 7.0f;
                drawList->AddLine(ImVec2(lineX, curTrackY), ImVec2(lineX, rowMidY), IM_COL32(80, 85, 95, 180), 1.0f);
                drawList->AddLine(ImVec2(lineX, rowMidY), ImVec2(indentX - 1.0f, rowMidY), IM_COL32(80, 85, 95, 180), 1.0f);
            }

            // トグルアイコン（▶ / ▼）または葉マーカー
            float iconW = 12.0f;
            if (item.hasChildren) {
                ImVec2 toggleMin(indentX, curTrackY + 2.0f);
                ImVec2 toggleMax(indentX + iconW + 4.0f, curTrackY + trackHeight - 2.0f);
                bool isHoverToggle = (mousePos.x >= toggleMin.x && mousePos.x <= toggleMax.x && mousePos.y >= toggleMin.y && mousePos.y <= toggleMax.y);

                if (item.isOpen) {
                    // 下向き三角 ▼
                    ImVec2 tri[3] = {
                        ImVec2(indentX + 2.0f, rowMidY - 3.0f),
                        ImVec2(indentX + 10.0f, rowMidY - 3.0f),
                        ImVec2(indentX + 6.0f, rowMidY + 3.0f)
                    };
                    drawList->AddTriangleFilled(tri[0], tri[1], tri[2], isHoverToggle ? IM_COL32(255, 230, 100, 255) : IM_COL32(200, 205, 220, 255));
                } else {
                    // 右向き三角 ▶
                    ImVec2 tri[3] = {
                        ImVec2(indentX + 3.0f, rowMidY - 5.0f),
                        ImVec2(indentX + 9.0f, rowMidY),
                        ImVec2(indentX + 3.0f, rowMidY + 5.0f)
                    };
                    drawList->AddTriangleFilled(tri[0], tri[1], tri[2], isHoverToggle ? IM_COL32(255, 230, 100, 255) : IM_COL32(170, 175, 190, 255));
                }

                if (isHoverToggle && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    context->GetAnimJointExpanded()[jointName] = !item.isOpen;
                }
            } else {
                // 葉ノード（ドット •）
                drawList->AddCircleFilled(ImVec2(indentX + 5.0f, rowMidY), 2.0f, IM_COL32(110, 115, 130, 255));
            }

            // ジョイント名テキスト
            float textStartX = indentX + iconW + 4.0f;
            ImU32 textCol = isSelected ? IM_COL32(110, 210, 255, 255) : (item.hasChildren ? IM_COL32(235, 240, 250, 255) : IM_COL32(185, 190, 200, 255));
            std::string dispTrackName = jointName + (isSelected && context->GetIsAnimLocked() ? " [Locked]" : "");
            drawList->AddText(ImVec2(textStartX, curTrackY + 3.0f), textCol, dispTrackName.c_str());

            // 行クリックによるボーン選択（トグルアイコン以外の領域をクリック時）
            if (!context->GetIsAnimLocked() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (mousePos.x >= p0.x && mousePos.x <= p0.x + trackListWidth && mousePos.y >= curTrackY && mousePos.y < curTrackY + trackHeight) {
                    bool clickedToggle = item.hasChildren && (mousePos.x >= indentX && mousePos.x <= indentX + iconW + 4.0f);
                    if (!clickedToggle) {
                        context->GetSelectedJointName() = jointName;
                        context->GetSelectedKeyIndex() = -1;
                        context->GetTempOverrides().clear();
                        context->UpdateAnimationPosePreview(sceneManager);
                    }
                }
            }

            curTrackY += trackHeight;
        }

        // ----------------------------------------------------
        // 7. 垂直再生ヘッド（Playhead）
        // ----------------------------------------------------
        float playheadX = timelineStartX + context->GetAnimEditorTime() * animTimelineZoom_ - animTimelineScrollX_;
        if (playheadX >= timelineStartX && playheadX <= timelineEndX) {
            drawList->AddLine(ImVec2(playheadX, p0.y), ImVec2(playheadX, contentBottomY), IM_COL32(50, 160, 255, 255), 2.0f);

            ImVec2 badgeP[4] = {
                ImVec2(playheadX - 10.0f, p0.y),
                ImVec2(playheadX + 10.0f, p0.y),
                ImVec2(playheadX + 6.0f, p0.y + rulerHeight - 2),
                ImVec2(playheadX - 6.0f, p0.y + rulerHeight - 2)
            };
            drawList->AddConvexPolyFilled(badgeP, 4, IM_COL32(40, 140, 255, 255));
            drawList->AddPolyline(badgeP, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.0f);

            char phBuf[16];
            int curFrame = static_cast<int>(std::round(context->GetAnimEditorTime() * context->GetAnimEditorFps()));
            snprintf(phBuf, sizeof(phBuf), "%d", curFrame);
            ImVec2 textSize = ImGui::CalcTextSize(phBuf);
            drawList->AddText(ImVec2(playheadX - textSize.x * 0.5f, p0.y + 3), IM_COL32(255, 255, 255, 255), phBuf);
        }

        // タイムライン領域の空きスペースクリックでシーク
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (mousePos.x >= timelineStartX && mousePos.x <= timelineEndX && mousePos.y > summaryY + summaryHeight && !isDraggingAnimKeyframe_ && !isSummaryKeyDrag_) {
                float clickTime = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                context->GetAnimEditorTime() = std::clamp(clickTime, 0.0f, context->GetEditingAnimation().duration);
                context->GetTempOverrides().clear();
                context->UpdateAnimationPosePreview(sceneManager);
            }
        }

        ImGui::Dummy(ImVec2(timelineStartX + maxDuration * animTimelineZoom_, totalHeight));
        ImGui::EndChild();
    }
    ImGui::End();
}



#endif
