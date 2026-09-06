#ifdef USE_IMGUI
#include "AnimationInspector.h"
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
#include "AnimationPreviewScene.h"
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

AnimationInspector::AnimationInspector() {
    Initialize();
}
void AnimationInspector::Initialize() {
}

void AnimationInspector::DrawInspectorUI(SceneManager* sceneManager, AnimationEditorContext* context) {
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "[Animation Properties] ボーン SRT 設定");
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    bool canUndo = context->CanUndo();
    bool canRedo = context->CanRedo();

    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("戻る (Ctrl+Z)")) {
        context->PerformAnimUndo(sceneManager);
    }
    if (!canUndo) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canRedo) ImGui::BeginDisabled();
    if (ImGui::Button("進む (Ctrl+Y)")) {
        context->PerformAnimRedo(sceneManager);
    }
    if (!canRedo) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (context->GetIsAnimLocked()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("ロック中 (L)")) context->GetIsAnimLocked() = false;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lキーでボーン選択固定を解除");
    } else {
        if (ImGui::Button("ロック (L)")) context->GetIsAnimLocked() = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lキーで選択ボーンを固定（誤選択防止）");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------------
    // アニメーション全体設定 (Duration / FPS / 総フレーム数)
    // ----------------------------------------------------
    if (ImGui::CollapsingHeader("アニメーション全体設定 (Duration / FPS)", ImGuiTreeNodeFlags_DefaultOpen)) {
        int totalFrames = static_cast<int>(std::round(context->GetEditingAnimation().duration * context->GetAnimEditorFps()));
        int curFrame = static_cast<int>(std::round(context->GetAnimEditorTime() * context->GetAnimEditorFps()));

        ImGui::Text("フレーム情報: %d / %d F (%.3fs / %.3fs)", curFrame, totalFrames, context->GetAnimEditorTime(), context->GetEditingAnimation().duration);

        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragInt("現在フレーム (Current F)", &curFrame, 1.0f, 0, totalFrames, "%d F")) {
            if (curFrame < 0) curFrame = 0;
            if (curFrame > totalFrames) curFrame = totalFrames;
            context->GetAnimEditorTime() = static_cast<float>(curFrame) / (context->GetAnimEditorFps() > 0.0f ? context->GetAnimEditorFps() : 60.0f);
            context->GetTempOverrides().clear();
            context->UpdateAnimationPosePreview(sceneManager);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragInt("最大フレーム数 (Duration)", &totalFrames, 1.0f, 1, 6000, "%d F")) {
            if (totalFrames < 1) totalFrames = 1;
            context->GetEditingAnimation().duration = static_cast<float>(totalFrames) / (context->GetAnimEditorFps() > 0.0f ? context->GetAnimEditorFps() : 60.0f);
        }
        if (ImGui::IsItemActivated()) {
            context->BeginDragSnapshot("フレーム数変更");
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && context->GetHasAnimDragPreSnapshot()) {
            context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
            if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
            context->GetRedoStack().clear();
            context->GetHasAnimDragPreSnapshot() = false;
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%.2f 秒]", context->GetEditingAnimation().duration);

        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("フレームレート (FPS)", &context->GetAnimEditorFps(), 1.0f, 10.0f, 120.0f, "%.0f fps");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    std::string targetName = "Player (プレイヤー)";
    if (context->GetSelectedGameObject()) targetName = context->GetSelectedGameObject()->GetName();
    else if (context->GetSelectedObject()) targetName = context->GetSelectedObject()->GetName();
    ImGui::Text("対象モデル: %s", targetName.c_str());

    ImGui::Spacing();
    ImGui::Text("選択ボーン (Joint):");
    ImGui::SetNextItemWidth(-1.0f);
    if (context->GetIsAnimLocked()) ImGui::BeginDisabled();
    if (ImGui::BeginCombo("##SelectedJointCombo", context->GetSelectedJointName().c_str())) {
        for (const auto& jName : context->GetCurrentJointList()) {
            bool isSel = (context->GetSelectedJointName() == jName);
            if (ImGui::Selectable(jName.c_str(), isSel)) {
                context->GetSelectedJointName() = jName;
                context->GetSelectedKeyIndex() = -1;
                context->GetTempOverrides().clear();
                context->UpdateAnimationPosePreview(sceneManager);
            }
        }
        ImGui::EndCombo();
    }
    if (context->GetIsAnimLocked()) ImGui::EndDisabled();

    ImGui::Spacing();

    // ギズモ操作モード切替ボタン群 (SRT & Lock)
    ImGui::Text("ギズモ操作モード:");
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    {
        bool isTrans = (context->GetGizmoMode() == 0);
        if (isTrans) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("移動 (T)", ImVec2(65, 24))) context->GetGizmoMode() = 0;
        if (isTrans) ImGui::PopStyleColor();

        ImGui::SameLine();
        bool isRot = (context->GetGizmoMode() == 1);
        if (isRot) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.75f, 0.3f, 1.0f));
        if (ImGui::Button("回転 (R)", ImVec2(65, 24))) context->GetGizmoMode() = 1;
        if (isRot) ImGui::PopStyleColor();

        ImGui::SameLine();
        bool isScale = (context->GetGizmoMode() == 2);
        if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
        if (ImGui::Button("拡縮 (S)", ImVec2(65, 24))) context->GetGizmoMode() = 2;
        if (isScale) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button(context->GetGizmoSpace() == 0 ? "Local" : "World", ImVec2(55, 24))) {
            context->GetGizmoSpace() = (context->GetGizmoSpace() == 0) ? 1 : 0;
        }

        ImGui::SameLine();
        if (context->GetIsAnimLocked()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button("ロック中", ImVec2(80, 24))) context->GetIsAnimLocked() = false;
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("ロック (L)", ImVec2(80, 24))) context->GetIsAnimLocked() = true;
        }
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (context->GetIsAnimLocked()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("[ボーン選択固定中] Lキーで解除");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    NodeAnimation& nodeAnim = context->GetEditingAnimation().nodeAnimations[context->GetSelectedJointName()];
    AnimatorComponent* anim = context->GetTargetAnimator(sceneManager);

    // ----------------------------------------------------
    // 対称編集モード (Symmetry Mode) 設定UI
    // ----------------------------------------------------
    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.95f, 1.0f), "[Symmetry] リアルタイム対称編集 (ミラー編集):");
    ImGui::Checkbox("対称編集を有効化", &context->GetAnimSymmetryMode());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("有効にすると、操作したボーンに対となるボーンが対称的に連動してリアルタイム更新されます");

    bool hasAnySymmetryAxis = context->GetAnimSymmetryAxisX() || context->GetAnimSymmetryAxisY() || context->GetAnimSymmetryAxisZ();
    std::string oppJointName = (context->GetAnimSymmetryMode() && hasAnySymmetryAxis) ? context->FindOppositeJointName(context->GetSelectedJointName(), context->GetAnimSymmetryAxisX(), context->GetAnimSymmetryAxisY(), context->GetAnimSymmetryAxisZ(), anim ? &anim->GetSkeleton() : nullptr) : "";

    if (context->GetAnimSymmetryMode()) {
        ImGui::SameLine();
        ImGui::Text("  対称軸:");
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        // X ボタン
        if (context->GetAnimSymmetryAxisX()) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        if (ImGui::Button("X", ImVec2(28, 22))) {
            context->GetAnimSymmetryAxisX() = !context->GetAnimSymmetryAxisX();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("X軸対称 (左右ミラー): %s", context->GetAnimSymmetryAxisX() ? "ON" : "OFF");

        ImGui::SameLine();
        // Y ボタン
        if (context->GetAnimSymmetryAxisY()) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.75f, 0.25f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        if (ImGui::Button("Y", ImVec2(28, 22))) {
            context->GetAnimSymmetryAxisY() = !context->GetAnimSymmetryAxisY();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Y軸対称 (上下ミラー): %s", context->GetAnimSymmetryAxisY() ? "ON" : "OFF");

        ImGui::SameLine();
        // Z ボタン
        if (context->GetAnimSymmetryAxisZ()) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.85f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        if (ImGui::Button("Z", ImVec2(28, 22))) {
            context->GetAnimSymmetryAxisZ() = !context->GetAnimSymmetryAxisZ();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Z軸対称 (前後ミラー): %s", context->GetAnimSymmetryAxisZ() ? "ON" : "OFF");

        ImGui::PopStyleVar(2);

        if (!hasAnySymmetryAxis) {
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "  -> (対称軸が選択されていません: XYZボタンをクリックして選択)");
        } else if (!oppJointName.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "  -> 対称ボーン: %s (連動中)", oppJointName.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  -> (単一/中央ボーン: 対称先なし)");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 対称ボーンへのリアルタイム連動更新ヘルパー
    auto syncOppositeBoneSRT = [&](const Vector3* newTrans, const Quaternion* newRot, const Vector3* newScale, bool isExplicitInsert = false) {
        if (!context->GetAnimSymmetryMode() || !hasAnySymmetryAxis || oppJointName.empty() || oppJointName == context->GetSelectedJointName()) return;
        if (!anim || !anim->HasSkeleton()) return;

        const Skeleton& skeleton = anim->GetSkeleton();

        Vector3 curS = nodeAnim.scale.empty() ? Vector3{ 1.0f, 1.0f, 1.0f } : CalculateValue(nodeAnim.scale, context->GetAnimEditorTime());
        Quaternion curR = nodeAnim.rotate.empty() ? Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } : CalculateValue(nodeAnim.rotate, context->GetAnimEditorTime());
        Vector3 curT = nodeAnim.translate.empty() ? Vector3{ 0.0f, 0.0f, 0.0f } : CalculateValue(nodeAnim.translate, context->GetAnimEditorTime());

        auto itTemp = context->GetTempOverrides().find(context->GetSelectedJointName());
        if (itTemp != context->GetTempOverrides().end()) {
            if (itTemp->second.translate) curT = *itTemp->second.translate;
            if (itTemp->second.rotate) curR = *itTemp->second.rotate;
            if (itTemp->second.scale) curS = *itTemp->second.scale;
        }

        if (newTrans) curT = *newTrans;
        if (newRot) curR = *newRot;
        if (newScale) curS = *newScale;

        Vector3 oppS, oppT;
        Quaternion oppQ;
        if (!ComputeBlenderSymmetrySRT(skeleton, context->GetSelectedJointName(), oppJointName, curS, curR, curT, context->GetAnimSymmetryAxisX(), context->GetAnimSymmetryAxisY(), context->GetAnimSymmetryAxisZ(), oppS, oppQ, oppT)) {
            return;
        }

        NodeAnimation& oppNodeAnim = context->GetEditingAnimation().nodeAnimations[oppJointName];

        if (newTrans) {
            bool found = false;
            for (size_t idx = 0; idx < oppNodeAnim.translate.size(); ++idx) {
                if (std::abs(oppNodeAnim.translate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    oppNodeAnim.translate[idx].value = oppT;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (isExplicitInsert) {
                    KeyframeVector3 newKf{ context->GetAnimEditorTime(), oppT };
                    auto itK = oppNodeAnim.translate.begin();
                    while (itK != oppNodeAnim.translate.end() && itK->time < newKf.time) ++itK;
                    oppNodeAnim.translate.insert(itK, newKf);
                } else {
                    context->GetTempOverrides()[oppJointName].translate = oppT;
                }
            }
        }

        if (newRot) {
            bool found = false;
            for (size_t idx = 0; idx < oppNodeAnim.rotate.size(); ++idx) {
                if (std::abs(oppNodeAnim.rotate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    oppNodeAnim.rotate[idx].value = oppQ;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (isExplicitInsert) {
                    KeyframeQuaternion newKf{ context->GetAnimEditorTime(), oppQ };
                    auto itK = oppNodeAnim.rotate.begin();
                    while (itK != oppNodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
                    oppNodeAnim.rotate.insert(itK, newKf);
                } else {
                    context->GetTempOverrides()[oppJointName].rotate = oppQ;
                }
            }
        }

        if (newScale) {
            bool found = false;
            for (size_t idx = 0; idx < oppNodeAnim.scale.size(); ++idx) {
                if (std::abs(oppNodeAnim.scale[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                    oppNodeAnim.scale[idx].value = oppS;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (isExplicitInsert) {
                    KeyframeVector3 newKf{ context->GetAnimEditorTime(), oppS };
                    auto itK = oppNodeAnim.scale.begin();
                    while (itK != oppNodeAnim.scale.end() && itK->time < newKf.time) ++itK;
                    oppNodeAnim.scale.insert(itK, newKf);
                } else {
                    context->GetTempOverrides()[oppJointName].scale = oppS;
                }
            }
        }
    };

    // ----------------------------------------------------
    // 1. 平行移動 (Translation / T)
    // ----------------------------------------------------
    Vector3 curTrans = { 0.0f, 0.0f, 0.0f };
    auto itTempT = context->GetTempOverrides().find(context->GetSelectedJointName());
    if (itTempT != context->GetTempOverrides().end() && itTempT->second.translate) {
        curTrans = *itTempT->second.translate;
    } else if (!nodeAnim.translate.empty()) {
        curTrans = CalculateValue(nodeAnim.translate, context->GetAnimEditorTime());
    } else if (anim && anim->HasSkeleton()) {
        const auto& skel = anim->GetSkeleton();
        auto itJ = skel.jointMap.find(context->GetSelectedJointName());
        if (itJ != skel.jointMap.end()) {
            curTrans = skel.joints[itJ->second].transform.translate;
        }
    }

    float transArr[3] = { curTrans.x, curTrans.y, curTrans.z };
    ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "[T] 平行移動 (Translation):");
    if (ImGui::DragFloat3("位置 (X, Y, Z)##Trans", transArr, 0.01f, -100.0f, 100.0f, "%.3f")) {
        Vector3 newTrans{ transArr[0], transArr[1], transArr[2] };
        bool found = false;
        for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
            if (std::abs(nodeAnim.translate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                nodeAnim.translate[idx].value = newTrans;
                found = true;
                break;
            }
        }
        if (!found) {
            context->GetTempOverrides()[context->GetSelectedJointName()].translate = newTrans;
        }
        syncOppositeBoneSRT(&newTrans, nullptr, nullptr);
        context->UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::IsItemActivated()) {
        context->BeginDragSnapshot("位置変更");
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && context->GetHasAnimDragPreSnapshot()) {
        context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
        if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
        context->GetRedoStack().clear();
        context->GetHasAnimDragPreSnapshot() = false;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##T")) {
        context->PushAnimUndoState("位置リセット");
        Vector3 defTrans = { 0.0f, 0.0f, 0.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(context->GetSelectedJointName());
            if (itJ != skel.jointMap.end()) defTrans = skel.joints[itJ->second].defaultTransform.translate;
        }
        KeyframeVector3 newKf{ context->GetAnimEditorTime(), defTrans };
        nodeAnim.translate.push_back(newKf);
        syncOppositeBoneSRT(&defTrans, nullptr, nullptr);
        context->GetTempOverrides()[context->GetSelectedJointName()].translate.reset();
        if (!oppJointName.empty()) context->GetTempOverrides()[oppJointName].translate.reset();
        context->UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();

    // ----------------------------------------------------
    // 2. 回転 (Rotation / R)
    // ----------------------------------------------------
    Quaternion curQuat = { 0.0f, 0.0f, 0.0f, 1.0f };
    auto itTempR = context->GetTempOverrides().find(context->GetSelectedJointName());
    if (itTempR != context->GetTempOverrides().end() && itTempR->second.rotate) {
        curQuat = *itTempR->second.rotate;
    } else if (!nodeAnim.rotate.empty()) {
        curQuat = CalculateValue(nodeAnim.rotate, context->GetAnimEditorTime());
    } else if (anim && anim->HasSkeleton()) {
        const auto& skel = anim->GetSkeleton();
        auto itJ = skel.jointMap.find(context->GetSelectedJointName());
        if (itJ != skel.jointMap.end()) {
            curQuat = skel.joints[itJ->second].transform.rotate;
        }
    }

    Vector3 euler = curQuat.ToEulerAngles();
    float eulerDeg[3] = { euler.x * 180.0f / 3.14159265f, euler.y * 180.0f / 3.14159265f, euler.z * 180.0f / 3.14159265f };
    float eulerRad[3] = { euler.x, euler.y, euler.z };

    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.4f, 1.0f), "[R] 回転 (Rotation):");
    bool rotChanged = false;
    if (ImGui::DragFloat3("度 (Deg: X, Y, Z)##RotDeg", eulerDeg, 0.5f, -360.0f, 360.0f, "%.1f°")) {
        rotChanged = true;
    }
    if (ImGui::IsItemActivated()) {
        context->BeginDragSnapshot("回転変更");
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && context->GetHasAnimDragPreSnapshot()) {
        context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
        if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
        context->GetRedoStack().clear();
        context->GetHasAnimDragPreSnapshot() = false;
    }

    if (ImGui::DragFloat3("ラジアン (Rad)##RotRad", eulerRad, 0.01f, -6.283f, 6.283f, "%.3f rad")) {
        eulerDeg[0] = eulerRad[0] * 180.0f / 3.14159265f;
        eulerDeg[1] = eulerRad[1] * 180.0f / 3.14159265f;
        eulerDeg[2] = eulerRad[2] * 180.0f / 3.14159265f;
        rotChanged = true;
    }
    if (ImGui::IsItemActivated()) {
        context->BeginDragSnapshot("回転変更");
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && context->GetHasAnimDragPreSnapshot()) {
        context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
        if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
        context->GetRedoStack().clear();
        context->GetHasAnimDragPreSnapshot() = false;
    }

    if (rotChanged) {
        float rx = eulerDeg[0] * 3.14159265f / 180.0f;
        float ry = eulerDeg[1] * 3.14159265f / 180.0f;
        float rz = eulerDeg[2] * 3.14159265f / 180.0f;
        Quaternion newQ = MakeEulerQuat(rx, ry, rz);

        bool foundKey = false;
        for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
            if (std::abs(nodeAnim.rotate[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                nodeAnim.rotate[idx].value = newQ;
                context->GetSelectedKeyIndex() = static_cast<int>(idx);
                foundKey = true;
                break;
            }
        }
        if (!foundKey) {
            context->GetTempOverrides()[context->GetSelectedJointName()].rotate = newQ;
        }
        syncOppositeBoneSRT(nullptr, &newQ, nullptr);
        context->UpdateAnimationPosePreview(sceneManager);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##R")) {
        context->PushAnimUndoState("回転リセット");
        Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(context->GetSelectedJointName());
            if (itJ != skel.jointMap.end()) defRot = skel.joints[itJ->second].defaultTransform.rotate;
        }
        KeyframeQuaternion newKf{ context->GetAnimEditorTime(), defRot };
        nodeAnim.rotate.push_back(newKf);
        syncOppositeBoneSRT(nullptr, &defRot, nullptr);
        context->GetTempOverrides()[context->GetSelectedJointName()].rotate.reset();
        if (!oppJointName.empty()) context->GetTempOverrides()[oppJointName].rotate.reset();
        context->UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();

    // ----------------------------------------------------
    // 3. 拡大縮小 (Scale / S)
    // ----------------------------------------------------
    Vector3 curScale = { 1.0f, 1.0f, 1.0f };
    auto itTempS = context->GetTempOverrides().find(context->GetSelectedJointName());
    if (itTempS != context->GetTempOverrides().end() && itTempS->second.scale) {
        curScale = *itTempS->second.scale;
    } else if (!nodeAnim.scale.empty()) {
        curScale = CalculateValue(nodeAnim.scale, context->GetAnimEditorTime());
    } else if (anim && anim->HasSkeleton()) {
        const auto& skel = anim->GetSkeleton();
        auto itJ = skel.jointMap.find(context->GetSelectedJointName());
        if (itJ != skel.jointMap.end()) {
            curScale = skel.joints[itJ->second].transform.scale;
        }
    }

    float scaleArr[3] = { curScale.x, curScale.y, curScale.z };
    ImGui::TextColored(ImVec4(0.4f, 0.65f, 0.95f, 1.0f), "[S] 拡大縮小 (Scale):");
    if (ImGui::DragFloat3("スケール (X, Y, Z)##Scale", scaleArr, 0.01f, 0.001f, 20.0f, "%.3f")) {
        Vector3 newSc{ scaleArr[0], scaleArr[1], scaleArr[2] };
        bool found = false;
        for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
            if (std::abs(nodeAnim.scale[idx].time - context->GetAnimEditorTime()) < 0.005f) {
                nodeAnim.scale[idx].value = newSc;
                found = true;
                break;
            }
        }
        if (!found) {
            context->GetTempOverrides()[context->GetSelectedJointName()].scale = newSc;
        }
        syncOppositeBoneSRT(nullptr, nullptr, &newSc);
        context->UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::IsItemActivated()) {
        context->BeginDragSnapshot("スケール変更");
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && context->GetHasAnimDragPreSnapshot()) {
        context->GetUndoStack().push_back(context->GetAnimDragPreSnapshot());
        if (context->GetUndoStack().size() > 64) context->GetUndoStack().erase(context->GetUndoStack().begin());
        context->GetRedoStack().clear();
        context->GetHasAnimDragPreSnapshot() = false;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##S")) {
        context->PushAnimUndoState("スケールリセット");
        Vector3 defSc = { 1.0f, 1.0f, 1.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(context->GetSelectedJointName());
            if (itJ != skel.jointMap.end()) defSc = skel.joints[itJ->second].defaultTransform.scale;
        }
        KeyframeVector3 newKf{ context->GetAnimEditorTime(), defSc };
        nodeAnim.scale.push_back(newKf);
        syncOppositeBoneSRT(nullptr, nullptr, &defSc);
        context->GetTempOverrides()[context->GetSelectedJointName()].scale.reset();
        if (!oppJointName.empty()) context->GetTempOverrides()[oppJointName].scale.reset();
        context->UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------------
    // キー挿入・リセット ボタン群
    // ----------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.55f, 0.3f, 1.0f));
    if (ImGui::Button("[+] 全ボーンの全SRTをキー挿入 (Shift+I)", ImVec2(-1, 28))) {
        context->InsertAllJointsSRTKey(sceneManager);
    }
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.65f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.45f, 0.05f, 1.0f));
    if (ImGui::Button("[+] 選択ボーンの全SRTをキー挿入 (I)", ImVec2(-1, 26))) {
        context->InsertSelectedJointSRTKey(sceneManager);
    }
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("[-] 選択ボーンの現在キー削除 (Del)", ImVec2(-1, 24))) {
        context->PushAnimUndoState("現在キー削除");
        float curT = context->GetAnimEditorTime();
        nodeAnim.rotate.erase(
            std::remove_if(nodeAnim.rotate.begin(), nodeAnim.rotate.end(),
                [curT](const KeyframeQuaternion& kf) { return std::abs(kf.time - curT) < 0.005f; }),
            nodeAnim.rotate.end()
        );
        nodeAnim.translate.erase(
            std::remove_if(nodeAnim.translate.begin(), nodeAnim.translate.end(),
                [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
            nodeAnim.translate.end()
        );
        nodeAnim.scale.erase(
            std::remove_if(nodeAnim.scale.begin(), nodeAnim.scale.end(),
                [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
            nodeAnim.scale.end()
        );
        context->GetSelectedKeyIndex() = -1;
        context->GetTempOverrides().erase(context->GetSelectedJointName());
        if (!oppJointName.empty()) context->GetTempOverrides().erase(oppJointName);
        context->UpdateAnimationPosePreview(sceneManager);
    }

    if (ImGui::Button("[-] 全ボーンの現在キー削除 (Shift+Del)", ImVec2(-1, 24))) {
        context->PushAnimUndoState("全ボーン現在キー削除");
        float curT = context->GetAnimEditorTime();
        for (auto& [nName, nAnim] : context->GetEditingAnimation().nodeAnimations) {
            nAnim.rotate.erase(
                std::remove_if(nAnim.rotate.begin(), nAnim.rotate.end(),
                    [curT](const KeyframeQuaternion& kf) { return std::abs(kf.time - curT) < 0.005f; }),
                nAnim.rotate.end()
            );
            nAnim.translate.erase(
                std::remove_if(nAnim.translate.begin(), nAnim.translate.end(),
                    [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
                nAnim.translate.end()
            );
            nAnim.scale.erase(
                std::remove_if(nAnim.scale.begin(), nAnim.scale.end(),
                    [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
                nAnim.scale.end()
            );
        }
        context->GetSelectedKeyIndex() = -1;
        context->GetTempOverrides().clear();
        context->UpdateAnimationPosePreview(sceneManager);
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    if (ImGui::Button("[T-Pose] 選択ボーンをTポーズ(0)に", ImVec2(-1, 24))) {
        context->PushAnimUndoState("Tポーズ設定");
        Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(context->GetSelectedJointName());
            if (itJ != skel.jointMap.end()) defRot = skel.joints[itJ->second].defaultTransform.rotate;
        }

        bool foundR = false;
        for (auto& kf : nodeAnim.rotate) {
            if (std::abs(kf.time - context->GetAnimEditorTime()) < 0.005f) {
                kf.value = defRot;
                foundR = true;
                break;
            }
        }
        if (!foundR) {
            KeyframeQuaternion newKf{ context->GetAnimEditorTime(), defRot };
            auto itK = nodeAnim.rotate.begin();
            while (itK != nodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
            auto inserted = nodeAnim.rotate.insert(itK, newKf);
            context->GetSelectedKeyIndex() = static_cast<int>(std::distance(nodeAnim.rotate.begin(), inserted));
        }
        context->GetTempOverrides().erase(context->GetSelectedJointName());
        if (!oppJointName.empty()) context->GetTempOverrides().erase(oppJointName);
        context->UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::Button("[T-Pose] 全ボーンをTポーズに (現在フレーム)", ImVec2(-1, 24))) {
        context->PushAnimUndoState("全ボーンTポーズ設定");
        const Skeleton* skelPtr = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

        for (const auto& jName : context->GetCurrentJointList()) {
            NodeAnimation& nAnim = context->GetEditingAnimation().nodeAnimations[jName];

            Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
            Vector3 defTrans{ 0.0f, 0.0f, 0.0f };
            Vector3 defScale{ 1.0f, 1.0f, 1.0f };

            if (skelPtr) {
                auto itJ = skelPtr->jointMap.find(jName);
                if (itJ != skelPtr->jointMap.end()) {
                    const auto& j = skelPtr->joints[itJ->second];
                    defRot = j.defaultTransform.rotate;
                    defTrans = j.defaultTransform.translate;
                    defScale = j.defaultTransform.scale;
                }
            }

            // 1. 回転をTポーズ(defaultTransform.rotate)に設定
            bool foundR = false;
            for (auto& kf : nAnim.rotate) {
                if (std::abs(kf.time - context->GetAnimEditorTime()) < 0.005f) {
                    kf.value = defRot;
                    foundR = true;
                    break;
                }
            }
            if (!foundR) {
                KeyframeQuaternion newKfR{ context->GetAnimEditorTime(), defRot };
                auto itR = nAnim.rotate.begin();
                while (itR != nAnim.rotate.end() && itR->time < newKfR.time) ++itR;
                nAnim.rotate.insert(itR, newKfR);
            }

            // 2. 移動 (すでに移動キーが存在する場合のみ、defaultTransform.translate に復帰)
            if (!nAnim.translate.empty()) {
                bool foundT = false;
                for (auto& kf : nAnim.translate) {
                    if (std::abs(kf.time - context->GetAnimEditorTime()) < 0.005f) {
                        kf.value = defTrans;
                        foundT = true;
                        break;
                    }
                }
                if (!foundT) {
                    KeyframeVector3 newKfT{ context->GetAnimEditorTime(), defTrans };
                    auto itT = nAnim.translate.begin();
                    while (itT != nAnim.translate.end() && itT->time < newKfT.time) ++itT;
                    nAnim.translate.insert(itT, newKfT);
                }
            }

            // 3. 拡縮 (すでに拡縮キーが存在する場合のみ、defaultTransform.scale に復帰)
            if (!nAnim.scale.empty()) {
                bool foundS = false;
                for (auto& kf : nAnim.scale) {
                    if (std::abs(kf.time - context->GetAnimEditorTime()) < 0.005f) {
                        kf.value = defScale;
                        foundS = true;
                        break;
                    }
                }
                if (!foundS) {
                    KeyframeVector3 newKfS{ context->GetAnimEditorTime(), defScale };
                    auto itS = nAnim.scale.begin();
                    while (itS != nAnim.scale.end() && itS->time < newKfS.time) ++itS;
                    nAnim.scale.insert(itS, newKfS);
                }
            }
        }
        context->GetTempOverrides().clear();
        context->UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::Button("[T-Pose] 全キーフレームをTポーズに初期化", ImVec2(-1, 24))) {
        context->PushAnimUndoState("全キーフレームTポーズ初期化");
        const Skeleton* skelPtr = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

        for (const auto& jName : context->GetCurrentJointList()) {
            NodeAnimation& nAnim = context->GetEditingAnimation().nodeAnimations[jName];
            nAnim.rotate.clear();
            nAnim.translate.clear();
            nAnim.scale.clear();

            Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
            Vector3 defTrans{ 0.0f, 0.0f, 0.0f };
            Vector3 defScale{ 1.0f, 1.0f, 1.0f };

            if (skelPtr) {
                auto itJ = skelPtr->jointMap.find(jName);
                if (itJ != skelPtr->jointMap.end()) {
                    const auto& j = skelPtr->joints[itJ->second];
                    defRot = j.defaultTransform.rotate;
                    defTrans = j.defaultTransform.translate;
                    defScale = j.defaultTransform.scale;
                }
            }

            nAnim.rotate.push_back({ 0.0f, defRot });
            nAnim.translate.push_back({ 0.0f, defTrans });
            nAnim.scale.push_back({ 0.0f, defScale });
        }
        context->GetSelectedKeyIndex() = -1;
        context->GetTempOverrides().clear();
        context->UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------------
    // キーフレーム一覧
    // ----------------------------------------------------
    std::vector<float> boneKeyTimes;
    {
        std::set<float> timeSet;
        for (const auto& k : nodeAnim.rotate) timeSet.insert(k.time);
        for (const auto& k : nodeAnim.translate) timeSet.insert(k.time);
        for (const auto& k : nodeAnim.scale) timeSet.insert(k.time);
        boneKeyTimes.assign(timeSet.begin(), timeSet.end());
        std::sort(boneKeyTimes.begin(), boneKeyTimes.end());
    }

    ImGui::Text("登録キーフレーム一覧 (%zu 個):", boneKeyTimes.size());
    if (boneKeyTimes.empty()) {
        ImGui::TextDisabled("キーフレームがありません。");
    } else {
        if (ImGui::BeginTable("##KeyframeListTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("フレーム / 時間", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 60.0f);

            int toDeleteIndex = -1;
            float toDeleteTime = -1.0f;

            for (size_t k = 0; k < boneKeyTimes.size(); ++k) {
                float kTime = boneKeyTimes[k];
                int kFrame = static_cast<int>(std::round(kTime * context->GetAnimEditorFps()));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                char kBuf[64];
                snprintf(kBuf, sizeof(kBuf), "Key %zu: %d F (%.3fs)##KeyRow%zu", k, kFrame, kTime, k);
                bool isCurTime = (std::abs(context->GetAnimEditorTime() - kTime) < 0.005f);

                if (ImGui::Selectable(kBuf, isCurTime)) {
                    context->GetAnimEditorTime() = kTime;
                    context->GetSelectedKeyIndex() = -1;
                    for (size_t ri = 0; ri < nodeAnim.rotate.size(); ++ri) {
                        if (std::abs(nodeAnim.rotate[ri].time - kTime) < 0.005f) {
                            context->GetSelectedKeyIndex() = static_cast<int>(ri);
                            break;
                        }
                    }
                    context->UpdateAnimationPosePreview(sceneManager);
                }

                ImGui::TableSetColumnIndex(1);
                char delBuf[32];
                snprintf(delBuf, sizeof(delBuf), "削除##KBtn%zu", k);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
                if (ImGui::SmallButton(delBuf)) {
                    toDeleteIndex = static_cast<int>(k);
                    toDeleteTime = kTime;
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::EndTable();

            if (toDeleteIndex >= 0) {
                context->PushAnimUndoState("キーフレーム削除");

                nodeAnim.rotate.erase(
                    std::remove_if(nodeAnim.rotate.begin(), nodeAnim.rotate.end(),
                        [toDeleteTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - toDeleteTime) < 0.005f; }),
                    nodeAnim.rotate.end()
                );

                nodeAnim.translate.erase(
                    std::remove_if(nodeAnim.translate.begin(), nodeAnim.translate.end(),
                        [toDeleteTime](const KeyframeVector3& kf) { return std::abs(kf.time - toDeleteTime) < 0.005f; }),
                    nodeAnim.translate.end()
                );

                nodeAnim.scale.erase(
                    std::remove_if(nodeAnim.scale.begin(), nodeAnim.scale.end(),
                        [toDeleteTime](const KeyframeVector3& kf) { return std::abs(kf.time - toDeleteTime) < 0.005f; }),
                    nodeAnim.scale.end()
                );

                context->GetSelectedKeyIndex() = -1;
                context->UpdateAnimationPosePreview(sceneManager);
            }
        }
    }

    // ショートカットキー判定 (Ctrl+Z: 元に戻す, Ctrl+Y: やり直す, T/R/S: ギズモ切替, L: ロック切替)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (io.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io.KeyShift) context->PerformAnimRedo(sceneManager);
                    else context->PerformAnimUndo(sceneManager);
                } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    context->PerformAnimRedo(sceneManager);
                }
            } else {
                if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
                    if (io.KeyShift) context->InsertAllJointsSRTKey(sceneManager);
                    else context->InsertSelectedJointSRTKey(sceneManager);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_T, false)) context->GetGizmoMode() = 0; // Translate (移動)
                if (ImGui::IsKeyPressed(ImGuiKey_R, false)) context->GetGizmoMode() = 1; // Rotate (回転)
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) context->GetGizmoMode() = 2; // Scale (拡縮)
                if (ImGui::IsKeyPressed(ImGuiKey_L, false)) context->GetIsAnimLocked() = !context->GetIsAnimLocked(); // Lock (ロック)
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) context->GetIsAnimHudMinimized() = !context->GetIsAnimHudMinimized(); // HUD Minimize toggle
            }
        }
    }

    // --- プレビューライティング設定 (Preview Lighting) ---
    auto* animScene = dynamic_cast<AnimationPreviewScene*>(sceneManager ? sceneManager->GetCurrentScene() : nullptr);
    if (animScene) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("[ライティング設定] プレビュー照明", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cfg = animScene->GetLightingConfig();

            // 1. 照明モード切り替え
            ImGui::Text("照明モード:");
            ImGui::SameLine();
            if (!cfg.useGameLighting) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.35f, 1.0f));
                ImGui::Button("[専用スタジオライト]");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("[ゲームシーンのライト]")) {
                    cfg.useGameLighting = true;
                }
                ImGui::TextDisabled("※アニメーションが見やすい専用スタジオ照明を使用しています。");
            } else {
                if (ImGui::Button("[専用スタジオライト]")) {
                    cfg.useGameLighting = false;
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.65f, 1.0f));
                ImGui::Button("[ゲームシーンのライト]");
                ImGui::PopStyleColor();
                ImGui::TextDisabled("※ゲーム本編のライト設定（平行光・点光源・スポット光）を使用しています。");
            }

            ImGui::Spacing();

            // 専用スタジオライト時の操作
            if (!cfg.useGameLighting) {
                // 2. プリセットボタングループ
                ImGui::Text("照明プリセット:");
                const char* presetNames[] = { "標準", "明るい", "陰影強調", "正面光", "輪郭強調" };
                for (int i = 0; i < 5; ++i) {
                    if (i > 0) ImGui::SameLine();
                    bool isCur = (cfg.currentPresetIndex == i);
                    if (isCur) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.4f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 0.48f, 1.0f));
                    }
                    if (ImGui::Button(presetNames[i])) {
                        cfg.ApplyPreset(i);
                    }
                    if (isCur) {
                        ImGui::PopStyleColor(2);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 3. 直感スライダー (全体明るさ・向き)
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.4f, 1.0f), "クイック調整 (直感操作)");
                ImGui::SliderFloat("全体の明るさ", &cfg.brightness, 0.2f, 3.0f, "%.2f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("キー光・リム光・環境光の全体の明るさを一括調整します");

                if (ImGui::SliderFloat("光の水平角度 (0~360度)", &cfg.horizontalAngleDeg, 0.0f, 360.0f, "%.0f 度")) {
                    cfg.RecalculateDirection();
                    cfg.currentPresetIndex = -1;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("水平方向から照らす向きを回転させます");

                if (ImGui::SliderFloat("光の高さ (仰角 10~80度)", &cfg.elevationDeg, 10.0f, 80.0f, "%.0f 度")) {
                    cfg.RecalculateDirection();
                    cfg.currentPresetIndex = -1;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("光の高さ（仰角）を調整します");

                ImGui::Spacing();

                // 4. 詳細設定 (折りたたみ)
                if (ImGui::TreeNode("詳細ライト調整 (キー光・リム光・環境光)")) {
                    // キーライト (平行光)
                    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "メインキーライト (平行光源)");
                    ImGui::Checkbox("キーライト有効##EnableKeyLight", &cfg.enableKeyLight);
                    if (cfg.enableKeyLight) {
                        ImGui::DragFloat("強度##KeyIntensity", &cfg.keyLightIntensity, 0.05f, 0.0f, 5.0f, "%.2f");
                        
                        float color[4] = { cfg.keyLightColor.x, cfg.keyLightColor.y, cfg.keyLightColor.z, cfg.keyLightColor.w };
                        if (ImGui::ColorEdit4("光色##KeyColor", color, ImGuiColorEditFlags_NoAlpha)) {
                            cfg.keyLightColor = { color[0], color[1], color[2], color[3] };
                        }

                        float dir[3] = { cfg.keyLightDirection.x, cfg.keyLightDirection.y, cfg.keyLightDirection.z };
                        if (ImGui::DragFloat3("光の向き##KeyDir", dir, 0.02f, -1.0f, 1.0f, "%.2f")) {
                            cfg.keyLightDirection = { dir[0], dir[1], dir[2] };
                            cfg.currentPresetIndex = -1;
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // リムライト (点光源)
                    ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "リムライト / 輪郭強調 (点光源)");
                    ImGui::Checkbox("リムライト有効##EnableRimLight", &cfg.enableRimLight);
                    if (cfg.enableRimLight) {
                        ImGui::DragFloat("強度##RimIntensity", &cfg.rimLightIntensity, 0.05f, 0.0f, 5.0f, "%.2f");
                        
                        float rimCol[4] = { cfg.rimLightColor.x, cfg.rimLightColor.y, cfg.rimLightColor.z, cfg.rimLightColor.w };
                        if (ImGui::ColorEdit4("光色##RimColor", rimCol, ImGuiColorEditFlags_NoAlpha)) {
                            cfg.rimLightColor = { rimCol[0], rimCol[1], rimCol[2], rimCol[3] };
                        }

                        float rimPos[3] = { cfg.rimLightPos.x, cfg.rimLightPos.y, cfg.rimLightPos.z };
                        if (ImGui::DragFloat3("光源位置##RimPos", rimPos, 0.1f, -20.0f, 20.0f, "%.1f")) {
                            cfg.rimLightPos = { rimPos[0], rimPos[1], rimPos[2] };
                        }
                        ImGui::DragFloat("照射半径##RimRadius", &cfg.rimLightRadius, 0.5f, 1.0f, 50.0f, "%.1f");
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // 環境光
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "環境光 (暗部ディテール)");
                    ImGui::DragFloat("環境光強度##AmbientIntensity", &cfg.ambientIntensity, 0.02f, 0.0f, 2.0f, "%.2f");

                    ImGui::TreePop();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 5. 保存・リセットボタン
            if (ImGui::Button("[ 設定を保存 ]", ImVec2(130, 26))) {
                animScene->SaveLightingConfig();
            }
            ImGui::SameLine();
            if (ImGui::Button("[ 初期値に戻す ]", ImVec2(130, 26))) {
                animScene->ResetLightingConfig();
                animScene->SaveLightingConfig();
            }
            ImGui::TextDisabled("※設定は resources/json/local/ に保存されます。");
        }
    }

    // 選択オブジェクト固有のインスペクター（Transform / Material 等）も下部に表示
    if (context->GetSelectedGameObject()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("オブジェクト コンポーネント", ImGuiTreeNodeFlags_DefaultOpen)) {
            context->GetSelectedGameObject()->DisplayImGui();
        }
    } else if (context->GetSelectedPrimitive()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        context->GetSelectedPrimitive()->DisplayImGui("プリミティブ プロパティ");
    } else if (context->GetSelectedObject()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        context->GetSelectedObject()->DisplayImGui("3Dオブジェクト プロパティ");
    }
}





#endif
