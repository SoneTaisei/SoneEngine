#ifdef USE_IMGUI
#include "GPUParticleTimeline.h"
#include "GPUParticleEditorContext.h"
#include <imgui_internal.h>
#include <algorithm>

void GPUParticleTimeline::Initialize() {}

void GPUParticleTimeline::DrawTimelineUI(SceneManager* /*sceneManager*/, GPUParticleEditorContext* context) {
    if (!context) return;

    if (ImGui::Begin("GPUパーティクル タイムライン & エミッター階層", &context->GetShowEditor(), ImGuiWindowFlags_NoScrollbar)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float leftWidth = 320.0f;
        float rightWidth = avail.x - leftWidth - 12.0f;
        if (rightWidth < 200.0f) rightWidth = 200.0f;

        // 左ペイン: ファイルI/O & エミッター一覧
        ImGui::BeginChild("##TimelineLeftPane", ImVec2(leftWidth, avail.y), true);
        DrawFileOperations(context);
        ImGui::Separator();
        DrawEmitterList(context);
        ImGui::EndChild();

        ImGui::SameLine();

        // 右ペイン: タイムライン & 時間変化トラック
        ImGui::BeginChild("##TimelineRightPane", ImVec2(rightWidth, avail.y), true);
        DrawTimelineTrack(context, rightWidth);
        ImGui::EndChild();
    }
    ImGui::End();
}

void GPUParticleTimeline::DrawFileOperations(GPUParticleEditorContext* context) {
    auto system = context->GetSystem();
    if (!system) return;

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[エフェクト全体設定]");

    // 既存ファイル読み込みドロップダウン (選択されたら即座に自動ロード)
    context->ScanParticleFiles();
    const auto& files = context->GetAvailableParticleFiles();
    std::string currentStem = std::filesystem::path(context->GetCurrentFilePath()).stem().string();
    if (currentStem.empty()) currentStem = "新規エフェクト";

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("エフェクト##EffectSelectCombo", currentStem.c_str())) {
        if (files.empty()) {
            ImGui::Selectable("(保存されたファイルがありません)", false, ImGuiSelectableFlags_Disabled);
        } else {
            for (const auto& file : files) {
                std::string stem = std::filesystem::path(file).stem().string();
                bool isSel = (context->GetCurrentFilePath() == file);
                if (ImGui::Selectable(stem.c_str(), isSel)) {
                    if (context->GetCurrentFilePath() != file) {
                        context->LoadSystem(file);
                    }
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("保存済みパーティクルを選択（切り替え時に自動ロード）");

    // エフェクト名
    auto& sysData = system->GetData();
    char nameBuf[128];
    strncpy_s(nameBuf, sysData.systemName.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("エフェクト名", nameBuf, sizeof(nameBuf))) {
        sysData.systemName = nameBuf;
    }

    // 総再生時間 & ループ
    ImGui::SetNextItemWidth(90);
    if (ImGui::DragFloat("総時間", &sysData.duration, 0.1f, 0.1f, 60.0f, "%.1fs")) {
        if (sysData.duration < 0.1f) sysData.duration = 0.1f;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("ループ", &sysData.isLoop)) {
        for (size_t i = 0; i < system->GetEmitterCount(); ++i) {
            if (auto em = system->GetEmitter(i)) {
                em->GetData().isLoop = sysData.isLoop;
            }
        }
    }

    ImGui::Spacing();

    // ファイル保存 & 新規
    if (ImGui::Button("[Save] 保存 (JSON)")) {
        context->SaveCurrentSystem();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("現在のエフェクト名 (%s.json) で保存", sysData.systemName.c_str());

    ImGui::SameLine();
    if (ImGui::Button("[New] 新規")) {
        context->CreateNewSystem();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("新規エフェクトを作成（停止状態で待機）");
}

void GPUParticleTimeline::DrawEmitterList(GPUParticleEditorContext* context) {
    auto system = context->GetSystem();
    if (!system) return;

    ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.4f, 1.0f), "[エミッター階層 (%zu個)]", system->GetEmitterCount());

    int selectedIdx = context->GetSelectedEmitterIndex();

    for (size_t i = 0; i < system->GetEmitterCount(); ++i) {
        auto emitter = system->GetEmitter(i);
        if (!emitter) continue;
        auto& data = emitter->GetData();

        ImGui::PushID(static_cast<int>(i));

        // 👁 表示/非表示
        bool visible = data.enabled;
        if (ImGui::Checkbox("##Vis", &visible)) {
            data.enabled = visible;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("有効 / 無効");
        ImGui::SameLine();

        // S (Solo)
        bool solo = data.solo;
        if (solo) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.1f, 1.0f));
        if (ImGui::Button(solo ? "[S]" : " S ", ImVec2(24, 20))) {
            data.solo = !solo;
        }
        if (solo) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("ソロ再生 (このエミッターのみ再生)");
        ImGui::SameLine();

        // M (Mute)
        bool mute = data.mute;
        if (mute) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(mute ? "[M]" : " M ", ImVec2(24, 20))) {
            data.mute = !mute;
        }
        if (mute) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("ミュート (消音・非表示)");
        ImGui::SameLine();

        // アイコン & 名前ボタン（選択）
        const char* typeIcon = (data.renderType == GPUParticleRenderType::Mesh) ? "[3D]" : "[2D]";
        std::string label = std::string(typeIcon) + " " + data.name;
        bool isSelected = (selectedIdx == static_cast<int>(i));

        if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(100, 20))) {
            context->SetSelectedEmitterIndex(static_cast<int>(i));
        }
        ImGui::SameLine();

        // 上下順
        if (ImGui::Button("↑", ImVec2(18, 20))) {
            system->MoveEmitterUp(i);
            if (selectedIdx == static_cast<int>(i)) context->SetSelectedEmitterIndex(static_cast<int>(i - 1));
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        if (ImGui::Button("↓", ImVec2(18, 20))) {
            system->MoveEmitterDown(i);
            if (selectedIdx == static_cast<int>(i)) context->SetSelectedEmitterIndex(static_cast<int>(i + 1));
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();

        // 複製
        if (ImGui::Button("+", ImVec2(18, 20))) {
            context->PushUndoState("Duplicate Emitter");
            system->DuplicateEmitter(i);
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();

        // 削除
        if (system->GetEmitterCount() > 1) {
            if (ImGui::Button("x", ImVec2(18, 20))) {
                context->PushUndoState("Remove Emitter");
                system->RemoveEmitter(i);
                if (selectedIdx >= static_cast<int>(system->GetEmitterCount())) {
                    context->SetSelectedEmitterIndex(static_cast<int>(system->GetEmitterCount()) - 1);
                }
                ImGui::PopID();
                break;
            }
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::Button("[+] エミッター追加", ImVec2(180, 24))) {
        context->PushUndoState("Add Emitter");
        GPUParticleEmitterData newEmitter;
        newEmitter.name = "Emitter_" + std::to_string(system->GetEmitterCount() + 1);
        system->AddEmitter(newEmitter);
        context->SetSelectedEmitterIndex(static_cast<int>(system->GetEmitterCount() - 1));
    }
}

void GPUParticleTimeline::DrawTimelineTrack(GPUParticleEditorContext* context, float totalWidth) {
    auto system = context->GetSystem();
    if (!system) return;

    float duration = system->GetData().duration;
    if (duration <= 0.01f) duration = 1.0f;
    float curTime = system->GetCurrentTime();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    float trackWidth = totalWidth - 40.0f;
    if (trackWidth < 100.0f) trackWidth = 100.0f;

    // 時間軸ルーラー (目盛り)
    float rulerHeight = 24.0f;
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + trackWidth, canvasPos.y + rulerHeight), IM_COL32(35, 35, 40, 255));

    int numSteps = 10;
    for (int i = 0; i <= numSteps; ++i) {
        float frac = (float)i / (float)numSteps;
        float x = canvasPos.x + frac * trackWidth;
        drawList->AddLine(ImVec2(x, canvasPos.y + rulerHeight - 8.0f), ImVec2(x, canvasPos.y + rulerHeight), IM_COL32(120, 120, 130, 255));

        char timeText[16];
        snprintf(timeText, sizeof(timeText), "%.1fs", frac * duration);
        drawList->AddText(ImVec2(x + 2.0f, canvasPos.y + 2.0f), IM_COL32(180, 180, 180, 255), timeText);
    }

    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + rulerHeight + 6.0f));

    // エミッター別タイムラインバー
    float barHeight = 26.0f;
    float currentY = canvasPos.y + rulerHeight + 10.0f;

    for (size_t i = 0; i < system->GetEmitterCount(); ++i) {
        auto emitter = system->GetEmitter(i);
        if (!emitter) continue;
        const auto& data = emitter->GetData();

        float startFrac = std::clamp(data.startDelay / duration, 0.0f, 1.0f);
        float endFrac = std::clamp((data.startDelay + data.duration) / duration, 0.0f, 1.0f);

        float barStartX = canvasPos.x + startFrac * trackWidth;
        float barEndX = canvasPos.x + endFrac * trackWidth;
        if (barEndX < barStartX + 6.0f) barEndX = barStartX + 6.0f;

        bool isSelected = (context->GetSelectedEmitterIndex() == static_cast<int>(i));
        ImU32 barCol = isSelected ? IM_COL32(70, 140, 220, 220) : IM_COL32(60, 70, 85, 180);
        ImU32 borderCol = isSelected ? IM_COL32(130, 200, 255, 255) : IM_COL32(90, 100, 120, 200);

        drawList->AddRectFilled(ImVec2(barStartX, currentY), ImVec2(barEndX, currentY + barHeight), barCol, 4.0f);
        drawList->AddRect(ImVec2(barStartX, currentY), ImVec2(barEndX, currentY + barHeight), borderCol, 4.0f, 0, 1.5f);

        // エミッター名描画
        drawList->AddText(ImVec2(barStartX + 6.0f, currentY + 4.0f), IM_COL32(255, 255, 255, 240), data.name.c_str());

        // バーストキーのマーカー描画
        for (const auto& b : data.bursts) {
            float burstTime = data.startDelay + b.time;
            float bFrac = std::clamp(burstTime / duration, 0.0f, 1.0f);
            float bx = canvasPos.x + bFrac * trackWidth;
            drawList->AddCircleFilled(ImVec2(bx, currentY + barHeight * 0.5f), 5.0f, IM_COL32(255, 200, 50, 255));
            drawList->AddCircle(ImVec2(bx, currentY + barHeight * 0.5f), 5.0f, IM_COL32(20, 20, 20, 255), 12, 1.0f);
        }

        currentY += barHeight + 6.0f;
    }

    // 再生ヘッド (シークバー)
    float headFrac = std::clamp(curTime / duration, 0.0f, 1.0f);
    float headX = canvasPos.x + headFrac * trackWidth;
    float totalTrackHeight = (rulerHeight + 10.0f) + system->GetEmitterCount() * (barHeight + 6.0f) + 30.0f;

    drawList->AddLine(ImVec2(headX, canvasPos.y), ImVec2(headX, canvasPos.y + totalTrackHeight), IM_COL32(255, 80, 80, 255), 2.0f);
    drawList->AddTriangleFilled(
        ImVec2(headX - 6.0f, canvasPos.y),
        ImVec2(headX + 6.0f, canvasPos.y),
        ImVec2(headX, canvasPos.y + 10.0f),
        IM_COL32(255, 80, 80, 255)
    );

    // クリック & ドラッグでシーク
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##TimelineScrubber", ImVec2(trackWidth, totalTrackHeight));
    if (ImGui::IsItemActive()) {
        float mouseX = ImGui::GetIO().MousePos.x;
        float newFrac = std::clamp((mouseX - canvasPos.x) / trackWidth, 0.0f, 1.0f);
        system->SetCurrentTime(newFrac * duration);
    }

    // 下部カラーグラデーションバー (選択中エミッター)
    auto selectedEmitter = context->GetSelectedEmitter();
    if (selectedEmitter) {
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + totalTrackHeight + 10.0f));
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "選択エミッターのカラー変化 (0%% ~ 100%%):");

        ImVec2 gradPos = ImGui::GetCursorScreenPos();
        float gradWidth = trackWidth;
        float gradHeight = 20.0f;

        const auto& edata = selectedEmitter->GetData();
        ImU32 colStart = IM_COL32(
            static_cast<int>(edata.startColor.x * 255),
            static_cast<int>(edata.startColor.y * 255),
            static_cast<int>(edata.startColor.z * 255),
            static_cast<int>(edata.startColor.w * 255)
        );
        ImU32 colEnd = IM_COL32(
            static_cast<int>(edata.endColor.x * 255),
            static_cast<int>(edata.endColor.y * 255),
            static_cast<int>(edata.endColor.z * 255),
            static_cast<int>(edata.endColor.w * 255)
        );

        // グラデーション短冊
        drawList->AddRectFilledMultiColor(gradPos, ImVec2(gradPos.x + gradWidth, gradPos.y + gradHeight), colStart, colEnd, colEnd, colStart);
        drawList->AddRect(gradPos, ImVec2(gradPos.x + gradWidth, gradPos.y + gradHeight), IM_COL32(100, 100, 100, 255));
    }
}
#endif
