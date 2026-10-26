#ifdef USE_IMGUI
#include "EditorHierarchy.h"
#include "EditorManager.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "GameObject/GameObject.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "Effect/ParticleManager.h"
#include "Editor/Animation/AnimationEditor.h"
#include "Editor/Animation/AnimationPreviewScene.h"
#include "Editor/Model3DEditor/Model3DEditor.h"
#include "Editor/Model3DEditor/PlacedObject3D.h"
#include "Editor/PostEffectEditor/PostEffectEditor.h"
#include "Editor/Common/ModelSelectModal.h"
#include <imgui.h>

void EditorHierarchy::Draw(bool* pOpen, EditorManager* editorManager, SceneManager* sceneManager) {
    if (!pOpen || !*pOpen || !editorManager || !sceneManager) return;

    if (ImGui::Begin("ヒエラルキー", pOpen)) {
        IScene *activeScene = sceneManager->GetCurrentScene();
        if (activeScene) {
            AnimationPreviewScene* animPreviewScene = dynamic_cast<AnimationPreviewScene*>(activeScene);
            bool isAnimMode = (editorManager->GetCurrentMode() == EditorManager::EditorMode::Animation) || 
                              (editorManager->GetActiveMainTab() == "アニメーションエディター") || 
                              (animPreviewScene != nullptr);

            if (animPreviewScene || isAnimMode) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 0.95f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.68f, 1.0f));
                if (ImGui::Button("+ モデルを追加 (Add Model)", ImVec2(-1.0f, 30.0f))) {
                    editorManager->GetOpenAnimModelSelectModal() = true;
                    if (editorManager->GetAnimModelSelectModal()) {
                        editorManager->GetAnimModelSelectModal()->RefreshModelList();
                    }
                }
                ImGui::PopStyleColor(3);

                if (animPreviewScene) {
                    ImGui::Spacing();
                    float btnWidth = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
                    if (ImGui::Button("[保存]", ImVec2(btnWidth, 24.0f))) {
                        animPreviewScene->SaveHierarchyToJson();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("追加・編集したオブジェクト状態をJSONに手動保存します");

                    ImGui::SameLine();
                    if (ImGui::Button("[読込]", ImVec2(btnWidth, 24.0f))) {
                        animPreviewScene->LoadHierarchyFromJson();
                        editorManager->SetSelectedGameObject(animPreviewScene->GetSelectedGameObject());
                        if (editorManager->GetAnimationEditor()) {
                            editorManager->GetAnimationEditor()->SetSelectedTargets(
                                editorManager->GetSelectedObject(), 
                                editorManager->GetSelectedGameObject(), 
                                editorManager->GetSelectedPrimitive());
                            editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                        }
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("JSONからオブジェクト状態を再読み込みします");

                    ImGui::SameLine();
                    if (ImGui::Button("[初期化]", ImVec2(btnWidth, 24.0f))) {
                        animPreviewScene->ResetHierarchyToDefault();
                        editorManager->SetSelectedGameObject(animPreviewScene->GetSelectedGameObject());
                        if (editorManager->GetAnimationEditor()) {
                            editorManager->GetAnimationEditor()->SetSelectedTargets(
                                editorManager->GetSelectedObject(), 
                                editorManager->GetSelectedGameObject(), 
                                editorManager->GetSelectedPrimitive());
                            editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                        }
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("ヒエラルキーを初期状態（Playerのみ）にリセットします");

                    ImGui::TextDisabled("※モデル追加・削除時は自動保存されます");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("GameObjects", ImGuiTreeNodeFlags_DefaultOpen)) {
                    std::shared_ptr<GameObject> animObjToDelete = nullptr;
                    for (auto &obj : activeScene->GetGameObjects()) {
                        bool isSelected = (editorManager->GetSelectedGameObject() == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            editorManager->SetSelectedGameObject(obj);
                            editorManager->SetSelectedObject(nullptr);
                            editorManager->SetSelectedParticle(nullptr);
                            editorManager->SetSelectedPrimitive(nullptr);
                            if (animPreviewScene) {
                                animPreviewScene->SetSelectedGameObject(obj);
                            }
                            if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                            if (editorManager->GetAnimationEditor()) {
                                editorManager->GetAnimationEditor()->SetSelectedTargets(
                                    editorManager->GetSelectedObject(), 
                                    editorManager->GetSelectedGameObject(), 
                                    editorManager->GetSelectedPrimitive());
                                editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                            }
                        }

                        if (animPreviewScene) {
                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("削除 (Delete)")) {
                                    animObjToDelete = obj;
                                }
                                ImGui::EndPopup();
                            }
                        }
                    }
                    if (animObjToDelete && animPreviewScene) {
                        if (editorManager->GetSelectedGameObject() == animObjToDelete) {
                            editorManager->SetSelectedGameObject(nullptr);
                            if (editorManager->GetAnimationEditor()) {
                                editorManager->GetAnimationEditor()->SetSelectedTargets(nullptr, nullptr, nullptr);
                                editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                            }
                        }
                        animPreviewScene->RemoveGameObject(animObjToDelete);
                    }
                }

                // 選択中オブジェクトのスケルトンボーン一覧
                auto animEditor = editorManager->GetAnimationEditor();
                if (animEditor && !animEditor->GetCurrentJointList().empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    if (ImGui::TreeNodeEx("[Bones] ボーン / 関節", ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (const auto& jointName : animEditor->GetCurrentJointList()) {
                            bool isJointSelected = (animEditor->GetSelectedJointName() == jointName);
                            if (ImGui::Selectable(("  " + jointName).c_str(), isJointSelected)) {
                                animEditor->SetSelectedJointName(jointName);
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            } else if (editorManager->GetCurrentMode() == EditorManager::EditorMode::ModelPlacement || editorManager->GetActiveMainTab() == "3Dモデル配置") {
                if (ImGui::CollapsingHeader("GameObjects", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto &obj : activeScene->GetGameObjects()) {
                        bool isSelected = (editorManager->GetSelectedGameObject() == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            editorManager->SetSelectedGameObject(obj);
                            editorManager->SetSelectedObject(nullptr);
                            editorManager->SetSelectedParticle(nullptr);
                            editorManager->SetSelectedPrimitive(nullptr);
                            if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                        }
                    }
                }

                auto model3DEditor = editorManager->GetModel3DEditor();
                if (model3DEditor && !model3DEditor->GetPlacedObjects().empty()) {
                    if (ImGui::CollapsingHeader("3Dモデル配置 (Placed Models)", ImGuiTreeNodeFlags_DefaultOpen)) {
                        PlacedObject3D* objToDelete = nullptr;
                        for (const auto& placedObj : model3DEditor->GetPlacedObjects()) {
                            if (!placedObj) continue;
                            bool isSelected = (model3DEditor->GetSelectedObject() == placedObj.get());
                            std::string label = "  " + placedObj->GetName() + "##Placed_" + std::to_string((uintptr_t)placedObj.get());
                            if (ImGui::Selectable(label.c_str(), isSelected)) {
                                model3DEditor->SetSelectedObject(placedObj.get());
                                editorManager->SetPlayerSelected(false);
                                editorManager->SetSelectedGameObject(nullptr);
                                editorManager->SetSelectedObject(nullptr);
                                editorManager->SetSelectedParticle(nullptr);
                                editorManager->SetSelectedPrimitive(nullptr);
                            }

                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("複製 (Duplicate)")) {
                                    model3DEditor->GetContext()->DuplicateObject(placedObj.get());
                                }
                                if (ImGui::MenuItem("削除 (Delete)")) {
                                    objToDelete = placedObj.get();
                                }
                                ImGui::EndPopup();
                            }
                        }
                        if (objToDelete) {
                            model3DEditor->GetContext()->RemoveObject(objToDelete);
                        }
                    }
                }
            } else {
                // 通常モード時: 全カテゴリを表示
                // 1. プレイヤー（存在する場合）
                if (activeScene->GetPlayer()) {
                    if (ImGui::Selectable("[Player] プレイヤー", editorManager->IsPlayerSelected())) {
                        editorManager->SetSelectedGameObject(nullptr);
                        editorManager->SetSelectedParticle(nullptr);
                        editorManager->SetSelectedPrimitive(nullptr);
                        editorManager->SetSelectedObject(nullptr);
                        editorManager->SetPlayerSelected(true);
                        if (editorManager->GetModel3DEditor())
                            editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                    }
                }

                // 2. 3Dモデル配置オブジェクト
                auto model3DEditor = editorManager->GetModel3DEditor();
                if (model3DEditor && !model3DEditor->GetPlacedObjects().empty()) {
                    if (ImGui::CollapsingHeader("3Dモデル配置 (Placed Models)", ImGuiTreeNodeFlags_DefaultOpen)) {
                        PlacedObject3D* objToDelete = nullptr;
                        for (const auto& placedObj : model3DEditor->GetPlacedObjects()) {
                            if (!placedObj) continue;
                            bool isSelected = (model3DEditor->GetSelectedObject() == placedObj.get());
                            std::string label = "  " + placedObj->GetName() + "##Placed_" + std::to_string((uintptr_t)placedObj.get());
                            if (ImGui::Selectable(label.c_str(), isSelected)) {
                                model3DEditor->SetSelectedObject(placedObj.get());
                                editorManager->SetPlayerSelected(false);
                                editorManager->SetSelectedGameObject(nullptr);
                                editorManager->SetSelectedObject(nullptr);
                                editorManager->SetSelectedParticle(nullptr);
                                editorManager->SetSelectedPrimitive(nullptr);
                            }

                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("複製 (Duplicate)")) {
                                    model3DEditor->GetContext()->DuplicateObject(placedObj.get());
                                }
                                if (ImGui::MenuItem("削除 (Delete)")) {
                                    objToDelete = placedObj.get();
                                }
                                ImGui::EndPopup();
                            }
                        }
                        if (objToDelete) {
                            model3DEditor->GetContext()->RemoveObject(objToDelete);
                        }
                    }
                }

                if (ImGui::CollapsingHeader("GameObjects", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto &obj : activeScene->GetGameObjects()) {
                        bool isSelected = (editorManager->GetSelectedGameObject() == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            editorManager->SetSelectedGameObject(obj);
                            editorManager->SetSelectedObject(nullptr);
                            editorManager->SetSelectedParticle(nullptr);
                            editorManager->SetSelectedPrimitive(nullptr);
                            if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                            editorManager->SetPlayerSelected(false);
                            if (editorManager->GetAnimationEditor()) {
                                editorManager->GetAnimationEditor()->SetSelectedTargets(
                                    editorManager->GetSelectedObject(), 
                                    editorManager->GetSelectedGameObject(), 
                                    editorManager->GetSelectedPrimitive());
                                editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                            }
                        }
                    }
                }
                if (ImGui::CollapsingHeader("オブジェクト (Objects)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *obj : activeScene->GetObjects()) {
                        bool isSelected = (editorManager->GetSelectedObject() == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            editorManager->SetSelectedGameObject(nullptr);
                            editorManager->SetSelectedObject(obj);
                            editorManager->SetSelectedParticle(nullptr);
                            editorManager->SetSelectedPrimitive(nullptr);
                            if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                            editorManager->SetPlayerSelected(false);
                            if (editorManager->GetAnimationEditor()) {
                                editorManager->GetAnimationEditor()->SetSelectedTargets(
                                    editorManager->GetSelectedObject(), 
                                    editorManager->GetSelectedGameObject(), 
                                    editorManager->GetSelectedPrimitive());
                                editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                            }
                        }
                    }
                }
                if (ImGui::CollapsingHeader("パーティクル (Particles)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *particle : activeScene->GetParticles()) {
                        bool isSelected = (editorManager->GetSelectedParticle() == particle);
                        if (ImGui::Selectable(particle->GetName().c_str(), isSelected)) {
                            editorManager->SetSelectedGameObject(nullptr);
                            editorManager->SetSelectedObject(nullptr);
                            editorManager->SetSelectedParticle(particle);
                            editorManager->SetPlayerSelected(false);
                            editorManager->SetSelectedPrimitive(nullptr);
                            if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                        }
                    }
                }
                if (ImGui::CollapsingHeader("プリミティブ (Primitives)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *primitive : activeScene->GetPrimitives()) {
                        bool isSelected = (editorManager->GetSelectedPrimitive() == primitive);
                        if (ImGui::Selectable(primitive->GetName().c_str(), isSelected)) {
                            editorManager->SetSelectedGameObject(nullptr);
                            editorManager->SetSelectedObject(nullptr);
                            editorManager->SetSelectedParticle(nullptr);
                            editorManager->SetSelectedPrimitive(primitive);
                            editorManager->SetPlayerSelected(false);
                            if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                        }
                    }
                }

                auto animEditor = editorManager->GetAnimationEditor();
                if (animEditor && !animEditor->GetCurrentJointList().empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    if (ImGui::TreeNodeEx("[Bones] ボーン / 関節", ImGuiTreeNodeFlags_DefaultOpen)) {
                        for (const auto& jointName : animEditor->GetCurrentJointList()) {
                            bool isJointSelected = (animEditor->GetSelectedJointName() == jointName);
                            if (ImGui::Selectable(("  " + jointName).c_str(), isJointSelected)) {
                                animEditor->SetSelectedJointName(jointName);
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            }
        }

        // ポストエフェクト一覧
        if (editorManager->GetPostEffectEditor()) {
            ImGui::Spacing();
            ImGui::Separator();
            editorManager->GetPostEffectEditor()->DrawHierarchy();
        }

        // アニメーションエディタ用モデル選択モーダルダイアログ
        if (editorManager->GetOpenAnimModelSelectModal()) {
            ImGui::OpenPopup("モデルを選択してオブジェクトを追加##AnimModelSelectModal");
        }
        if (editorManager->GetAnimModelSelectModal()) {
            editorManager->GetAnimModelSelectModal()->DrawModal("モデルを選択してオブジェクトを追加##AnimModelSelectModal", editorManager->GetOpenAnimModelSelectModal(), [&](const ModelSelectItem& selectedItem) {
                auto* pScene = dynamic_cast<AnimationPreviewScene*>(sceneManager->GetCurrentScene());
                if (pScene) {
                    auto newObj = pScene->AddGameObjectFromModel(selectedItem.directoryPath, selectedItem.fileName, selectedItem.displayName);
                    if (newObj) {
                        editorManager->SetSelectedGameObject(newObj);
                        pScene->SetSelectedGameObject(newObj);
                        editorManager->SetSelectedObject(nullptr);
                        editorManager->SetSelectedParticle(nullptr);
                        editorManager->SetSelectedPrimitive(nullptr);
                        if (editorManager->GetModel3DEditor()) editorManager->GetModel3DEditor()->SetSelectedObject(nullptr);
                        if (editorManager->GetAnimationEditor()) {
                            editorManager->GetAnimationEditor()->SetSelectedTargets(nullptr, editorManager->GetSelectedGameObject(), nullptr);
                            editorManager->GetAnimationEditor()->RefreshAnimationJointList(sceneManager);
                        }
                    }
                }
            });
        }
    }
    ImGui::End();
}
#endif
