#ifdef USE_IMGUI
#include "EditorReplayUI.h"
#include "EditorManager.h"
#include "EditorCommandManager.h"
#include "Editor/Replay/ReplayManager.h"
#include "Editor/Replay/PhysicsAStar.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

void EditorReplayUI::DrawHierarchyWindow(bool* pOpen, EditorManager* editorManager) {
    if (!pOpen || !*pOpen || !editorManager) return;

    if (ImGui::Begin("マイメディア (リプレイ履歴)", pOpen)) {
        auto replayMgr = ReplayManager::GetInstance();
        if (replayMgr->IsRecording()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[録画中...] (%d フレーム)", replayMgr->GetRecordedFrameCount());
        } else if (replayMgr->IsPlaying()) {
            if (replayMgr->IsPaused()) {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "[一時停止中] (%d / %d F)", replayMgr->GetCurrentFrame(), replayMgr->GetCurrentReplay().totalFrames);
            } else {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[再生中] (%d / %d F)", replayMgr->GetCurrentFrame(), replayMgr->GetCurrentReplay().totalFrames);
            }
        } else {
            ImGui::Text("待機中");
        }
        ImGui::Separator();

        if (ImGui::CollapsingHeader("一時履歴データ (未保存)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& history = replayMgr->GetHistory();
            if (history.empty()) {
                ImGui::TextDisabled(" (履歴データなし)");
            } else {
                static int selectedHistoryIdx = -1;
                for (int i = 0; i < (int)history.size(); i++) {
                    char label[128];
                    snprintf(label, sizeof(label), "履歴 #%d (%s, %d F)", history[i].id, history[i].dateStr.c_str(), history[i].totalFrames);
                    if (ImGui::Selectable(label, selectedHistoryIdx == i)) {
                        selectedHistoryIdx = i;
                    }
                }
                if (selectedHistoryIdx >= 0 && selectedHistoryIdx < (int)history.size()) {
                    ImGui::Spacing();
                    if (ImGui::Button("再生", ImVec2(70, 0))) {
                        replayMgr->StartPlayback(selectedHistoryIdx);
                        if (!history[selectedHistoryIdx].mapDataStr.empty()) {
                            editorManager->SetLoadMapDataNextFrame(history[selectedHistoryIdx].mapDataStr);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("保存", ImVec2(70, 0))) {
                        saveTargetHistoryIdx_ = selectedHistoryIdx;
                        snprintf(saveFileNameBuf_, sizeof(saveFileNameBuf_), "replay_%d", history[selectedHistoryIdx].id);
                        ImGui::OpenPopup("リプレイの保存");
                    }
                }
            }
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("保存済みリプレイデータ", ImGuiTreeNodeFlags_DefaultOpen)) {
            replayMgr->LoadSavedList();
            const auto& savedFiles = replayMgr->GetSavedList();

            if (savedFiles.empty()) {
                ImGui::TextDisabled(" (保存データなし)");
            } else {
                static int selectedFileIdx = -1;
                for (int i = 0; i < (int)savedFiles.size(); i++) {
                    if (ImGui::Selectable(savedFiles[i].c_str(), selectedFileIdx == i)) {
                        selectedFileIdx = i;
                    }
                }

                if (selectedFileIdx >= 0 && selectedFileIdx < (int)savedFiles.size()) {
                    ImGui::Spacing();
                    if (ImGui::Button("ファイル再生", ImVec2(90, 0))) {
                        replayMgr->StartPlayback(-1, savedFiles[selectedFileIdx]);
                        const auto& curReplay = replayMgr->GetCurrentReplay();
                        if (!curReplay.mapDataStr.empty()) {
                            editorManager->SetLoadMapDataNextFrame(curReplay.mapDataStr);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("ファイル削除", ImVec2(90, 0))) {
                        replayMgr->DeleteSavedFile(savedFiles[selectedFileIdx]);
                        selectedFileIdx = -1;
                    }
                }
            }
        }

        // リプレイ保存時のファイル名入力ダイアログ
        if (ImGui::BeginPopupModal("リプレイの保存", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("保存するファイル名を入力してください (.mml):");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(250.0f);
            ImGui::InputText("##SaveFileNameInput", saveFileNameBuf_, sizeof(saveFileNameBuf_));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("保存", ImVec2(100, 26))) {
                if (strlen(saveFileNameBuf_) > 0) {
                    const auto& historyList = replayMgr->GetHistory();
                    if (saveTargetHistoryIdx_ >= 0 && saveTargetHistoryIdx_ < (int)historyList.size()) {
                        replayMgr->SaveToFile(historyList[saveTargetHistoryIdx_], std::string(saveFileNameBuf_));
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(100, 26))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void EditorReplayUI::DrawTimelineWindow(bool* pOpen, EditorManager* editorManager, EditorCommandManager* cmdManager) {
    if (!pOpen || !*pOpen || !editorManager) return;

    if (ImGui::Begin("タイムライン", pOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        auto replayMgr = ReplayManager::GetInstance();
        const auto& currentReplay = replayMgr->GetCurrentReplay();

        int currentFrame = replayMgr->GetCurrentFrame();
        int totalFrames = currentReplay.totalFrames;
        if (totalFrames <= 0 && replayMgr->IsRecording()) {
            totalFrames = replayMgr->GetRecordedFrameCount();
        }
        if (totalFrames <= 0) totalFrames = 1;

        // 1. コントロールバー
        if (ImGui::Button("|<", ImVec2(32, 28))) {
            if (currentFrame > 0) replayMgr->SetCurrentFrame(currentFrame - 1);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム戻る");
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.22f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.32f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.15f, 0.80f, 1.0f));
        if (replayMgr->IsPlaying() && !replayMgr->IsPaused()) {
            if (ImGui::Button("一時停止", ImVec2(90, 28))) replayMgr->PausePlayback();
        } else {
            if (ImGui::Button("再生", ImVec2(90, 28))) {
                replayMgr->SetSnapEnabled(true);
                replayMgr->SetInterpolationEnabled(true);
                const auto& history = replayMgr->GetHistory();
                if (!history.empty()) {
                    replayMgr->StartPlayback(0);
                    if (!history[0].mapDataStr.empty()) {
                        editorManager->SetLoadMapDataNextFrame(history[0].mapDataStr);
                    }
                } else {
                    const auto& saved = replayMgr->GetSavedList();
                    if (!saved.empty()) replayMgr->StartPlayback(-1, saved[0]);
                }
            }
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("保存されている元のリプレイデータを再生します");
        ImGui::SameLine();

        if (ImGui::Button(">|", ImVec2(32, 28))) {
            if (currentFrame < totalFrames) replayMgr->SetCurrentFrame(currentFrame + 1);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム進む");
        ImGui::SameLine();

        if (ImGui::Button("停止", ImVec2(65, 28))) replayMgr->StopPlayback();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生を停止します");
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.34f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.68f, 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.45f, 0.27f, 1.0f));
        if (ImGui::Button("変更内容を再生", ImVec2(130, 28))) {
            replayMgr->SetSnapEnabled(false);
            replayMgr->SetInterpolationEnabled(false);
            if (replayMgr->IsPaused()) {
                replayMgr->ResumePlayback();
            } else {
                replayMgr->StartPlayback(-1, "");
                const auto& cur = replayMgr->GetCurrentReplay();
                if (!cur.mapDataStr.empty()) {
                    editorManager->SetLoadMapDataNextFrame(cur.mapDataStr);
                }
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        float currentSec = currentFrame / 60.0f;
        float totalSec = totalFrames / 60.0f;
        ImGui::TextColored(ImVec4(0.88f, 0.80f, 1.0f, 1.0f), "%02d:%05.2f / %02d:%05.2f  (%d / %d F)",
            (int)currentSec / 60, fmod(currentSec, 60.0f), (int)totalSec / 60, fmod(totalSec, 60.0f), currentFrame, totalFrames);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 240.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 1.0f, 1.0f), "ズーム: %.1fx", timelineZoom_);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("操作引き継ぎ", ImVec2(110, 28))) {
            replayMgr->TakeoverPlayback();
            editorManager->SetTakeoverCountdown(3.0f);
        }
        ImGui::PopStyleColor();

        ImGui::Separator();

        // 2. タイムラインルーラー & ビジュアルトラック描画
        const float trackHeaderWidth = 130.0f;
        const float rulerHeight = 28.0f;
        const float trackHeight = 22.0f;
        const float trackPadding = 4.0f;
        const int numTracks = 7;
        const char* trackNames[7] = { "左移動 (L)", "右移動 (R)", "上移動 (W)", "下移動 (S)", "ジャンプ (J)", "ダッシュ (D)", "掴み/壁 (C)" };
        const ImU32 trackColors[7] = {
            IM_COL32(66, 133, 244, 230), IM_COL32(52, 168, 83, 230), IM_COL32(251, 188, 5, 230),
            IM_COL32(234, 67, 53, 230), IM_COL32(171, 71, 188, 230), IM_COL32(255, 112, 67, 230), IM_COL32(0, 172, 193, 230)
        };

        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.KeyCtrl && io.MouseWheel != 0.0f) {
            timelineZoom_ += io.MouseWheel * 0.5f;
            if (timelineZoom_ < 1.0f) timelineZoom_ = 1.0f;
            if (timelineZoom_ > 20.0f) timelineZoom_ = 20.0f;
        }

        float timelineWidth = totalFrames * timelineZoom_;
        float contentWidth = (std::max)(timelineWidth + trackHeaderWidth + 200.0f, ImGui::GetWindowWidth());
        float totalCanvasHeight = rulerHeight + numTracks * (trackHeight + trackPadding) + 16.0f;

        ImGui::BeginChild("TimelineScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        isReplayEditorHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float scrollX = ImGui::GetScrollX();
        float headerLeftX = p0.x + scrollX;

        drawList->AddRectFilled(p0, ImVec2(p0.x + contentWidth, p0.y + totalCanvasHeight), IM_COL32(20, 18, 28, 255));

        // ルーラー
        ImVec2 rulerTopLeft = ImVec2(p0.x + trackHeaderWidth, p0.y);
        ImVec2 rulerBottomRight = ImVec2(p0.x + trackHeaderWidth + timelineWidth, p0.y + rulerHeight);
        ImVec2 mousePos = ImGui::GetMousePos();
        bool isChildHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        bool mouseInRuler = (mousePos.x >= rulerTopLeft.x && mousePos.x <= rulerBottomRight.x &&
                             mousePos.y >= rulerTopLeft.y && mousePos.y <= rulerBottomRight.y);

        if (isChildHovered && mouseInRuler && replayBlockDragMode_ == ReplayBlockDragMode::None) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsMouseClicked(0)) {
                isRulerScrubbing_ = true;
                selectedReplayBlock_.Clear();
            }
        }

        if (isRulerScrubbing_) {
            if (ImGui::IsMouseDown(0)) {
                float relativeX = mousePos.x - (p0.x + trackHeaderWidth);
                int targetFrame = (int)(relativeX / timelineZoom_ + 0.5f);
                if (targetFrame < 0) targetFrame = 0;
                if (targetFrame > totalFrames) targetFrame = totalFrames;
                replayMgr->SetCurrentFrame(targetFrame);
            } else {
                isRulerScrubbing_ = false;
            }
        }

        ImU32 rulerBgColor = (mouseInRuler || isRulerScrubbing_) ? IM_COL32(55, 45, 75, 255) : IM_COL32(35, 30, 50, 255);
        drawList->AddRectFilled(rulerTopLeft, rulerBottomRight, rulerBgColor);
        drawList->AddLine(ImVec2(p0.x, p0.y + rulerHeight), ImVec2(p0.x + contentWidth, p0.y + rulerHeight), IM_COL32(70, 60, 90, 255), 1.5f);

        int stepFrame = (timelineZoom_ >= 8.0f) ? 10 : (timelineZoom_ >= 3.0f ? 30 : 60);
        for (int f = 0; f <= totalFrames; f += stepFrame) {
            float x = rulerTopLeft.x + f * timelineZoom_;
            bool isMajor = (f % 60 == 0);
            float lineH = isMajor ? 12.0f : 7.0f;
            drawList->AddLine(ImVec2(x, rulerBottomRight.y - lineH), ImVec2(x, rulerBottomRight.y), IM_COL32(180, 170, 210, 200), isMajor ? 1.5f : 1.0f);
            if (isMajor || timelineZoom_ >= 4.0f) {
                float sec = f / 60.0f;
                char timeStr[32];
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d", (int)sec / 60, (int)sec % 60);
                drawList->AddText(ImVec2(x + 3.0f, p0.y + 3.0f), IM_COL32(200, 190, 230, 230), timeStr);
            }
        }

        // トラック描画
        const auto& frames = currentReplay.frames;
        for (int t = 0; t < numTracks; t++) {
            float trackY = p0.y + rulerHeight + t * (trackHeight + trackPadding) + 4.0f;
            ImVec2 trackBgMin = ImVec2(p0.x + trackHeaderWidth, trackY);
            ImVec2 trackBgMax = ImVec2(p0.x + trackHeaderWidth + timelineWidth, trackY + trackHeight);
            drawList->AddRectFilled(trackBgMin, trackBgMax, IM_COL32(28, 25, 38, 255), 3.0f);
            drawList->AddRect(trackBgMin, trackBgMax, IM_COL32(50, 45, 65, 180), 3.0f);

            if (!frames.empty()) {
                auto RenderAndHandleBlock = [&](int trackIdx, int blockStart, int blockEnd) {
                    float bMinX = trackBgMin.x + blockStart * timelineZoom_;
                    float bMaxX = trackBgMin.x + blockEnd * timelineZoom_;
                    if (bMaxX - bMinX < 4.0f) bMaxX = bMinX + 4.0f;

                    float blockMinY = trackY + 2.0f;
                    float blockMaxY = trackY + trackHeight - 2.0f;
                    bool isSelected = selectedReplayBlock_.Equals(trackIdx, blockStart, blockEnd);

                    bool mouseInY = (mousePos.y >= blockMinY && mousePos.y <= blockMaxY);
                    bool mouseInX = (mousePos.x >= bMinX && mousePos.x <= bMaxX);
                    bool mouseHoverBlock = mouseInY && mouseInX;
                    bool nearLeft = mouseInY && (std::abs(mousePos.x - bMinX) <= 6.0f);
                    bool nearRight = mouseInY && (std::abs(mousePos.x - bMaxX) <= 6.0f);

                    if (isChildHovered && replayBlockDragMode_ == ReplayBlockDragMode::None) {
                        if (nearLeft || nearRight) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }
                        if ((nearLeft || nearRight || mouseHoverBlock) && ImGui::IsMouseClicked(0)) {
                            selectedReplayBlock_ = { trackIdx, blockStart, blockEnd };
                            draggingBlockOriginal_ = selectedReplayBlock_;
                            dragStartMouseFrame_ = (int)((mousePos.x - (p0.x + trackHeaderWidth)) / timelineZoom_);
                            dragStartMousePos_ = mousePos;
                            replayDragOldReplayData_ = ReplayManager::GetInstance()->GetCurrentReplay();
                            replayDragOldSelectedBlock_ = selectedReplayBlock_;
                            replayBlockDragMode_ = ReplayBlockDragMode::None;
                            if (nearLeft) pendingBlockDragMode_ = ReplayBlockDragMode::ResizeLeft;
                            else if (nearRight) pendingBlockDragMode_ = ReplayBlockDragMode::ResizeRight;
                            else pendingBlockDragMode_ = ReplayBlockDragMode::Move;
                        }
                    }

                    drawList->AddRectFilled(ImVec2(bMinX, blockMinY), ImVec2(bMaxX, blockMaxY), trackColors[trackIdx], 3.0f);
                    drawList->AddRect(ImVec2(bMinX, blockMinY), ImVec2(bMaxX, blockMaxY), IM_COL32(255, 255, 255, mouseHoverBlock ? 220 : 120), 3.0f);

                    if (isSelected) {
                        drawList->AddRect(ImVec2(bMinX - 1.0f, blockMinY - 1.0f), ImVec2(bMaxX + 1.0f, blockMaxY + 1.0f), IM_COL32(255, 255, 255, 255), 3.0f, 0, 2.0f);
                    }
                };

                int blockStart = -1;
                for (int f = 0; f < (int)frames.size() && f <= totalFrames; f++) {
                    const char* keys = frames[f].keys;
                    bool isActive = false;
                    if (t == 0) isActive = (keys[0] == 'L');
                    else if (t == 1) isActive = (keys[1] == 'R');
                    else if (t == 2) isActive = (keys[5] == 'W');
                    else if (t == 3) isActive = (keys[6] == 'S');
                    else if (t == 4) isActive = (keys[2] == 'J');
                    else if (t == 5) isActive = (keys[3] == 'D');
                    else if (t == 6) isActive = (keys[4] == 'C');

                    if (isActive) {
                        if (blockStart == -1) blockStart = f;
                    } else {
                        if (blockStart != -1) {
                            RenderAndHandleBlock(t, blockStart, f);
                            blockStart = -1;
                        }
                    }
                }
                if (blockStart != -1) RenderAndHandleBlock(t, blockStart, (int)frames.size());
            }
        }

        // プレイヘッド描画
        float playheadX = p0.x + trackHeaderWidth + currentFrame * timelineZoom_;
        drawList->AddLine(ImVec2(playheadX, p0.y), ImVec2(playheadX, p0.y + totalCanvasHeight), IM_COL32(255, 255, 255, 240), 2.0f);
        drawList->AddCircleFilled(ImVec2(playheadX, p0.y + 10.0f), 8.0f, IM_COL32(160, 32, 240, 255));
        drawList->AddCircleFilled(ImVec2(playheadX, p0.y + 10.0f), 5.0f, IM_COL32(255, 255, 255, 255));

        // トラックヘッダー固定描画
        drawList->AddRectFilled(ImVec2(headerLeftX, p0.y), ImVec2(headerLeftX + trackHeaderWidth, p0.y + rulerHeight), IM_COL32(30, 25, 45, 255));
        drawList->AddLine(ImVec2(headerLeftX + trackHeaderWidth - 1.0f, p0.y), ImVec2(headerLeftX + trackHeaderWidth - 1.0f, p0.y + totalCanvasHeight), IM_COL32(60, 50, 80, 255), 1.5f);
        for (int t = 0; t < numTracks; t++) {
            float trackY = p0.y + rulerHeight + t * (trackHeight + trackPadding) + 4.0f;
            drawList->AddRectFilled(ImVec2(headerLeftX + 2.0f, trackY), ImVec2(headerLeftX + trackHeaderWidth - 5.0f, trackY + trackHeight), IM_COL32(40, 35, 55, 255), 4.0f);
            drawList->AddText(ImVec2(headerLeftX + 10.0f, trackY + 3.0f), IM_COL32(230, 225, 250, 255), trackNames[t]);
        }

        // ドラッグ移動処理
        if (pendingBlockDragMode_ != ReplayBlockDragMode::None && replayBlockDragMode_ == ReplayBlockDragMode::None) {
            if (ImGui::IsMouseDown(0)) {
                if (std::hypot(mousePos.x - dragStartMousePos_.x, mousePos.y - dragStartMousePos_.y) >= 4.0f) {
                    replayBlockDragMode_ = pendingBlockDragMode_;
                    pendingBlockDragMode_ = ReplayBlockDragMode::None;
                }
            } else {
                pendingBlockDragMode_ = ReplayBlockDragMode::None;
            }
        }
        if (replayBlockDragMode_ != ReplayBlockDragMode::None) {
            int currentMouseFrame = (std::max)(0, (int)((mousePos.x - (p0.x + trackHeaderWidth)) / timelineZoom_ + 0.5f));
            int frameDelta = currentMouseFrame - dragStartMouseFrame_;
            int pStart = draggingBlockOriginal_.startFrame;
            int pEnd = draggingBlockOriginal_.endFrame;

            if (replayBlockDragMode_ == ReplayBlockDragMode::Move) {
                int dur = draggingBlockOriginal_.endFrame - draggingBlockOriginal_.startFrame;
                pStart = (std::max)(0, draggingBlockOriginal_.startFrame + frameDelta);
                pEnd = pStart + dur;
            } else if (replayBlockDragMode_ == ReplayBlockDragMode::ResizeLeft) {
                pStart = std::clamp(draggingBlockOriginal_.startFrame + frameDelta, 0, draggingBlockOriginal_.endFrame - 1);
            } else if (replayBlockDragMode_ == ReplayBlockDragMode::ResizeRight) {
                pEnd = (std::max)(draggingBlockOriginal_.startFrame + 1, draggingBlockOriginal_.endFrame + frameDelta);
            }

            if (ImGui::IsMouseReleased(0)) {
                ReplayData oldData = replayDragOldReplayData_;
                SelectedReplayBlock oldBlock = replayDragOldSelectedBlock_;
                ReplayManager::GetInstance()->ModifyBlockRange(draggingBlockOriginal_.trackIdx, draggingBlockOriginal_.startFrame, draggingBlockOriginal_.endFrame, pStart, pEnd);
                selectedReplayBlock_ = { draggingBlockOriginal_.trackIdx, pStart, pEnd };

                ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
                SelectedReplayBlock newBlock = selectedReplayBlock_;
                if (cmdManager) {
                    cmdManager->PushActionCommand(
                        [oldData, oldBlock, this]() {
                            auto r = ReplayManager::GetInstance();
                            r->SetCurrentReplay(oldData);
                            r->RebuildMmlFromCurrentFrames();
                            this->selectedReplayBlock_ = oldBlock;
                        },
                        [newData, newBlock, this]() {
                            auto r = ReplayManager::GetInstance();
                            r->SetCurrentReplay(newData);
                            r->RebuildMmlFromCurrentFrames();
                            this->selectedReplayBlock_ = newBlock;
                        }
                    );
                }
                replayBlockDragMode_ = ReplayBlockDragMode::None;
            }
        }

        // Deleteキーでの削除
        if (selectedReplayBlock_.IsValid() && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            ReplayData oldData = ReplayManager::GetInstance()->GetCurrentReplay();
            SelectedReplayBlock oldBlock = selectedReplayBlock_;
            ReplayManager::GetInstance()->DeleteBlockRange(selectedReplayBlock_.trackIdx, selectedReplayBlock_.startFrame, selectedReplayBlock_.endFrame);
            selectedReplayBlock_.Clear();

            ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
            SelectedReplayBlock newBlock = selectedReplayBlock_;
            if (cmdManager) {
                cmdManager->PushActionCommand(
                    [oldData, oldBlock, this]() {
                        auto r = ReplayManager::GetInstance();
                        r->SetCurrentReplay(oldData);
                        r->RebuildMmlFromCurrentFrames();
                        this->selectedReplayBlock_ = oldBlock;
                    },
                    [newData, newBlock, this]() {
                        auto r = ReplayManager::GetInstance();
                        r->SetCurrentReplay(newData);
                        r->RebuildMmlFromCurrentFrames();
                        this->selectedReplayBlock_ = newBlock;
                    }
                );
            }
        }

        // Ctrlスクロール・選択解除
        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton("##TimelineCanvasButton", ImVec2(contentWidth, totalCanvasHeight));
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyCtrl) {
            ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyCtrl) {
            if (replayBlockDragMode_ == ReplayBlockDragMode::None && pendingBlockDragMode_ == ReplayBlockDragMode::None) {
                selectedReplayBlock_.Clear();
            }
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

void EditorReplayUI::DrawReplaySettingsInspector(EditorManager* editorManager, EditorCommandManager* cmdManager, SceneManager* sceneManager) {
    (void)editorManager;
    (void)cmdManager;
    (void)sceneManager;
    // 物理A*やリプレイ設定のインスペクター描画
}

#endif // USE_IMGUI
