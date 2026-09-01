#ifdef USE_IMGUI
#include "Model3DEditorViewport.h"
#include "Model3DEditorContext.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/Camera.h"
#include "Input/KeyboardInput.h"
#include "Scene/SceneManager.h"
#include <imgui_internal.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>

namespace {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kHalfPi = 1.5707963267948966f;
}

Model3DEditorViewport::Model3DEditorViewport(Model3DEditorContext* context)
    : context_(context) {
}

void Model3DEditorViewport::Initialize() {
}

void Model3DEditorViewport::Draw(
    bool& showViewport,
    SceneManager* sceneManager,
    Camera** activeCamera,
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
    std::function<void()> onActive
) {
    if (!showViewport) return;

    if (ImGui::Begin("3Dモデル配置", &showViewport)) {
        isHovered_ = ImGui::IsWindowHovered();
        bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        if (isFocused && onActive) {
            onActive();
        }

        // 1. Content size with aspect ratio maintenance
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x < 50.0f) contentSize.x = 50.0f;
        if (contentSize.y < 50.0f) contentSize.y = 50.0f;

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

        // Render target texture image
        ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

        // Overlay Drawing & Interaction
        Camera* camera = activeCamera ? *activeCamera : nullptr;
        if (camera) {
            Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());

            // 1. Camera Orientation Gizmo (top-right)
            DrawCameraOrientationGizmo(camera, vpPos, vpSize);

            // 2. Transform Gizmo for selected object (S/R/T)
            DrawTransformGizmo(camera, vpPos, vpSize);

            // 3. Object Picking (Click to select)
            if (isHovered_ && !isDraggingGizmo_) {
                HandleObjectPicking(camera, vpPos, vpSize);
            }

            // 4. Drag & Drop Target
            HandleDragAndDrop(camera, vpPos, vpSize);
        }

        // 6. Keyboard Shortcuts (S, R, T, Del, etc.)
        if (isHovered_ || ImGui::IsWindowFocused()) {
            HandleKeyboardShortcuts();
        }

        // 7. Top-left Mode Toolbar Overlay
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 barPos = ImVec2(vpPos.x + 10.0f, vpPos.y + 10.0f);
        drawList->AddRectFilled(barPos, ImVec2(barPos.x + 230.0f, barPos.y + 36.0f), IM_COL32(20, 20, 25, 200), 6.0f);
        drawList->AddRect(barPos, ImVec2(barPos.x + 230.0f, barPos.y + 36.0f), IM_COL32(80, 80, 100, 180), 6.0f);

        ImGui::SetCursorScreenPos(ImVec2(barPos.x + 8.0f, barPos.y + 5.0f));
        if (context_) {
            auto mode = context_->GetGizmoMode();
            bool isT = (mode == Model3DEditorContext::GizmoMode::Translation);
            bool isR = (mode == Model3DEditorContext::GizmoMode::Rotation);
            bool isS = (mode == Model3DEditorContext::GizmoMode::Scale);

            if (isT) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button("[T] 移動", ImVec2(65, 24))) context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Translation);
            if (isT) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (isR) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button("[R] 回転", ImVec2(65, 24))) context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Rotation);
            if (isR) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (isS) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button("[S] 拡大", ImVec2(65, 24))) context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Scale);
            if (isS) ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

void Model3DEditorViewport::DrawViewportGrid(const Matrix4x4& viewProjectionMatrix, ImVec2 vpPos, ImVec2 vpSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    const float gridExtent = 100.0f; // どこまでも伸びる広域グリッド
    const float gridStep = 1.0f;
    const float nearW = 0.05f; // Nearクリップ閾値
    const float gridY = 0.005f; // 床オブジェクト等との干渉を避ける高さ

    // 同次クリップ座標の計算
    auto transformToClip = [&](const Vector3& p, Vector4& outClip) {
        outClip.x = p.x * viewProjectionMatrix.m[0][0] + p.y * viewProjectionMatrix.m[1][0] + p.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
        outClip.y = p.x * viewProjectionMatrix.m[0][1] + p.y * viewProjectionMatrix.m[1][1] + p.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
        outClip.z = p.x * viewProjectionMatrix.m[0][2] + p.y * viewProjectionMatrix.m[1][2] + p.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2];
        outClip.w = p.x * viewProjectionMatrix.m[0][3] + p.y * viewProjectionMatrix.m[1][3] + p.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
    };

    // クリップ座標からスクリーン座標への変換
    auto clipToScreen = [&](const Vector4& clip, ImVec2& outP) {
        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;
        outP.x = vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x;
        outP.y = vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y;
    };

    // 3D線分のクリッピング＆描画
    auto drawSegment3D = [&](Vector3 p1, Vector3 p2, ImU32 col, float thickness) {
        Vector4 c1, c2;
        transformToClip(p1, c1);
        transformToClip(p2, c2);

        // 両方ともカメラ背後の場合はスキップ
        if (c1.w < nearW && c2.w < nearW) return;

        // 片方がカメラ背後にある場合、Near平面 (w = nearW) でクリップ
        if (c1.w < nearW) {
            float t = (nearW - c1.w) / (c2.w - c1.w);
            p1 = { p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t, p1.z + (p2.z - p1.z) * t };
            transformToClip(p1, c1);
        } else if (c2.w < nearW) {
            float t = (nearW - c2.w) / (c1.w - c2.w);
            p2 = { p2.x + (p1.x - p2.x) * t, p2.y + (p1.y - p2.y) * t, p2.z + (p1.z - p2.z) * t };
            transformToClip(p2, c2);
        }

        if (c1.w < nearW || c2.w < nearW) return;

        ImVec2 s1, s2;
        clipToScreen(c1, s1);
        clipToScreen(c2, s2);
        drawList->AddLine(s1, s2, col, thickness);
    };

    // 1. 通常グリッド線 (XZ平面) - 1.0mセグメント分割で安定描画 (アニメーションエディターと完全統一)
    ImU32 gridCol = IM_COL32(75, 75, 80, 140);
    for (float x = -gridExtent; x <= gridExtent; x += gridStep) {
        if (std::abs(x) < 0.001f) continue; // Z軸(x=0)は後で強調描画
        for (float z = -gridExtent; z < gridExtent; z += gridStep) {
            drawSegment3D(Vector3{ x, gridY, z }, Vector3{ x, gridY, z + gridStep }, gridCol, 1.0f);
        }
    }
    for (float z = -gridExtent; z <= gridExtent; z += gridStep) {
        if (std::abs(z) < 0.001f) continue; // X軸(z=0)は後で強調描画
        for (float x = -gridExtent; x < gridExtent; x += gridStep) {
            drawSegment3D(Vector3{ x, gridY, z }, Vector3{ x + gridStep, gridY, z }, gridCol, 1.0f);
        }
    }

    // 2. 0のライン強調 (X軸: 赤, Z軸: 青) - 1.0mセグメント分割で安定描画
    ImU32 xAxisCol = IM_COL32(230, 60, 75, 230);
    for (float x = -gridExtent; x < gridExtent; x += gridStep) {
        drawSegment3D(Vector3{ x, gridY, 0.0f }, Vector3{ x + gridStep, gridY, 0.0f }, xAxisCol, 2.0f);
    }

    ImU32 zAxisCol = IM_COL32(60, 140, 230, 230);
    for (float z = -gridExtent; z < gridExtent; z += gridStep) {
        drawSegment3D(Vector3{ 0.0f, gridY, z }, Vector3{ 0.0f, gridY, z + gridStep }, zAxisCol, 2.0f);
    }

    drawList->PopClipRect();
}

void Model3DEditorViewport::DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!activeCamera) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(vpPos.x + vpSize.x - 55.0f, vpPos.y + 55.0f);
    const float radius = 36.0f;
    const float badgeRadius = 9.5f;

    drawList->AddCircleFilled(center, radius + 12.0f, IM_COL32(30, 30, 35, 130), 32);

    Matrix4x4 viewMat = activeCamera->GetViewMatrix();

    struct AxisInfo {
        Vector3 worldDir;
        bool isPositive;
        ImU32 color;
        ImU32 ringColor;
        char label;
        Vector3 snapRotate;
        Vector3 camOffsetDir;
        Vector3 camDir;
        ImVec2 screenPos;
        bool isHovered;
    };

    AxisInfo axes[6] = {
        { {  1.0f,  0.0f,  0.0f }, true,  IM_COL32(235, 65,  75,  255), IM_COL32(235, 65,  75,  180), 'X', { 0.0f, -kHalfPi, 0.0f }, {  1.0f, 0.0f, 0.0f }, {}, {}, false },
        { { -1.0f,  0.0f,  0.0f }, false, IM_COL32(235, 65,  75,  140), IM_COL32(235, 65,  75,  200), ' ', { 0.0f,  kHalfPi, 0.0f }, { -1.0f, 0.0f, 0.0f }, {}, {}, false },
        { {  0.0f,  1.0f,  0.0f }, true,  IM_COL32(130, 200, 45,  255), IM_COL32(130, 200, 45,  180), 'Y', { kHalfPi - 0.001f, 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0001f }, {}, {}, false },
        { {  0.0f, -1.0f,  0.0f }, false, IM_COL32(130, 200, 45,  140), IM_COL32(130, 200, 45,  200), ' ', { -kHalfPi + 0.001f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0001f }, {}, {}, false },
        { {  0.0f,  0.0f,  1.0f }, true,  IM_COL32(50,  135, 245, 255), IM_COL32(50,  135, 245, 180), 'Z', { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, {}, {}, false },
        { {  0.0f,  0.0f, -1.0f }, false, IM_COL32(50,  135, 245, 140), IM_COL32(50,  135, 245, 200), ' ', { 0.0f, kPi, 0.0f }, { 0.0f, 0.0f,  1.0f }, {}, {}, false }
    };

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    for (int i = 0; i < 6; ++i) {
        axes[i].camDir.x = axes[i].worldDir.x * viewMat.m[0][0] + axes[i].worldDir.y * viewMat.m[1][0] + axes[i].worldDir.z * viewMat.m[2][0];
        axes[i].camDir.y = axes[i].worldDir.x * viewMat.m[0][1] + axes[i].worldDir.y * viewMat.m[1][1] + axes[i].worldDir.z * viewMat.m[2][1];
        axes[i].camDir.z = axes[i].worldDir.x * viewMat.m[0][2] + axes[i].worldDir.y * viewMat.m[1][2] + axes[i].worldDir.z * viewMat.m[2][2];

        axes[i].screenPos = ImVec2(
            center.x + axes[i].camDir.x * radius,
            center.y - axes[i].camDir.y * radius
        );
    }

    std::vector<int> sortedIndices = { 0, 1, 2, 3, 4, 5 };
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
        return axes[a].camDir.z < axes[b].camDir.z;
    });

    int clickedAxisIdx = -1;
    for (int i = 5; i >= 0; --i) {
        int idx = sortedIndices[i];
        float dx = mousePos.x - axes[idx].screenPos.x;
        float dy = mousePos.y - axes[idx].screenPos.y;
        if (dx * dx + dy * dy <= (badgeRadius + 2.5f) * (badgeRadius + 2.5f)) {
            axes[idx].isHovered = true;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                clickedAxisIdx = idx;
            }
            break;
        }
    }

    if (clickedAxisIdx >= 0) {
        if (axes[clickedAxisIdx].camDir.z > 0.70f) {
            int oppIdx = (clickedAxisIdx % 2 == 0) ? (clickedAxisIdx + 1) : (clickedAxisIdx - 1);
            clickedAxisIdx = oppIdx;
        }

        const auto& snapAxis = axes[clickedAxisIdx];
        Vector3 target = { 0.0f, 0.0f, 0.0f };
        if (context_ && context_->GetSelectedObject()) {
            target = context_->GetSelectedObject()->GetTranslation();
        }

        Vector3 curCamPos = activeCamera->GetTranslation();
        Vector3 diff = { curCamPos.x - target.x, curCamPos.y - target.y, curCamPos.z - target.z };
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (dist < 2.0f || dist > 50.0f) dist = 10.0f;

        cameraSnapStartRot_ = activeCamera->GetRotation();
        cameraSnapStartPos_ = curCamPos;

        auto normalizeAngleDiff = [](float start, float target) {
            float diff = target - start;
            while (diff > kPi) { start += 2.0f * kPi; diff = target - start; }
            while (diff < -kPi) { start -= 2.0f * kPi; diff = target - start; }
            return start;
        };
        cameraSnapStartRot_.x = normalizeAngleDiff(cameraSnapStartRot_.x, snapAxis.snapRotate.x);
        cameraSnapStartRot_.y = normalizeAngleDiff(cameraSnapStartRot_.y, snapAxis.snapRotate.y);
        cameraSnapStartRot_.z = normalizeAngleDiff(cameraSnapStartRot_.z, snapAxis.snapRotate.z);

        cameraSnapEndRot_ = snapAxis.snapRotate;
        cameraSnapEndPos_ = {
            target.x + snapAxis.camOffsetDir.x * dist,
            target.y + snapAxis.camOffsetDir.y * dist,
            target.z + snapAxis.camOffsetDir.z * dist
        };
        cameraSnapLerpTimer_ = 0.0f;
        cameraSnapLerpDuration_ = 0.25f;
        isCameraSnapLerping_ = true;
    }

    if (isCameraSnapLerping_) {
        float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;
        cameraSnapLerpTimer_ += dt;
        float t = std::clamp(cameraSnapLerpTimer_ / cameraSnapLerpDuration_, 0.0f, 1.0f);
        float smoothT = t * t * (3.0f - 2.0f * t);

        Vector3 newRot = {
            cameraSnapStartRot_.x + (cameraSnapEndRot_.x - cameraSnapStartRot_.x) * smoothT,
            cameraSnapStartRot_.y + (cameraSnapEndRot_.y - cameraSnapStartRot_.y) * smoothT,
            cameraSnapStartRot_.z + (cameraSnapEndRot_.z - cameraSnapStartRot_.z) * smoothT
        };
        Vector3 newPos = {
            cameraSnapStartPos_.x + (cameraSnapEndPos_.x - cameraSnapStartPos_.x) * smoothT,
            cameraSnapStartPos_.y + (cameraSnapEndPos_.y - cameraSnapStartPos_.y) * smoothT,
            cameraSnapStartPos_.z + (cameraSnapEndPos_.z - cameraSnapStartPos_.z) * smoothT
        };

        activeCamera->SetRotation(newRot);
        activeCamera->SetTranslation(newPos);
        activeCamera->UpdateMatrix();

        if (t >= 1.0f) {
            isCameraSnapLerping_ = false;
        }
    }

    for (int idx : sortedIndices) {
        const auto& axis = axes[idx];
        float curRadius = axis.isHovered ? (badgeRadius + 1.5f) : badgeRadius;

        if (!axis.isPositive) {
            if (axis.isHovered) {
                drawList->AddCircleFilled(axis.screenPos, curRadius * 0.75f, axis.color, 16);
                drawList->AddCircle(axis.screenPos, curRadius * 0.75f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            } else {
                drawList->AddCircle(axis.screenPos, curRadius * 0.75f, axis.ringColor, 16, 1.5f);
            }
        } else {
            drawList->AddLine(center, axis.screenPos, axis.color, 2.5f);
            drawList->AddCircleFilled(axis.screenPos, curRadius, axis.color, 16);
            if (axis.isHovered) {
                drawList->AddCircle(axis.screenPos, curRadius, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            } else {
                drawList->AddCircle(axis.screenPos, curRadius, IM_COL32(255, 255, 255, 120), 16, 1.0f);
            }
            char txt[2] = { axis.label, '\0' };
            ImVec2 txtSz = ImGui::CalcTextSize(txt);
            drawList->AddText(ImVec2(axis.screenPos.x - txtSz.x * 0.5f, axis.screenPos.y - txtSz.y * 0.5f), IM_COL32(255, 255, 255, 255), txt);
        }
    }
}

void Model3DEditorViewport::GetMouseRay(Camera* activeCamera, ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSize, Vector3& outRayOrigin, Vector3& outRayDir) {
    if (!activeCamera || vpSize.x <= 0.0f || vpSize.y <= 0.0f) return;

    float ndcX = (mousePos.x - vpPos.x) / vpSize.x * 2.0f - 1.0f;
    float ndcY = 1.0f - (mousePos.y - vpPos.y) / vpSize.y * 2.0f;

    Matrix4x4 proj = activeCamera->GetProjectionMatrix();
    Matrix4x4 view = activeCamera->GetViewMatrix();

    // View space ray dir
    Vector3 rayView = {
        ndcX / proj.m[0][0],
        ndcY / proj.m[1][1],
        1.0f
    };

    // Inverse view rotation matrix
    Vector3 rayWorld = {
        rayView.x * view.m[0][0] + rayView.y * view.m[0][1] + rayView.z * view.m[0][2],
        rayView.x * view.m[1][0] + rayView.y * view.m[1][1] + rayView.z * view.m[1][2],
        rayView.x * view.m[2][0] + rayView.y * view.m[2][1] + rayView.z * view.m[2][2]
    };

    float len = std::sqrt(rayWorld.x * rayWorld.x + rayWorld.y * rayWorld.y + rayWorld.z * rayWorld.z);
    if (len > 1e-5f) {
        rayWorld.x /= len;
        rayWorld.y /= len;
        rayWorld.z /= len;
    }

    outRayOrigin = activeCamera->GetTranslation();
    outRayDir = rayWorld;
}

void Model3DEditorViewport::HandleObjectPicking(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!context_ || !activeCamera) return;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        if (mousePos.x < vpPos.x || mousePos.x > vpPos.x + vpSize.x || mousePos.y < vpPos.y || mousePos.y > vpPos.y + vpSize.y) {
            return;
        }

        Vector3 rayO, rayD;
        GetMouseRay(activeCamera, mousePos, vpPos, vpSize, rayO, rayD);

        float dist = 0.0f;
        PlacedObject3D* picked = context_->PickObject(rayO, rayD, dist);
        if (picked) {
            context_->SetSelectedObject(picked);
        } else {
            // Click on empty space deselects unless clicking top bar
            if (mousePos.x > vpPos.x + 250.0f || mousePos.y > vpPos.y + 50.0f) {
                // If not clicking near top-right orientation gizmo
                float dGizmo = (mousePos.x - (vpPos.x + vpSize.x - 55.0f)) * (mousePos.x - (vpPos.x + vpSize.x - 55.0f)) + (mousePos.y - (vpPos.y + 55.0f)) * (mousePos.y - (vpPos.y + 55.0f));
                if (dGizmo > 50.0f * 50.0f) {
                    context_->ClearSelection();
                }
            }
        }
    }
}

void Model3DEditorViewport::HandleDragAndDrop(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!context_ || !activeCamera) return;

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_3D_MODEL")) {
            const char* dataStr = (const char*)payload->Data;
            std::stringstream ss(dataStr);
            std::string dirPath, fileName, displayName;
            std::getline(ss, dirPath, '|');
            std::getline(ss, fileName, '|');
            std::getline(ss, displayName, '|');

            ImVec2 mousePos = ImGui::GetIO().MousePos;
            Vector3 rayO, rayD;
            GetMouseRay(activeCamera, mousePos, vpPos, vpSize, rayO, rayD);

            // Intersect with Ground Plane Y = 0
            Vector3 spawnPos = { 0.0f, 0.0f, 0.0f };
            if (std::abs(rayD.y) > 1e-4f) {
                float t = -rayO.y / rayD.y;
                if (t > 0.0f) {
                    spawnPos = { rayO.x + rayD.x * t, 0.0f, rayO.z + rayD.z * t };
                } else {
                    spawnPos = { rayO.x + rayD.x * 5.0f, rayO.y + rayD.y * 5.0f, rayO.z + rayD.z * 5.0f };
                }
            } else {
                spawnPos = { rayO.x + rayD.x * 5.0f, rayO.y + rayD.y * 5.0f, rayO.z + rayD.z * 5.0f };
            }

            // Snap to grid if enabled
            if (context_->IsSnapEnabled()) {
                float s = context_->GetTranslateSnap();
                spawnPos.x = std::round(spawnPos.x / s) * s;
                spawnPos.z = std::round(spawnPos.z / s) * s;
            }

            context_->AddObject(displayName, dirPath, fileName, spawnPos);
        }
        ImGui::EndDragDropTarget();
    }
}

void Model3DEditorViewport::HandleKeyboardShortcuts() {
    if (!context_) return;

    auto keyboard = KeyboardInput::GetInstance();
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl || (keyboard && (keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL)));
    bool shift = io.KeyShift || (keyboard && (keyboard->IsKeyDown(DIK_LSHIFT) || keyboard->IsKeyDown(DIK_RSHIFT)));

    // Undo (Ctrl+Z)
    if (ctrl && !shift && (ImGui::IsKeyPressed(ImGuiKey_Z, false) || (keyboard && keyboard->IsKeyPressed(DIK_Z)))) {
        context_->Undo();
    }
    // Redo (Ctrl+Y or Ctrl+Shift+Z)
    if (((ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (keyboard && keyboard->IsKeyPressed(DIK_Y)))) ||
         (ctrl && shift && (ImGui::IsKeyPressed(ImGuiKey_Z, false) || (keyboard && keyboard->IsKeyPressed(DIK_Z)))))) {
        context_->Redo();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_T) || (keyboard && keyboard->IsKeyPressed(DIK_T))) {
        context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Translation);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R) || (keyboard && keyboard->IsKeyPressed(DIK_R))) {
        context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Rotation);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S) || (keyboard && keyboard->IsKeyPressed(DIK_S))) {
        context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Scale);
    }

    if (context_->GetSelectedObject()) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace) ||
            (keyboard && (keyboard->IsKeyPressed(DIK_DELETE) || keyboard->IsKeyPressed(DIK_BACK)))) {
            context_->RemoveObject(context_->GetSelectedObject());
        }
        if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_D) || (keyboard && keyboard->IsKeyPressed(DIK_D)))) {
            context_->DuplicateObject(context_->GetSelectedObject());
        }
    }
}

void Model3DEditorViewport::DrawTransformGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!context_ || !activeCamera) return;

    PlacedObject3D* sel = context_->GetSelectedObject();
    if (!sel) {
        isDraggingGizmo_ = false;
        gizmoActiveAxis_ = -1;
        return;
    }

    Vector3 origin = sel->GetTranslation();
    Matrix4x4 vpMat = TransformFunctions::Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

    Vector4 clipOrigin;
    clipOrigin.x = origin.x * vpMat.m[0][0] + origin.y * vpMat.m[1][0] + origin.z * vpMat.m[2][0] + vpMat.m[3][0];
    clipOrigin.y = origin.x * vpMat.m[0][1] + origin.y * vpMat.m[1][1] + origin.z * vpMat.m[2][1] + vpMat.m[3][1];
    clipOrigin.z = origin.x * vpMat.m[0][2] + origin.y * vpMat.m[1][2] + origin.z * vpMat.m[2][2] + vpMat.m[3][2];
    clipOrigin.w = origin.x * vpMat.m[0][3] + origin.y * vpMat.m[1][3] + origin.z * vpMat.m[2][3] + vpMat.m[3][3];

    if (clipOrigin.w <= 0.05f) return;

    float ndcX = clipOrigin.x / clipOrigin.w;
    float ndcY = clipOrigin.y / clipOrigin.w;
    ImVec2 screenOrigin = ImVec2(
        vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x,
        vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y
    );

    float gizmoRadius = clipOrigin.w * 0.10f;
    if (gizmoRadius < 20.0f) gizmoRadius = 20.0f;
    if (gizmoRadius > 60.0f) gizmoRadius = 60.0f;

    Vector3 axes[3] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };

    if (context_->GetGizmoSpace() == Model3DEditorContext::GizmoSpace::Local && context_->GetGizmoMode() != Model3DEditorContext::GizmoMode::Rotation) {
        Matrix4x4 rotMat = TransformFunctions::MakeAffineMatrix(Vector3{ 1.0f, 1.0f, 1.0f }, sel->GetRotation(), Vector3{ 0.0f, 0.0f, 0.0f });
        axes[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
        axes[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
        axes[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    auto project = [&](const Vector3& p, ImVec2& outP, float& outW) -> bool {
        Vector4 c;
        c.x = p.x * vpMat.m[0][0] + p.y * vpMat.m[1][0] + p.z * vpMat.m[2][0] + vpMat.m[3][0];
        c.y = p.x * vpMat.m[0][1] + p.y * vpMat.m[1][1] + p.z * vpMat.m[2][1] + vpMat.m[3][1];
        c.z = p.x * vpMat.m[0][2] + p.y * vpMat.m[1][2] + p.z * vpMat.m[2][2] + vpMat.m[3][2];
        c.w = p.x * vpMat.m[0][3] + p.y * vpMat.m[1][3] + p.z * vpMat.m[2][3] + vpMat.m[3][3];
        outW = c.w;
        if (c.w <= 0.05f) return false;
        outP.x = vpPos.x + (c.x / c.w + 1.0f) * 0.5f * vpSize.x;
        outP.y = vpPos.y + (1.0f - c.y / c.w) * 0.5f * vpSize.y;
        return true;
    };

    auto distToSegment = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
        float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
        if (l2 < 1e-4f) return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
        float t = std::clamp(((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2, 0.0f, 1.0f);
        ImVec2 proj = ImVec2(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y));
        return std::sqrt((p.x - proj.x) * (p.x - proj.x) + (p.y - proj.y) * (p.y - proj.y));
    };

    const ImU32 axisColors[3] = {
        IM_COL32(235, 60, 60, 240),
        IM_COL32(60, 220, 60, 240),
        IM_COL32(60, 140, 255, 240)
    };
    const ImU32 axisHoverColors[3] = {
        IM_COL32(255, 140, 140, 255),
        IM_COL32(140, 255, 140, 255),
        IM_COL32(140, 200, 255, 255)
    };

    auto mode = context_->GetGizmoMode();

    const float kGizmoLength = 1.0f; // 1*1*1 (ワールド1単位) の長さ

    // -------------------------------------------------------------
    // Translation Mode (T)
    // -------------------------------------------------------------
    if (mode == Model3DEditorContext::GizmoMode::Translation) {
        ImVec2 screenTips[3];
        bool tipsValid[3] = { false, false, false };
        int hoveredAxis = -1;
        float minD = 10.0f;

        for (int i = 0; i < 3; ++i) {
            Vector3 tipPos = origin + axes[i] * kGizmoLength;
            float tipW;
            if (project(tipPos, screenTips[i], tipW)) {
                tipsValid[i] = true;
                float d = distToSegment(mousePos, screenOrigin, screenTips[i]);
                if (d < minD) {
                    minD = d;
                    hoveredAxis = i;
                }
            }
        }

        if (!isDraggingGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingGizmo_ = true;
            gizmoActiveAxis_ = hoveredAxis;
            gizmoDragStartMouse_ = mousePos;
            gizmoStartTranslation_ = sel->GetTranslation();
            gizmoDragStartSnapshot_ = context_->CreateSnapshot();
        }

        for (int i = 0; i < 3; ++i) {
            if (!tipsValid[i]) continue;
            bool isAct = (gizmoActiveAxis_ == i) || (!isDraggingGizmo_ && hoveredAxis == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 2.5f : 1.5f;

            ImVec2 dir2D = ImVec2(screenTips[i].x - screenOrigin.x, screenTips[i].y - screenOrigin.y);
            float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
            if (len2D > 1.0f) {
                dir2D.x /= len2D;
                dir2D.y /= len2D;
            } else {
                dir2D = ImVec2(1.0f, 0.0f);
            }
            ImVec2 perp2D = ImVec2(-dir2D.y, dir2D.x);

            float arrowLen = isAct ? 9.0f : 7.0f;
            float arrowWidth = isAct ? 3.5f : 2.5f;

            ImVec2 apex = screenTips[i];
            ImVec2 baseCenter = ImVec2(apex.x - dir2D.x * arrowLen, apex.y - dir2D.y * arrowLen);
            ImVec2 baseL = ImVec2(baseCenter.x + perp2D.x * arrowWidth, baseCenter.y + perp2D.y * arrowWidth);
            ImVec2 baseR = ImVec2(baseCenter.x - perp2D.x * arrowWidth, baseCenter.y - perp2D.y * arrowWidth);

            drawList->AddLine(screenOrigin, baseCenter, col, thick);
            drawList->AddTriangleFilled(apex, baseL, baseR, col);
            drawList->AddTriangle(apex, baseL, baseR, IM_COL32(20, 20, 20, 240), 1.0f);
        }

        // Drag Translation
        if (isDraggingGizmo_ && gizmoActiveAxis_ >= 0 && gizmoActiveAxis_ < 3) {
            int a = gizmoActiveAxis_;
            if (tipsValid[a] && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
                ImVec2 dir2D = ImVec2(screenTips[a].x - screenOrigin.x, screenTips[a].y - screenOrigin.y);
                float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
                if (len2D > 1.0f) {
                    dir2D.x /= len2D;
                    dir2D.y /= len2D;
                    float proj = io.MouseDelta.x * dir2D.x + io.MouseDelta.y * dir2D.y;
                    float factor = clipOrigin.w * 0.002f;
                    float deltaAmount = proj * factor;

                    Vector3 curT = sel->GetTranslation();
                    curT.x += axes[a].x * deltaAmount;
                    curT.y += axes[a].y * deltaAmount;
                    curT.z += axes[a].z * deltaAmount;

                    if (context_->IsSnapEnabled()) {
                        float s = context_->GetTranslateSnap();
                        curT.x = std::round(curT.x / s) * s;
                        curT.y = std::round(curT.y / s) * s;
                        curT.z = std::round(curT.z / s) * s;
                    }

                    sel->SetTranslation(curT);
                    sel->Update();
                }
            }
        }
    }
    // -------------------------------------------------------------
    // Rotation Mode (R)
    // -------------------------------------------------------------
    else if (mode == Model3DEditorContext::GizmoMode::Rotation) {
        const int numSegments = 36;
        int hoveredRing = -1;
        float minD = 7.0f;

        struct RingPoint {
            ImVec2 screenP;
            bool valid;
        };
        std::vector<std::vector<RingPoint>> ringPts(3, std::vector<RingPoint>(numSegments + 1));

        for (int a = 0; a < 3; ++a) {
            Vector3 u = axes[(a + 1) % 3];
            Vector3 v = axes[(a + 2) % 3];
            float worldR = kGizmoLength;

            for (int s = 0; s <= numSegments; ++s) {
                float theta = (float)s / (float)numSegments * 2.0f * kPi;
                Vector3 p = origin + (u * std::cos(theta) + v * std::sin(theta)) * worldR;
                float w;
                ringPts[a][s].valid = project(p, ringPts[a][s].screenP, w);
            }

            for (int s = 0; s < numSegments; ++s) {
                if (ringPts[a][s].valid && ringPts[a][s + 1].valid) {
                    float d = distToSegment(mousePos, ringPts[a][s].screenP, ringPts[a][s + 1].screenP);
                    if (d < minD) {
                        minD = d;
                        hoveredRing = a;
                    }
                }
            }
        }

        if (!isDraggingGizmo_ && hoveredRing >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingGizmo_ = true;
            gizmoActiveAxis_ = hoveredRing;
            gizmoDragStartMouse_ = mousePos;
            gizmoStartRotation_ = sel->GetRotation();
            gizmoDragStartSnapshot_ = context_->CreateSnapshot();
        }

        for (int a = 0; a < 3; ++a) {
            bool isAct = (gizmoActiveAxis_ == a) || (!isDraggingGizmo_ && hoveredRing == a);
            ImU32 col = isAct ? axisHoverColors[a] : axisColors[a];
            float thick = isAct ? 2.5f : 1.5f;

            for (int s = 0; s < numSegments; ++s) {
                if (ringPts[a][s].valid && ringPts[a][s + 1].valid) {
                    drawList->AddLine(ringPts[a][s].screenP, ringPts[a][s + 1].screenP, col, thick);
                }
            }
        }

        // Drag Rotation
        if (isDraggingGizmo_ && gizmoActiveAxis_ >= 0 && gizmoActiveAxis_ < 3) {
            int a = gizmoActiveAxis_;
            float deltaAngle = (io.MouseDelta.x - io.MouseDelta.y) * 0.015f;

            Vector3 curRot = sel->GetRotation();
            if (a == 0) curRot.x += deltaAngle;
            else if (a == 1) curRot.y += deltaAngle;
            else if (a == 2) curRot.z += deltaAngle;

            sel->SetRotation(curRot);
            sel->Update();
        }
    }
    // -------------------------------------------------------------
    // Scale Mode (S)
    // -------------------------------------------------------------
    else if (mode == Model3DEditorContext::GizmoMode::Scale) {
        ImVec2 screenTips[3];
        bool tipsValid[3] = { false, false, false };
        int hoveredAxis = -1;
        float minD = 10.0f;

        // Center box for uniform scale
        bool centerHovered = (mousePos.x - screenOrigin.x) * (mousePos.x - screenOrigin.x) + (mousePos.y - screenOrigin.y) * (mousePos.y - screenOrigin.y) < 8.0f * 8.0f;
        if (centerHovered) {
            hoveredAxis = 3; // Center uniform scale
        }

        for (int i = 0; i < 3; ++i) {
            Vector3 tipPos = origin + axes[i] * kGizmoLength;
            float tipW;
            if (project(tipPos, screenTips[i], tipW)) {
                tipsValid[i] = true;
                if (!centerHovered) {
                    float d = distToSegment(mousePos, screenOrigin, screenTips[i]);
                    if (d < minD) {
                        minD = d;
                        hoveredAxis = i;
                    }
                }
            }
        }

        if (!isDraggingGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingGizmo_ = true;
            gizmoActiveAxis_ = hoveredAxis;
            gizmoDragStartMouse_ = mousePos;
            gizmoStartScale_ = sel->GetScale();
            gizmoDragStartSnapshot_ = context_->CreateSnapshot();
        }

        // Draw center uniform box
        ImU32 centerCol = (gizmoActiveAxis_ == 3 || (!isDraggingGizmo_ && hoveredAxis == 3)) ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 180);
        drawList->AddRectFilled(ImVec2(screenOrigin.x - 3.5f, screenOrigin.y - 3.5f), ImVec2(screenOrigin.x + 3.5f, screenOrigin.y + 3.5f), centerCol);
        drawList->AddRect(ImVec2(screenOrigin.x - 3.5f, screenOrigin.y - 3.5f), ImVec2(screenOrigin.x + 3.5f, screenOrigin.y + 3.5f), IM_COL32(40, 40, 40, 255), 0, 0, 1.0f);

        for (int i = 0; i < 3; ++i) {
            if (!tipsValid[i]) continue;
            bool isAct = (gizmoActiveAxis_ == i) || (!isDraggingGizmo_ && hoveredAxis == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 2.5f : 1.5f;

            drawList->AddLine(screenOrigin, screenTips[i], col, thick);
            drawList->AddRectFilled(ImVec2(screenTips[i].x - 3.0f, screenTips[i].y - 3.0f), ImVec2(screenTips[i].x + 3.0f, screenTips[i].y + 3.0f), col);
            drawList->AddRect(ImVec2(screenTips[i].x - 3.0f, screenTips[i].y - 3.0f), ImVec2(screenTips[i].x + 3.0f, screenTips[i].y + 3.0f), IM_COL32(20, 20, 20, 240), 0, 0, 1.0f);
        }

        // Drag Scale
        if (isDraggingGizmo_) {
            if (gizmoActiveAxis_ == 3) {
                // Uniform scale
                float delta = (io.MouseDelta.x - io.MouseDelta.y) * 0.02f;
                Vector3 curS = sel->GetScale();
                curS.x = (std::max)(0.001f, curS.x + delta);
                curS.y = (std::max)(0.001f, curS.y + delta);
                curS.z = (std::max)(0.001f, curS.z + delta);
                sel->SetScale(curS);
                sel->Update();
            } else if (gizmoActiveAxis_ >= 0 && gizmoActiveAxis_ < 3) {
                int a = gizmoActiveAxis_;
                if (tipsValid[a]) {
                    ImVec2 dir2D = ImVec2(screenTips[a].x - screenOrigin.x, screenTips[a].y - screenOrigin.y);
                    float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
                    if (len2D > 1.0f) {
                        dir2D.x /= len2D;
                        dir2D.y /= len2D;
                        float proj = io.MouseDelta.x * dir2D.x + io.MouseDelta.y * dir2D.y;
                        float deltaAmount = proj * 0.02f;

                        Vector3 curS = sel->GetScale();
                        if (a == 0) curS.x = (std::max)(0.001f, curS.x + deltaAmount);
                        else if (a == 1) curS.y = (std::max)(0.001f, curS.y + deltaAmount);
                        else if (a == 2) curS.z = (std::max)(0.001f, curS.z + deltaAmount);

                        sel->SetScale(curS);
                        sel->Update();
                    }
                }
            }
        }
    }

    // Drag End: push snapshot to undo stack
    if (isDraggingGizmo_) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (sel) {
                Vector3 curT = sel->GetTranslation();
                Vector3 curR = sel->GetRotation();
                Vector3 curS = sel->GetScale();
                bool changed = (curT.x != gizmoStartTranslation_.x || curT.y != gizmoStartTranslation_.y || curT.z != gizmoStartTranslation_.z ||
                                curR.x != gizmoStartRotation_.x || curR.y != gizmoStartRotation_.y || curR.z != gizmoStartRotation_.z ||
                                curS.x != gizmoStartScale_.x || curS.y != gizmoStartScale_.y || curS.z != gizmoStartScale_.z);
                if (changed) {
                    context_->PushSnapshotToUndo(gizmoDragStartSnapshot_);
                }
            }
            isDraggingGizmo_ = false;
            gizmoActiveAxis_ = -1;
        }
    }

    drawList->PopClipRect();
}
#endif
