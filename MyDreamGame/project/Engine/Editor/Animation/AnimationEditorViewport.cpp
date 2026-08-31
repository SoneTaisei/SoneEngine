#ifdef USE_IMGUI
#include "AnimationEditorViewport.h"
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

AnimationEditorViewport::AnimationEditorViewport() {
    Initialize();
}
void AnimationEditorViewport::Initialize() {
}

void AnimationEditorViewport::DrawViewportGrid(const Matrix4x4& viewProjectionMatrix, ImVec2 vpPos, ImVec2 vpSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    const float gridExtent = 12.0f;
    const float gridStep = 1.0f;
    const float nearW = 0.05f; // Nearクリップ閾値
    const float gridY = 0.005f; // 床オブジェクトとの干渉を避ける高さ

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

    // 1. 通常グリッド線 (XZ平面) - 1.0mセグメント分割で安定描画
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

    // 2. 0のライン強調 (X軸: 赤, Z軸: 青/緑) - 1.0mセグメント分割で安定描画
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

void AnimationEditorViewport::DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!activeCamera) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(vpPos.x + vpSize.x - 55.0f, vpPos.y + 55.0f);
    const float radius = 36.0f;
    const float badgeRadius = 9.5f;

    // 背景の円形プレート（半透明）
    drawList->AddCircleFilled(center, radius + 12.0f, IM_COL32(30, 30, 35, 130), 32);

    Matrix4x4 viewMat = activeCamera->GetViewMatrix();

    // 6本の軸定義 (ワールド方向, 正/負, 色, ラベル, スナップ時の目標回転, 目標位置オフセット方向)
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

    const float PI_VAL = 3.1415926535f;
    const float HALF_PI = 1.5707963268f;

    AxisInfo axes[6] = {
        // +X: 右側面 (Right) - カメラは+X側に位置し、-X方向を向く -> Y回転: -90°
        { {  1.0f,  0.0f,  0.0f }, true,  IM_COL32(235, 65,  75,  255), IM_COL32(235, 65,  75,  180), 'X', { 0.0f, -HALF_PI, 0.0f }, {  1.0f, 0.0f, 0.0f }, {}, {}, false },
        // -X: 左側面 (Left) - カメラは-X側に位置し、+X方向を向く -> Y回転: +90°
        { { -1.0f,  0.0f,  0.0f }, false, IM_COL32(235, 65,  75,  140), IM_COL32(235, 65,  75,  200), ' ', { 0.0f,  HALF_PI, 0.0f }, { -1.0f, 0.0f, 0.0f }, {}, {}, false },
        // +Y: 上面 (Top) - カメラは+Y側に位置し、真下を向く -> X回転: +90°
        { {  0.0f,  1.0f,  0.0f }, true,  IM_COL32(130, 200, 45,  255), IM_COL32(130, 200, 45,  180), 'Y', { HALF_PI - 0.001f, 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0001f }, {}, {}, false },
        // -Y: 底面 (Bottom) - カメラは-Y側に位置し、真上を向く -> X回転: -90°
        { {  0.0f, -1.0f,  0.0f }, false, IM_COL32(130, 200, 45,  140), IM_COL32(130, 200, 45,  200), ' ', { -HALF_PI + 0.001f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0001f }, {}, {}, false },
        // +Z: 正面 (Front) - カメラは-Z側に位置し、+Z方向(手前)を向く -> Y回転: 0°
        { {  0.0f,  0.0f,  1.0f }, true,  IM_COL32(50,  135, 245, 255), IM_COL32(50,  135, 245, 180), 'Z', { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, {}, {}, false },
        // -Z: 背面 (Back) - カメラは+Z側に位置し、-Z方向(奥)を向く -> Y回転: 180°
        { {  0.0f,  0.0f, -1.0f }, false, IM_COL32(50,  135, 245, 140), IM_COL32(50,  135, 245, 200), ' ', { 0.0f, PI_VAL, 0.0f }, { 0.0f, 0.0f,  1.0f }, {}, {}, false }
    };

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    // カメラ座標系への変換 (ViewMatrixの回転成分を適用)
    for (int i = 0; i < 6; ++i) {
        axes[i].camDir.x = axes[i].worldDir.x * viewMat.m[0][0] + axes[i].worldDir.y * viewMat.m[1][0] + axes[i].worldDir.z * viewMat.m[2][0];
        axes[i].camDir.y = axes[i].worldDir.x * viewMat.m[0][1] + axes[i].worldDir.y * viewMat.m[1][1] + axes[i].worldDir.z * viewMat.m[2][1];
        axes[i].camDir.z = axes[i].worldDir.x * viewMat.m[0][2] + axes[i].worldDir.y * viewMat.m[1][2] + axes[i].worldDir.z * viewMat.m[2][2];

        axes[i].screenPos = ImVec2(
            center.x + axes[i].camDir.x * radius,
            center.y - axes[i].camDir.y * radius
        );
    }

    // 深度(camDir.z)の昇順(奥 -> 手前)にソート
    std::vector<int> sortedIndices = { 0, 1, 2, 3, 4, 5 };
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
        return axes[a].camDir.z < axes[b].camDir.z;
    });

    // ホバー＆クリック判定（手前側にある軸から優先して判定）
    int clickedAxisIdx = -1;
    for (int i = 5; i >= 0; --i) {
        int idx = sortedIndices[i];
        float dx = mousePos.x - axes[idx].screenPos.x;
        float dy = mousePos.y - axes[idx].screenPos.y;
        float distSq = dx * dx + dy * dy;
        float hitRadius = badgeRadius + 2.5f;
        if (distSq <= hitRadius * hitRadius) {
            axes[idx].isHovered = true;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                clickedAxisIdx = idx;
            }
            break;
        }
    }

    // 軸スナップの開始 (クリック時 - 線形補間アニメーション)
    if (clickedAxisIdx >= 0) {
        // 現在すでにクリックした軸の正面を向いている場合、反対側の軸（+X<->-X, +Y<->-Y, +Z<->-Z）にトグル反転
        if (axes[clickedAxisIdx].camDir.z > 0.70f) {
            int oppIdx = (clickedAxisIdx % 2 == 0) ? (clickedAxisIdx + 1) : (clickedAxisIdx - 1);
            clickedAxisIdx = oppIdx;
        }

        const auto& snapAxis = axes[clickedAxisIdx];
        
        // 注視点 (モデル中心: 0, 1.0, 0)
        Vector3 target = { 0.0f, 1.0f, 0.0f };
        Vector3 curCamPos = activeCamera->GetTranslation();
        Vector3 diff = { curCamPos.x - target.x, curCamPos.y - target.y, curCamPos.z - target.z };
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (dist < 2.0f || dist > 30.0f) dist = 6.0f; // 適切な距離にフォールバック

        cameraSnapStartRot_ = activeCamera->GetRotation();
        cameraSnapStartPos_ = curCamPos;

        // 角度の最短経路補間（-PI〜+PIの差分調整）
        auto normalizeAngleDiff = [](float start, float target) {
            float diff = target - start;
            while (diff > 3.14159265f) { start += 6.2831853f; diff = target - start; }
            while (diff < -3.14159265f) { start -= 6.2831853f; diff = target - start; }
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

    // 描画: 奥から手前へ
    for (int idx : sortedIndices) {
        const auto& axis = axes[idx];
        float curRadius = axis.isHovered ? (badgeRadius + 1.5f) : badgeRadius;

        if (!axis.isPositive) {
            // 負方向軸: 半透明のリング（円周のみ）
            if (axis.isHovered) {
                drawList->AddCircleFilled(axis.screenPos, curRadius * 0.75f, axis.color, 16);
                drawList->AddCircle(axis.screenPos, curRadius * 0.75f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            } else {
                drawList->AddCircle(axis.screenPos, curRadius * 0.75f, axis.ringColor, 16, 1.5f);
            }
        } else {
            // 正方向軸: 中心から軸への線分
            drawList->AddLine(center, axis.screenPos, axis.color, 2.5f);

            // 塗りつぶしバッジ円
            drawList->AddCircleFilled(axis.screenPos, curRadius, axis.color, 16);
            if (axis.isHovered) {
                drawList->AddCircle(axis.screenPos, curRadius, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            } else {
                drawList->AddCircle(axis.screenPos, curRadius, IM_COL32(255, 255, 255, 120), 16, 1.0f);
            }

            // 文字 ('X', 'Y', 'Z')
            char txt[2] = { axis.label, '\0' };
            ImVec2 txtSz = ImGui::CalcTextSize(txt);
            drawList->AddText(ImVec2(axis.screenPos.x - txtSz.x * 0.5f, axis.screenPos.y - txtSz.y * 0.5f), IM_COL32(255, 255, 255, 255), txt);
        }
    }
}


void AnimationEditorViewport::DrawSkeletonJointsOverlay(SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize, AnimationEditorContext* context) {
    if (!sceneManager || !activeCamera) return;

    AnimatorComponent* animator = context->GetTargetAnimator(sceneManager);
    Matrix4x4 worldMatrix = TransformFunctions::MakeIdentity4x4();

    if (context->GetSelectedGameObject()) {
        auto* tr = context->GetSelectedGameObject()->GetComponent<TransformComponent>();
        if (tr) worldMatrix = tr->GetWorldMatrix();
    } else if (context->GetSelectedObject()) {
        worldMatrix = TransformFunctions::MakeAffineMatrix(
            context->GetSelectedObject()->GetScale(),
            context->GetSelectedObject()->GetRotation(),
            context->GetSelectedObject()->GetTranslation()
        );
    } else if (sceneManager && sceneManager->GetCurrentScene()) {
        auto* scene = sceneManager->GetCurrentScene();
        for (auto& go : scene->GetGameObjects()) {
            if (go && go->GetComponent<AnimatorComponent>() == animator) {
                auto* tr = go->GetComponent<TransformComponent>();
                if (tr) worldMatrix = tr->GetWorldMatrix();
                break;
            }
        }
    }

    if (!animator || !animator->HasSkeleton()) return;

    const Skeleton& skeleton = animator->GetSkeleton();
    if (skeleton.joints.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    Matrix4x4 vpMat = TransformFunctions::Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

    // 各ジョイントのスクリーン座標を事前計算
    struct JointScreenInfo {
        Vector3 worldPos;
        ImVec2 screenPos;
        bool isVisible;
        float depth;
    };
    std::vector<JointScreenInfo> screenJoints(skeleton.joints.size());

    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        const auto& joint = skeleton.joints[i];
        // 親階層の回転・平行移動を含む完全なワールド変換行列
        Matrix4x4 jointWorld = TransformFunctions::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
        Vector3 worldPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

        screenJoints[i].worldPos = worldPos;

        Vector4 clip;
        clip.x = worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0];
        clip.y = worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1];
        clip.z = worldPos.x * vpMat.m[0][2] + worldPos.y * vpMat.m[1][2] + worldPos.z * vpMat.m[2][2] + vpMat.m[3][2];
        clip.w = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];

        if (clip.w > 0.05f) {
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            screenJoints[i].screenPos = ImVec2(
                vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x,
                vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y
            );
            screenJoints[i].isVisible = true;
            screenJoints[i].depth = clip.w;
        } else {
            screenJoints[i].isVisible = false;
        }
    }

    // 1. 親子間のボーン接続線を描画
    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        const auto& joint = skeleton.joints[i];
        if (joint.parent.has_value()) {
            int32_t pIdx = joint.parent.value();
            if (pIdx >= 0 && pIdx < static_cast<int32_t>(screenJoints.size())) {
                if (screenJoints[i].isVisible && screenJoints[pIdx].isVisible) {
                    bool isConnectedToSelected = (joint.name == context->GetSelectedJointName() || skeleton.joints[pIdx].name == context->GetSelectedJointName());
                    ImU32 boneCol = isConnectedToSelected ? IM_COL32(255, 220, 80, 230) : IM_COL32(140, 210, 255, 150);
                    float thickness = isConnectedToSelected ? 3.0f : 1.8f;
                    drawList->AddLine(screenJoints[pIdx].screenPos, screenJoints[i].screenPos, boneCol, thickness);
                }
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    int hoveredJointIdx = -1;
    float closestDistSq = 20.0f * 20.0f; // クリック判定半径

    // 2. 非選択ジョイントの丸を描画 & ホバー判定
    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        if (!screenJoints[i].isVisible) continue;
        const auto& joint = skeleton.joints[i];
        bool isSelected = (joint.name == context->GetSelectedJointName());

        ImVec2 p = screenJoints[i].screenPos;
        float dx = mousePos.x - p.x;
        float dy = mousePos.y - p.y;
        float dSq = dx * dx + dy * dy;

        if (dSq < closestDistSq) {
            closestDistSq = dSq;
            hoveredJointIdx = static_cast<int>(i);
        }

        if (!isSelected) {
            // パキッとした水色サークル＋白い核＋濃い縁取り
            drawList->AddCircleFilled(p, 5.0f, IM_COL32(60, 170, 255, 230), 16);
            drawList->AddCircleFilled(p, 2.0f, IM_COL32(255, 255, 255, 255), 10);
            drawList->AddCircle(p, 5.0f, IM_COL32(20, 50, 100, 240), 16, 1.2f);
        }
    }

    // ホバー時のハイライトとクリック選択（ロック中でない場合のみ他ボーンを選択可能）
    if (hoveredJointIdx >= 0 && !isDraggingAnimGizmo_ && !context->GetIsAnimLocked()) {
        const auto& hJoint = skeleton.joints[hoveredJointIdx];
        if (hJoint.name != context->GetSelectedJointName()) {
            ImVec2 hp = screenJoints[hoveredJointIdx].screenPos;
            drawList->AddCircle(hp, 9.0f, IM_COL32(255, 255, 255, 230), 16, 2.0f);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && animGizmoActiveAxis_ < 0) {
            context->GetSelectedJointName() = hJoint.name;
            context->GetSelectedKeyIndex() = -1;
        }
    }

    // 3. 選択中のジョイントを最前面に強調描画
    auto it = skeleton.jointMap.find(context->GetSelectedJointName());
    if (it == skeleton.jointMap.end() && !skeleton.joints.empty()) {
        // 見つからない場合は先頭のボーンにフォールバック
        context->GetSelectedJointName() = skeleton.joints[0].name;
        it = skeleton.jointMap.find(context->GetSelectedJointName());
    }

    if (it != skeleton.jointMap.end() && it->second < screenJoints.size()) {
        int selIdx = it->second;
        if (screenJoints[selIdx].isVisible) {
            ImVec2 sp = screenJoints[selIdx].screenPos;

            // パルスアニメーション効果
            static float pulseTimer = 0.0f;
            pulseTimer += (io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f) * 5.0f;
            float pulseOffset = std::sin(pulseTimer) * 2.0f;

            // 外側の強調リング
            drawList->AddCircle(sp, 12.0f + pulseOffset, IM_COL32(255, 255, 255, 240), 24, 2.5f);
            drawList->AddCircle(sp, 15.5f + pulseOffset, IM_COL32(255, 200, 30, 160), 24, 1.2f);

            // 中心の黄色い丸
            drawList->AddCircleFilled(sp, 8.0f, IM_COL32(255, 200, 30, 255), 20);
            drawList->AddCircleFilled(sp, 3.5f, IM_COL32(255, 255, 255, 255), 12);
            drawList->AddCircle(sp, 8.0f, IM_COL32(180, 110, 0, 255), 20, 1.5f);

            // ボーン名のテキストラベル（右上に黒半透明背景付き）
            std::string label = context->GetSelectedJointName() + (context->GetIsAnimLocked() ? " [Locked]" : "");
            ImVec2 txtSz = ImGui::CalcTextSize(label.c_str());
            ImVec2 boxMin = ImVec2(sp.x + 12.0f, sp.y - txtSz.y * 0.5f - 4.0f);
            ImVec2 boxMax = ImVec2(boxMin.x + txtSz.x + 10.0f, boxMin.y + txtSz.y + 8.0f);

            drawList->AddRectFilled(boxMin, boxMax, IM_COL32(15, 15, 20, 230), 4.0f);
            drawList->AddRect(boxMin, boxMax, context->GetIsAnimLocked() ? IM_COL32(255, 90, 90, 220) : IM_COL32(255, 205, 40, 220), 4.0f, 0, 1.5f);
            drawList->AddText(ImVec2(boxMin.x + 5.0f, boxMin.y + 4.0f), context->GetIsAnimLocked() ? IM_COL32(255, 140, 140, 255) : IM_COL32(255, 240, 120, 255), label.c_str());
        }
    }

    drawList->PopClipRect();
}


void AnimationEditorViewport::DrawBoneTransformGizmo(SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize, AnimationEditorContext* context) {
    if (!sceneManager || !activeCamera) return;

    AnimatorComponent* animator = context->GetTargetAnimator(sceneManager);
    if (!animator || !animator->HasSkeleton()) return;

    const Skeleton& skeleton = animator->GetSkeleton();
    auto it = skeleton.jointMap.find(context->GetSelectedJointName());
    if (it == skeleton.jointMap.end()) return;

    int32_t jointIdx = it->second;
    if (jointIdx < 0 || jointIdx >= static_cast<int32_t>(skeleton.joints.size())) return;

    const Joint& joint = skeleton.joints[jointIdx];

    Matrix4x4 worldMatrix = TransformFunctions::MakeIdentity4x4();
    if (context->GetSelectedGameObject()) {
        auto* tr = context->GetSelectedGameObject()->GetComponent<TransformComponent>();
        if (tr) worldMatrix = tr->GetWorldMatrix();
    } else if (context->GetSelectedObject()) {
        worldMatrix = TransformFunctions::MakeAffineMatrix(
            context->GetSelectedObject()->GetScale(),
            context->GetSelectedObject()->GetRotation(),
            context->GetSelectedObject()->GetTranslation()
        );
    } else if (sceneManager && sceneManager->GetCurrentScene()) {
        for (auto& go : sceneManager->GetCurrentScene()->GetGameObjects()) {
            if (go && go->GetComponent<AnimatorComponent>() == animator) {
                auto* tr = go->GetComponent<TransformComponent>();
                if (tr) worldMatrix = tr->GetWorldMatrix();
                break;
            }
        }
    }

    Matrix4x4 jointWorld = TransformFunctions::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
    Vector3 origin = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

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

    float gizmoRadius = clipOrigin.w * 0.18f;

    Vector3 axes[3] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };

    if (context->GetGizmoSpace() == 0 && context->GetGizmoMode() != 1) { // Local space (Rotation Mode 1 は常にワールド固定軸で表示)
        Vector3 lx = { jointWorld.m[0][0], jointWorld.m[0][1], jointWorld.m[0][2] };
        Vector3 ly = { jointWorld.m[1][0], jointWorld.m[1][1], jointWorld.m[1][2] };
        Vector3 lz = { jointWorld.m[2][0], jointWorld.m[2][1], jointWorld.m[2][2] };
        float lenX = std::sqrt(lx.x * lx.x + lx.y * lx.y + lx.z * lx.z); if (lenX > 1e-5f) axes[0] = lx * (1.0f / lenX);
        float lenY = std::sqrt(ly.x * ly.x + ly.y * ly.y + ly.z * ly.z); if (lenY > 1e-5f) axes[1] = ly * (1.0f / lenY);
        float lenZ = std::sqrt(lz.x * lz.x + lz.y * lz.y + lz.z * lz.z); if (lenZ > 1e-5f) axes[2] = lz * (1.0f / lenZ);
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

    NodeAnimation& nodeAnim = context->GetEditingAnimation().nodeAnimations[context->GetSelectedJointName()];

    bool hasAnySymmetryAxis = context->GetAnimSymmetryAxisX() || context->GetAnimSymmetryAxisY() || context->GetAnimSymmetryAxisZ();
    std::string gizmoOppJoint = (context->GetAnimSymmetryMode() && hasAnySymmetryAxis) ? context->FindOppositeJointName(context->GetSelectedJointName(), context->GetAnimSymmetryAxisX(), context->GetAnimSymmetryAxisY(), context->GetAnimSymmetryAxisZ(), &skeleton) : "";
    auto syncGizmoOppositeSRT = [&](const Vector3* newT, const Quaternion* newR, const Vector3* newS) {
        if (!context->GetAnimSymmetryMode() || !hasAnySymmetryAxis || gizmoOppJoint.empty() || gizmoOppJoint == context->GetSelectedJointName()) return;

        Vector3 curS = joint.defaultTransform.scale;
        if (!nodeAnim.scale.empty()) curS = CalculateValue(nodeAnim.scale, context->GetAnimEditorTime());
        auto itS = context->GetTempOverrides().find(context->GetSelectedJointName());
        if (itS != context->GetTempOverrides().end() && itS->second.scale) curS = *itS->second.scale;

        Quaternion curR = joint.defaultTransform.rotate;
        if (!nodeAnim.rotate.empty()) curR = CalculateValue(nodeAnim.rotate, context->GetAnimEditorTime());
        auto itR = context->GetTempOverrides().find(context->GetSelectedJointName());
        if (itR != context->GetTempOverrides().end() && itR->second.rotate) curR = *itR->second.rotate;

        Vector3 curT = joint.defaultTransform.translate;
        if (!nodeAnim.translate.empty()) curT = CalculateValue(nodeAnim.translate, context->GetAnimEditorTime());
        auto itT = context->GetTempOverrides().find(context->GetSelectedJointName());
        if (itT != context->GetTempOverrides().end() && itT->second.translate) curT = *itT->second.translate;

        if (newT) curT = *newT;
        if (newR) curR = *newR;
        if (newS) curS = *newS;

        Vector3 oppS, oppT;
        Quaternion oppQ;
        if (!ComputeBlenderSymmetrySRT(skeleton, context->GetSelectedJointName(), gizmoOppJoint, curS, curR, curT, context->GetAnimSymmetryAxisX(), context->GetAnimSymmetryAxisY(), context->GetAnimSymmetryAxisZ(), oppS, oppQ, oppT)) {
            return;
        }

        NodeAnimation& oppNode = context->GetEditingAnimation().nodeAnimations[gizmoOppJoint];

        if (newT) {
            bool found = false;
            for (size_t idx = 0; idx < oppNode.translate.size(); ++idx) {
                if (std::abs(oppNode.translate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    oppNode.translate[idx].value = oppT;
                    found = true;
                    break;
                }
            }
            if (!found) context->GetTempOverrides()[gizmoOppJoint].translate = oppT;
        }
        if (newR) {
            bool found = false;
            for (size_t idx = 0; idx < oppNode.rotate.size(); ++idx) {
                if (std::abs(oppNode.rotate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    oppNode.rotate[idx].value = oppQ;
                    found = true;
                    break;
                }
            }
            if (!found) context->GetTempOverrides()[gizmoOppJoint].rotate = oppQ;
        }
        if (newS) {
            bool found = false;
            for (size_t idx = 0; idx < oppNode.scale.size(); ++idx) {
                if (std::abs(oppNode.scale[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    oppNode.scale[idx].value = oppS;
                    found = true;
                    break;
                }
            }
            if (!found) context->GetTempOverrides()[gizmoOppJoint].scale = oppS;
        }
    };

    if (!isDraggingAnimGizmo_) {
        animGizmoActiveAxis_ = -1;
    }

    // --------------------------------------------------------
    // Mode 0: Translation (移動)
    // --------------------------------------------------------
    if (context->GetGizmoMode() == 0) {
        ImVec2 screenTips[3];
        bool tipsValid[3] = { false, false, false };
        int hoveredAxis = -1;
        float minD = 12.0f;

        for (int i = 0; i < 3; ++i) {
            Vector3 tipPos = origin + axes[i] * gizmoRadius;
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

        if (!isDraggingAnimGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingAnimGizmo_ = true;
            animGizmoActiveAxis_ = hoveredAxis;
            animGizmoDragStartMouse_ = mousePos;
            context->BeginDragSnapshot("ギズモ移動");
        }

        // Draw axis lines and arrow tips (移動用矢印)
        for (int i = 0; i < 3; ++i) {
            if (!tipsValid[i]) continue;
            bool isAct = (animGizmoActiveAxis_ == i) || (!isDraggingAnimGizmo_ && hoveredAxis == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 3.5f : 2.0f;

            ImVec2 dir2D = ImVec2(screenTips[i].x - screenOrigin.x, screenTips[i].y - screenOrigin.y);
            float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
            if (len2D > 1.0f) {
                dir2D.x /= len2D;
                dir2D.y /= len2D;
            } else {
                dir2D = ImVec2(1.0f, 0.0f);
            }
            ImVec2 perp2D = ImVec2(-dir2D.y, dir2D.x);

            float arrowLen = isAct ? 15.0f : 12.0f;
            float arrowWidth = isAct ? 6.5f : 5.0f;

            ImVec2 apex = screenTips[i];
            ImVec2 baseCenter = ImVec2(apex.x - dir2D.x * arrowLen, apex.y - dir2D.y * arrowLen);
            ImVec2 baseL = ImVec2(baseCenter.x + perp2D.x * arrowWidth, baseCenter.y + perp2D.y * arrowWidth);
            ImVec2 baseR = ImVec2(baseCenter.x - perp2D.x * arrowWidth, baseCenter.y - perp2D.y * arrowWidth);

            // 軸ラインの描画 (矢印の付け根まで)
            drawList->AddLine(screenOrigin, baseCenter, col, thick);

            // 矢印ヘッド（三角形）の描画
            drawList->AddTriangleFilled(apex, baseL, baseR, col);
            drawList->AddTriangle(apex, baseL, baseR, IM_COL32(20, 20, 20, 240), 1.2f);
        }

        // Handle Dragging Translation
        if (isDraggingAnimGizmo_ && animGizmoActiveAxis_ >= 0 && animGizmoActiveAxis_ < 3) {
            int a = animGizmoActiveAxis_;
            if (tipsValid[a] && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
                ImVec2 dir2D = ImVec2(screenTips[a].x - screenOrigin.x, screenTips[a].y - screenOrigin.y);
                float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
                if (len2D > 1.0f) {
                    dir2D.x /= len2D;
                    dir2D.y /= len2D;
                    float proj = io.MouseDelta.x * dir2D.x + io.MouseDelta.y * dir2D.y;
                    float factor = gizmoRadius / (std::max)(len2D, 60.0f);
                    float deltaAmount = proj * factor * 1.2f;

                    Vector3 curT = joint.defaultTransform.translate;
                    if (!nodeAnim.translate.empty()) {
                        curT = CalculateValue(nodeAnim.translate, context->GetAnimEditorTime());
                    }
                    auto itTempT = context->GetTempOverrides().find(context->GetSelectedJointName());
                    if (itTempT != context->GetTempOverrides().end() && itTempT->second.translate) {
                        curT = *itTempT->second.translate;
                    }

                    Vector3 newT = curT;
                    if (a == 0) newT.x += deltaAmount;
                    else if (a == 1) newT.y += deltaAmount;
                    else if (a == 2) newT.z += deltaAmount;

                    bool found = false;
                    for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
                        if (std::abs(nodeAnim.translate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                            nodeAnim.translate[idx].value = newT;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        context->GetTempOverrides()[context->GetSelectedJointName()].translate = newT;
                    }
                    syncGizmoOppositeSRT(&newT, nullptr, nullptr);
                    context->UpdateAnimationPosePreview(sceneManager);
                }
            }
        }
    }
    // --------------------------------------------------------
    // Mode 1: Rotation (回転)
    // --------------------------------------------------------
    else if (context->GetGizmoMode() == 1) {
        const int numSegments = 32;
        int hoveredRing = -1;
        float minD = 8.0f;

        struct RingData {
            std::vector<ImVec2> pts;
            bool valid = false;
        };
        RingData rings[3];

        for (int i = 0; i < 3; ++i) {
            Vector3 uAxis = axes[(i + 1) % 3];
            Vector3 vAxis = axes[(i + 2) % 3];
            float ringR = gizmoRadius * 0.85f;

            rings[i].pts.reserve(numSegments + 1);
            for (int s = 0; s <= numSegments; ++s) {
                float theta = (static_cast<float>(s) / numSegments) * 6.2831853f;
                Vector3 p = origin + (uAxis * std::cos(theta) + vAxis * std::sin(theta)) * ringR;
                ImVec2 sp;
                float spW;
                if (project(p, sp, spW)) {
                    rings[i].pts.push_back(sp);
                }
            }
            if (rings[i].pts.size() >= numSegments) {
                rings[i].valid = true;
                for (size_t s = 0; s + 1 < rings[i].pts.size(); ++s) {
                    float d = distToSegment(mousePos, rings[i].pts[s], rings[i].pts[s + 1]);
                    if (d < minD) {
                        minD = d;
                        hoveredRing = i;
                    }
                }
            }
        }

        if (!isDraggingAnimGizmo_ && hoveredRing >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingAnimGizmo_ = true;
            animGizmoActiveAxis_ = hoveredRing;
            animGizmoDragStartMouse_ = mousePos;
            context->BeginDragSnapshot("ギズモ回転");
        }

        // Draw rings
        for (int i = 0; i < 3; ++i) {
            if (!rings[i].valid) continue;
            bool isAct = (animGizmoActiveAxis_ == i) || (!isDraggingAnimGizmo_ && hoveredRing == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 3.5f : 2.0f;

            for (size_t s = 0; s + 1 < rings[i].pts.size(); ++s) {
                drawList->AddLine(rings[i].pts[s], rings[i].pts[s + 1], col, thick);
            }
        }

        // Handle Dragging Rotation
        if (isDraggingAnimGizmo_ && animGizmoActiveAxis_ >= 0 && animGizmoActiveAxis_ < 3) {
            int a = animGizmoActiveAxis_;
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                float prevAng = std::atan2((mousePos.y - io.MouseDelta.y) - screenOrigin.y, (mousePos.x - io.MouseDelta.x) - screenOrigin.x);
                float curAng = std::atan2(mousePos.y - screenOrigin.y, mousePos.x - screenOrigin.x);
                float deltaAngle = curAng - prevAng;
                while (deltaAngle > 3.14159f) deltaAngle -= 6.28318f;
                while (deltaAngle < -3.14159f) deltaAngle += 6.28318f;

                Vector3 toCam = activeCamera->GetTranslation() - origin;
                float lenCam = std::sqrt(toCam.x * toCam.x + toCam.y * toCam.y + toCam.z * toCam.z);
                if (lenCam > 1e-5f) toCam = toCam / lenCam;
                float facing = axes[a].x * toCam.x + axes[a].y * toCam.y + axes[a].z * toCam.z;
                if (facing < 0.0f) deltaAngle = -deltaAngle;

                Quaternion curQ = joint.defaultTransform.rotate;
                if (!nodeAnim.rotate.empty()) {
                    curQ = CalculateValue(nodeAnim.rotate, context->GetAnimEditorTime());
                }
                auto itTempR = context->GetTempOverrides().find(context->GetSelectedJointName());
                if (itTempR != context->GetTempOverrides().end() && itTempR->second.rotate) {
                    curQ = *itTempR->second.rotate;
                }

                // ワールド固定軸 axes[a] を親の空間に逆変換してローカル回転軸を得る
                Vector3 worldRotAxis = axes[a];
                Matrix4x4 mParent = TransformFunctions::MakeIdentity4x4();
                if (joint.parent.has_value() && joint.parent.value() >= 0 && joint.parent.value() < static_cast<int32_t>(skeleton.joints.size())) {
                    mParent = skeleton.joints[joint.parent.value()].skeletonSpaceMatrix;
                }
                Matrix4x4 invParent = TransformFunctions::Inverse(mParent);
                Vector3 localRotAxis = invParent * worldRotAxis;
                float lenAx = std::sqrt(localRotAxis.x * localRotAxis.x + localRotAxis.y * localRotAxis.y + localRotAxis.z * localRotAxis.z);
                if (lenAx > 1e-5f) localRotAxis = localRotAxis * (1.0f / lenAx);

                Quaternion qRotDelta = MakeRotHelper(localRotAxis, deltaAngle * 1.5f);
                Quaternion newQ = qRotDelta * curQ;
                float qLen = std::sqrt(newQ.x * newQ.x + newQ.y * newQ.y + newQ.z * newQ.z + newQ.w * newQ.w);
                if (qLen > 1e-5f) {
                    newQ.x /= qLen;
                    newQ.y /= qLen;
                    newQ.z /= qLen;
                    newQ.w /= qLen;
                }

                bool found = false;
                for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
                    if (std::abs(nodeAnim.rotate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                        nodeAnim.rotate[idx].value = newQ;
                        context->GetSelectedKeyIndex() = static_cast<int>(idx);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    context->GetTempOverrides()[context->GetSelectedJointName()].rotate = newQ;
                }
                syncGizmoOppositeSRT(nullptr, &newQ, nullptr);
                context->UpdateAnimationPosePreview(sceneManager);
            }
        }
    }
    // --------------------------------------------------------
    // Mode 2: Scale (拡大縮小)
    // --------------------------------------------------------
    else if (context->GetGizmoMode() == 2) {
        ImVec2 screenTips[3];
        bool tipsValid[3] = { false, false, false };
        int hoveredAxis = -1;
        float minD = 12.0f;

        // Check center box (uniform scale)
        float distCenter = std::sqrt((mousePos.x - screenOrigin.x) * (mousePos.x - screenOrigin.x) + (mousePos.y - screenOrigin.y) * (mousePos.y - screenOrigin.y));
        if (distCenter <= 8.0f) {
            hoveredAxis = 3;
        } else {
            for (int i = 0; i < 3; ++i) {
                Vector3 tipPos = origin + axes[i] * gizmoRadius;
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
        }

        if (!isDraggingAnimGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingAnimGizmo_ = true;
            animGizmoActiveAxis_ = hoveredAxis;
            animGizmoDragStartMouse_ = mousePos;
            context->BeginDragSnapshot("ギズモ拡縮");
        }

        // Draw axis lines and boxes
        for (int i = 0; i < 3; ++i) {
            if (!tipsValid[i]) continue;
            bool isAct = (animGizmoActiveAxis_ == i) || (!isDraggingAnimGizmo_ && hoveredAxis == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 3.5f : 2.0f;

            drawList->AddLine(screenOrigin, screenTips[i], col, thick);

            // Tip box
            float bSz = isAct ? 7.0f : 5.0f;
            drawList->AddRectFilled(ImVec2(screenTips[i].x - bSz, screenTips[i].y - bSz), ImVec2(screenTips[i].x + bSz, screenTips[i].y + bSz), col);
            drawList->AddRect(ImVec2(screenTips[i].x - bSz, screenTips[i].y - bSz), ImVec2(screenTips[i].x + bSz, screenTips[i].y + bSz), IM_COL32(20, 20, 20, 240));
        }

        // Draw center box
        bool centerAct = (animGizmoActiveAxis_ == 3) || (!isDraggingAnimGizmo_ && hoveredAxis == 3);
        float cSz = centerAct ? 8.0f : 6.0f;
        ImU32 cCol = centerAct ? IM_COL32(255, 255, 140, 255) : IM_COL32(240, 220, 80, 240);
        drawList->AddRectFilled(ImVec2(screenOrigin.x - cSz, screenOrigin.y - cSz), ImVec2(screenOrigin.x + cSz, screenOrigin.y + cSz), cCol);
        drawList->AddRect(ImVec2(screenOrigin.x - cSz, screenOrigin.y - cSz), ImVec2(screenOrigin.x + cSz, screenOrigin.y + cSz), IM_COL32(20, 20, 20, 255));

        // Handle Dragging Scale
        if (isDraggingAnimGizmo_ && animGizmoActiveAxis_ >= 0) {
            Vector3 curS = joint.defaultTransform.scale;
            if (!nodeAnim.scale.empty()) {
                curS = CalculateValue(nodeAnim.scale, context->GetAnimEditorTime());
            }
            auto itTempS = context->GetTempOverrides().find(context->GetSelectedJointName());
            if (itTempS != context->GetTempOverrides().end() && itTempS->second.scale) {
                curS = *itTempS->second.scale;
            }

            if (animGizmoActiveAxis_ == 3) {
                // Uniform scale
                float proj = io.MouseDelta.x - io.MouseDelta.y;
                float factor = 1.0f + proj * 0.02f;
                curS = curS * factor;
            } else {
                int a = animGizmoActiveAxis_;
                if (tipsValid[a]) {
                    ImVec2 dir2D = ImVec2(screenTips[a].x - screenOrigin.x, screenTips[a].y - screenOrigin.y);
                    float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
                    if (len2D > 1.0f) {
                        dir2D.x /= len2D;
                        dir2D.y /= len2D;
                        float proj = io.MouseDelta.x * dir2D.x + io.MouseDelta.y * dir2D.y;
                        float factor = 1.0f + proj * 0.02f;
                        if (a == 0) curS.x *= factor;
                        else if (a == 1) curS.y *= factor;
                        else if (a == 2) curS.z *= factor;
                    }
                }
            }

            curS.x = (std::max)(0.001f, curS.x);
            curS.y = (std::max)(0.001f, curS.y);
            curS.z = (std::max)(0.001f, curS.z);

            bool found = false;
            for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
                if (std::abs(nodeAnim.scale[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    nodeAnim.scale[idx].value = curS;
                    found = true;
                    break;
                }
            }
            if (!found) {
                context->GetTempOverrides()[context->GetSelectedJointName()].scale = curS;
            }
            syncGizmoOppositeSRT(nullptr, nullptr, &curS);
            context->UpdateAnimationPosePreview(sceneManager);
        }
    }

    if (context->GetIsAnimLocked()) {
        drawList->AddText(ImVec2(screenOrigin.x + 8, screenOrigin.y - 18), IM_COL32(255, 120, 120, 255), "[LOCKED (L)]");
    }

    if (!io.MouseDown[0]) {
        if (isDraggingAnimGizmo_ && context->GetHasAnimDragPreSnapshot()) {
            context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
            if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
            context->GetRedoStack().clear();
            context->GetHasAnimDragPreSnapshot() = false;
        }
        isDraggingAnimGizmo_ = false;
        animGizmoActiveAxis_ = -1;
    }

    drawList->PopClipRect();
}

void AnimationEditorViewport::DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, AnimationEditorContext* context) {
    // 3Dレンダリング結果のプレビュー表示
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
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
    ImVec2 animViewPos = ImGui::GetCursorScreenPos();
    ImVec2 animViewSize = imageSize;
    ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

    // カメラ軸スナップの線形補間アニメーション更新
    Camera* cam = activeCamera ? *activeCamera : nullptr;
    if (cam) {
        if (isCameraSnapLerping_) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[1] || io.MouseDown[2] || io.MouseWheel != 0.0f) {
                // ユーザーによる手動操作があれば補間中断
                isCameraSnapLerping_ = false;
            } else {
                float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;
                cameraSnapLerpTimer_ += dt;
                float t = std::clamp(cameraSnapLerpTimer_ / cameraSnapLerpDuration_, 0.0f, 1.0f);
                float easeT = t * t * (3.0f - 2.0f * t); // スムーズステップ

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

        // ボーン（スケルトン）位置の可視化＆選択オーバーレイ描画
        DrawSkeletonJointsOverlay(sceneManager, cam, animViewPos, animViewSize, context);

        // ボーン SRT ギズモ（マニピュレーター）の描画 & 操作
        DrawBoneTransformGizmo(sceneManager, cam, animViewPos, animViewSize, context);

        // カメラ向きギズモ（スナップ対応）の描画
        DrawCameraOrientationGizmo(cam, animViewPos, animViewSize);
    }

    // ビューポート左上にHUD / ギズモツールバー表示
    ImGui::SetCursorPos(ImVec2(currentPos.x + 10.0f, currentPos.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.70f));
    
    if (context->GetIsAnimHudMinimized()) {
        // 縮小化表示 (「拡大化」ボタンのみを表示)
        ImGui::BeginChild("##AnimViewportHUD", ImVec2(96, 40), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        if (ImGui::Button("拡大化", ImVec2(80, 24))) {
            context->GetIsAnimHudMinimized() = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("HUDを展開 (Hキーで切替)");
        ImGui::PopStyleVar();
        ImGui::EndChild();
    } else {
        // 通常（展開）表示
        ImGui::BeginChild("##AnimViewportHUD", ImVec2(480, 115), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        // 左上に縮小化ボタンを配置（拡大化ボタンと同じ位置）
        if (ImGui::Button("縮小化", ImVec2(80, 24))) {
            context->GetIsAnimHudMinimized() = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("HUDを縮小化 (Hキーで切替)");

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[3D Viewport] アニメーションビューポート");

        int curF = static_cast<int>(std::round(context->GetAnimEditorTime() * context->GetAnimEditorFps()));
        int totF = static_cast<int>(std::round(context->GetEditingAnimation().duration * context->GetAnimEditorFps()));
        ImGui::Text("フレーム: %d / %d  (%.3fs)", curF, totF, context->GetAnimEditorTime());
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "選択ボーン: %s%s", context->GetSelectedJointName().c_str(), context->GetIsAnimLocked() ? " [固定中]" : "");

        // ギズモツールバー (SRT / Local-World / Lock)
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        {
            bool isTrans = (context->GetGizmoMode() == 0);
            if (isTrans) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("[T] 移動", ImVec2(62, 20))) context->GetGizmoMode() = 0;
            if (isTrans) ImGui::PopStyleColor();

            ImGui::SameLine();
            bool isRot = (context->GetGizmoMode() == 1);
            if (isRot) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.75f, 0.3f, 1.0f));
            if (ImGui::Button("[R] 回転", ImVec2(62, 20))) context->GetGizmoMode() = 1;
            if (isRot) ImGui::PopStyleColor();

            ImGui::SameLine();
            bool isScale = (context->GetGizmoMode() == 2);
            if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
            if (ImGui::Button("[S] 拡縮", ImVec2(62, 20))) context->GetGizmoMode() = 2;
            if (isScale) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::Button(context->GetGizmoSpace() == 0 ? "Local" : "World", ImVec2(52, 20))) {
                context->GetGizmoSpace() = (context->GetGizmoSpace() == 0) ? 1 : 0;
            }

            ImGui::SameLine();
            if (context->GetIsAnimLocked()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
                if (ImGui::Button("[L] ロック中", ImVec2(88, 20))) context->GetIsAnimLocked() = false;
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button("[L] ロック", ImVec2(75, 20))) context->GetIsAnimLocked() = true;
            }
        }
        ImGui::PopStyleVar(); // ItemSpacing
        ImGui::PopStyleVar(); // FrameRounding

        ImGui::EndChild();
    }
    ImGui::PopStyleColor();

    // ショートカットキー判定 (Ctrl+Z: 元に戻す, Ctrl+Y: やり直す, T/R/S でギズモ切替, L でロック切替, H でHUD縮小/展開切替)
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                if (io.KeyShift) {
                    context->PerformAnimRedo(sceneManager);
                } else {
                    context->PerformAnimUndo(sceneManager);
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                context->PerformAnimRedo(sceneManager);
            }
        } else {
            if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
                if (io.KeyShift) {
                    context->InsertAllJointsSRTKey(sceneManager);
                } else {
                    context->InsertSelectedJointSRTKey(sceneManager);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_T, false)) context->GetGizmoMode() = 0; // Translate (移動)
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) context->GetGizmoMode() = 1; // Rotate (回転)
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) context->GetGizmoMode() = 2; // Scale (拡縮)
            if (ImGui::IsKeyPressed(ImGuiKey_L, false)) context->GetIsAnimLocked() = !context->GetIsAnimLocked(); // Lock (ロック)
            if (ImGui::IsKeyPressed(ImGuiKey_H, false)) context->GetIsAnimHudMinimized() = !context->GetIsAnimHudMinimized(); // HUD Minimize toggle
        }
    }
}



#endif
