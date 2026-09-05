#ifdef USE_IMGUI
#include "GPUParticleEditorViewport.h"
#include "GPUParticleEditorContext.h"
#include "Scene/SceneManager.h"
#include "Core/Utility/TransformFunctions.h"
#include <imgui_internal.h>
#include <cmath>
#include <numbers>
#include <algorithm>

void GPUParticleEditorViewport::Initialize() {
    isCameraSnapLerping_ = false;
}

void GPUParticleEditorViewport::DrawMainView(SceneManager* /*sceneManager*/, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, GPUParticleEditorContext* context) {
    if (!context) return;

    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    if (contentSize.x <= 10.0f || contentSize.y <= 10.0f) return;

    float aspect = 1280.0f / 720.0f;
    float windowAspect = contentSize.x / contentSize.y;
    ImVec2 imageSize;
    if (windowAspect > aspect) {
        imageSize.y = contentSize.y;
        imageSize.x = contentSize.y * aspect;
    } else {
        imageSize.x = contentSize.x;
        imageSize.y = contentSize.x / aspect;
    }

    ImVec2 currentPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));
    ImVec2 vpPos = ImGui::GetCursorScreenPos();
    ImVec2 vpSize = imageSize;

    // 3Dレンダリング結果のテクスチャ表示
    ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

    Camera* cam = activeCamera ? *activeCamera : nullptr;

    // カメラ軸スナップの線形補間更新
    if (cam && isCameraSnapLerping_) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.MouseDown[1] || io.MouseDown[2] || io.MouseWheel != 0.0f) {
            isCameraSnapLerping_ = false;
        } else {
            float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;
            cameraSnapLerpTimer_ += dt;
            float t = std::clamp(cameraSnapLerpTimer_ / cameraSnapLerpDuration_, 0.0f, 1.0f);
            float easeT = t * t * (3.0f - 2.0f * t);

            Vector3 curRot = {
                cameraSnapStartRot_.x + (cameraSnapEndRot_.x - cameraSnapStartRot_.x) * easeT,
                cameraSnapStartRot_.y + (cameraSnapEndRot_.y - cameraSnapStartRot_.y) * easeT,
                cameraSnapStartRot_.z + (cameraSnapEndRot_.z - cameraSnapStartRot_.z) * easeT
            };
            Vector3 curPos = {
                cameraSnapStartPos_.x + (cameraSnapEndPos_.x - cameraSnapStartPos_.x) * easeT,
                cameraSnapStartPos_.y + (cameraSnapEndPos_.y - cameraSnapStartPos_.y) * easeT,
                cameraSnapStartPos_.z + (cameraSnapEndPos_.z - cameraSnapStartPos_.z) * easeT
            };
            cam->SetRotation(curRot);
            cam->SetTranslation(curPos);
            cam->UpdateMatrix();

            if (t >= 1.0f) {
                isCameraSnapLerping_ = false;
            }
        }
    }

    // 発生形状ギズモ
    if (cam && context->GetShowShapeGizmo()) {
        DrawShapeGizmo(cam, vpPos, vpSize, context);
    }

    // カメラ向きギズモ
    if (cam) {
        DrawCameraOrientationGizmo(cam, vpPos, vpSize);
    }

    // 上部再生コントロール HUD
    DrawHUD(context, vpPos, vpSize);

    // 右上ステータス HUD
    DrawStatsHUD(context, vpPos, vpSize);
}

void GPUParticleEditorViewport::DrawHUD(GPUParticleEditorContext* context, ImVec2 vpPos, ImVec2 /*vpSize*/) {
    ImGui::SetCursorScreenPos(ImVec2(vpPos.x + 12.0f, vpPos.y + 12.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.13f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    if (ImGui::BeginChild("##GPUParticleHUD", ImVec2(660, 42), true, ImGuiWindowFlags_NoScrollbar)) {
        // エフェクト選択コンボ (選択されたら即座に自動ロード)
        context->ScanParticleFiles();
        const auto& files = context->GetAvailableParticleFiles();
        std::string currentStem = std::filesystem::path(context->GetCurrentFilePath()).stem().string();
        if (currentStem.empty()) currentStem = "新規エフェクト";

        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo("##HudParticleCombo", currentStem.c_str())) {
            if (files.empty()) {
                ImGui::Selectable("(保存ファイルなし)", false, ImGuiSelectableFlags_Disabled);
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
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("パーティクル切替（選択で自動ロード）");
        ImGui::SameLine();

        auto system = context->GetSystem();
        bool isPlaying = system ? system->IsPlaying() : false;

        // 再生 / 一時停止
        if (isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.35f, 0.20f, 1.0f));
            if (ImGui::Button("[||] 一時停止", ImVec2(95, 26))) {
                if (system) system->Pause();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.70f, 0.35f, 1.0f));
            if (ImGui::Button("[>] 再生", ImVec2(95, 26))) {
                if (system) system->Play();
            }
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();

        // 最初から
        if (ImGui::Button("[<<] リスタート", ImVec2(100, 26))) {
            if (system) system->Restart();
        }
        ImGui::SameLine();

        // 単発バースト
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.25f, 0.85f, 1.0f));
        if (ImGui::Button("[Burst] バースト", ImVec2(100, 26))) {
            if (system) system->TriggerBurstAll();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // 速度スライダー
        ImGui::SetNextItemWidth(90.0f);
        float speed = context->GetPlaybackSpeed();
        if (ImGui::SliderFloat("##Speed", &speed, 0.1f, 2.0f, "%.1fx")) {
            context->SetPlaybackSpeed(speed);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生速度 (0.1x ~ 2.0x)");
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

void GPUParticleEditorViewport::DrawStatsHUD(GPUParticleEditorContext* context, ImVec2 vpPos, ImVec2 vpSize) {
    ImGui::SetCursorScreenPos(ImVec2(vpPos.x + vpSize.x - 220.0f, vpPos.y + 12.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 0.75f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    if (ImGui::BeginChild("##GPUParticleStats", ImVec2(208, 68), true, ImGuiWindowFlags_NoScrollbar)) {
        auto system = context->GetSystem();
        uint32_t activeCount = system ? system->GetTotalActiveParticles() : 0;
        uint32_t maxCount = system ? system->GetTotalMaxParticles() : 0;
        float curTime = system ? system->GetCurrentTime() : 0.0f;
        float duration = system ? system->GetData().duration : 0.0f;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "粒子数: %u / %u", activeCount, maxCount);
        ImGui::Text("時間: %.2fs / %.2fs", curTime, duration);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "FPS: %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void GPUParticleEditorViewport::DrawShapeGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize, GPUParticleEditorContext* context) {
    auto emitter = context->GetSelectedEmitter();
    if (!emitter || !activeCamera) return;

    const auto& data = emitter->GetData();
    Matrix4x4 viewProj = TransformFunctions::Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    auto worldToScreen = [&](const Vector3& p, ImVec2& outScreen) -> bool {
        Vector4 clip = {
            p.x * viewProj.m[0][0] + p.y * viewProj.m[1][0] + p.z * viewProj.m[2][0] + viewProj.m[3][0],
            p.x * viewProj.m[0][1] + p.y * viewProj.m[1][1] + p.z * viewProj.m[2][1] + viewProj.m[3][1],
            p.x * viewProj.m[0][2] + p.y * viewProj.m[1][2] + p.z * viewProj.m[2][2] + viewProj.m[3][2],
            p.x * viewProj.m[0][3] + p.y * viewProj.m[1][3] + p.z * viewProj.m[2][3] + viewProj.m[3][3]
        };
        if (clip.w <= 0.05f) return false;
        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;
        outScreen.x = vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x;
        outScreen.y = vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y;
        return true;
    };

    ImU32 gizmoCol = IM_COL32(240, 180, 50, 200);

    if (data.shape == GPUParticleSpawnShape::Sphere) {
        // 球の3軸ワイヤーフレームリング
        const int segments = 32;
        for (int axis = 0; axis < 3; ++axis) {
            ImVec2 prevScreen;
            bool prevValid = false;
            for (int s = 0; s <= segments; ++s) {
                float angle = (float)s / (float)segments * 2.0f * std::numbers::pi_v<float>;
                Vector3 p = { 0.0f, 0.0f, 0.0f };
                if (axis == 0) { p.x = std::cos(angle) * data.shapeRadius; p.z = std::sin(angle) * data.shapeRadius; }
                else if (axis == 1) { p.x = std::cos(angle) * data.shapeRadius; p.y = std::sin(angle) * data.shapeRadius; }
                else { p.y = std::cos(angle) * data.shapeRadius; p.z = std::sin(angle) * data.shapeRadius; }

                ImVec2 scr;
                if (worldToScreen(p, scr)) {
                    if (prevValid) drawList->AddLine(prevScreen, scr, gizmoCol, 1.5f);
                    prevScreen = scr;
                    prevValid = true;
                } else {
                    prevValid = false;
                }
            }
        }
    } else if (data.shape == GPUParticleSpawnShape::Box) {
        // 直方体の12本エッジ
        Vector3 h = { data.shapeBoxSize.x * 0.5f, data.shapeBoxSize.y * 0.5f, data.shapeBoxSize.z * 0.5f };
        Vector3 corners[8] = {
            { -h.x, -h.y, -h.z }, {  h.x, -h.y, -h.z }, {  h.x, -h.y,  h.z }, { -h.x, -h.y,  h.z },
            { -h.x,  h.y, -h.z }, {  h.x,  h.y, -h.z }, {  h.x,  h.y,  h.z }, { -h.x,  h.y,  h.z }
        };
        int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
        };
        for (int e = 0; e < 12; ++e) {
            ImVec2 s0, s1;
            if (worldToScreen(corners[edges[e][0]], s0) && worldToScreen(corners[edges[e][1]], s1)) {
                drawList->AddLine(s0, s1, gizmoCol, 1.5f);
            }
        }
    } else if (data.shape == GPUParticleSpawnShape::Ring) {
        const int segments = 32;
        ImVec2 prevScreen;
        bool prevValid = false;
        for (int s = 0; s <= segments; ++s) {
            float angle = (float)s / (float)segments * 2.0f * std::numbers::pi_v<float>;
            Vector3 p = { std::cos(angle) * data.shapeRadius, 0.0f, std::sin(angle) * data.shapeRadius };
            ImVec2 scr;
            if (worldToScreen(p, scr)) {
                if (prevValid) drawList->AddLine(prevScreen, scr, gizmoCol, 1.5f);
                prevScreen = scr;
                prevValid = true;
            } else {
                prevValid = false;
            }
        }
    }

    drawList->PopClipRect();
}

void GPUParticleEditorViewport::DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!activeCamera) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(vpPos.x + vpSize.x - 45.0f, vpPos.y + 115.0f);
    const float radius = 30.0f;
    const float badgeRadius = 8.0f;

    drawList->AddCircleFilled(center, radius + 8.0f, IM_COL32(25, 25, 30, 140), 24);

    Matrix4x4 viewMat = activeCamera->GetViewMatrix();

    struct AxisInfo {
        Vector3 worldDir;
        bool isPositive;
        ImU32 color;
        ImU32 ringColor;
        char label;
        Vector3 snapRotate;
        Vector3 camOffsetDir;
    };

    AxisInfo axes[6] = {
        { { -1.0f,  0.0f,  0.0f }, false, IM_COL32(180,  50,  50, 180), IM_COL32(230,  70,  70, 240), 'X', { 0.0f, -std::numbers::pi_v<float> * 0.5f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
        { {  1.0f,  0.0f,  0.0f }, true,  IM_COL32(230,  60,  60, 255), IM_COL32(255, 120, 120, 255), 'X', { 0.0f,  std::numbers::pi_v<float> * 0.5f, 0.0f }, {  1.0f, 0.0f, 0.0f } },
        { {  0.0f, -1.0f,  0.0f }, false, IM_COL32( 50, 180,  50, 180), IM_COL32( 70, 230,  70, 240), 'Y', { -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
        { {  0.0f,  1.0f,  0.0f }, true,  IM_COL32( 60, 210,  60, 255), IM_COL32(120, 255, 120, 255), 'Y', {  std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0f } },
        { {  0.0f,  0.0f, -1.0f }, false, IM_COL32( 50,  50, 180, 180), IM_COL32( 70,  70, 230, 240), 'Z', { 0.0f, std::numbers::pi_v<float>, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  0.0f,  0.0f,  1.0f }, true,  IM_COL32( 60,  80, 230, 255), IM_COL32(120, 140, 255, 255), 'Z', { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
    };

    struct ProjectedAxis {
        int index;
        ImVec2 screenPos;
        float depth;
    };
    std::vector<ProjectedAxis> proj(6);

    for (int i = 0; i < 6; ++i) {
        float vx = axes[i].worldDir.x * viewMat.m[0][0] + axes[i].worldDir.y * viewMat.m[1][0] + axes[i].worldDir.z * viewMat.m[2][0];
        float vy = axes[i].worldDir.x * viewMat.m[0][1] + axes[i].worldDir.y * viewMat.m[1][1] + axes[i].worldDir.z * viewMat.m[2][1];
        float vz = axes[i].worldDir.x * viewMat.m[0][2] + axes[i].worldDir.y * viewMat.m[1][2] + axes[i].worldDir.z * viewMat.m[2][2];

        proj[i].index = i;
        proj[i].screenPos = ImVec2(center.x + vx * radius, center.y - vy * radius);
        proj[i].depth = vz;
    }

    std::sort(proj.begin(), proj.end(), [](const ProjectedAxis& a, const ProjectedAxis& b) {
        return a.depth < b.depth;
    });

    for (const auto& p : proj) {
        const auto& axis = axes[p.index];
        drawList->AddLine(center, p.screenPos, IM_COL32(100, 100, 110, 160), 1.5f);
        drawList->AddCircleFilled(p.screenPos, badgeRadius, axis.color, 16);
        drawList->AddCircle(p.screenPos, badgeRadius, axis.ringColor, 16, 1.5f);
    }
}
#endif
