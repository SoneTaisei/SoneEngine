#ifdef USE_IMGUI
#include "EditorInspector.h"
#include "EditorManager.h"
#include "EditorCommandManager.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Graphics/Camera/GameCamera.h"
#include "Graphics/Camera/DebugCamera.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Collision/CollisionManager.h"
#include "GameObject/GameObject.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "Effect/ParticleManager.h"
#include "Game2D/Player/Player2D.h"
#include "Editor/Animation/AnimationEditor.h"
#include "Editor/GPUParticle/GPUParticleEditor.h"
#include "Editor/Model3DEditor/Model3DEditor.h"
#include "Editor/MapEditor/MapEditor.h"
#include "Editor/MapEditor/MapEditorContext.h"
#include "Editor/LightEditor/LightEditor.h"
#include "Editor/Replay/ReplayManager.h"
#include "Editor/Replay/LevelEvolutionAI.h"
#include "EditorReplayUI.h"
#include "Core/Utility/Structs.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

void EditorInspector::Draw(bool* pOpen, EditorManager* em, SceneManager* sceneManager,
                           ModelCommon* modelCommon, GameCamera* gameCamera, DebugCamera* debugCamera) {
    if (!pOpen || !*pOpen || !em) return;

    if (ImGui::Begin("インスペクター", pOpen)) {
        int mapTool = em->mapEditor_ ? em->mapEditor_->GetContext()->GetSelectedTool() : 0;
        bool isMapChipSelected = (mapTool >= 100 || (mapTool >= 1 && mapTool <= 12));
        bool isPlacedModelSelected = (em->model3DEditor_ && em->model3DEditor_->GetSelectedObject() != nullptr);

        if (em->currentMode_ == EditorManager::EditorMode::Animation) {
            if (em->forceShowGlobalSettings_) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.35f, 1.0f));
                if (ImGui::Button("[ アニメーション設定に戻る ]", ImVec2(-1, 28))) em->forceShowGlobalSettings_ = false;
                ImGui::PopStyleColor();
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.28f, 0.38f, 1.0f));
                if (ImGui::Button("グローバル設定を表示 (ゲーム全体)", ImVec2(-1, 26))) em->forceShowGlobalSettings_ = true;
                ImGui::PopStyleColor();
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            }
        } else {
            bool isMapChipSel = (mapTool >= 1 && mapTool != 6 && mapTool != 10);
            if (em->selectedGameObject_ || em->selectedObject_ || em->selectedParticle_ || em->selectedPrimitive_ ||
                isMapChipSel || em->selectedReplayBlock_.IsValid() || isPlacedModelSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                if (ImGui::Button("グローバル設定を表示", ImVec2(-1, 0))) {
                    em->selectedGameObject_ = nullptr; em->selectedObject_ = nullptr;
                    em->selectedParticle_ = nullptr; em->selectedPrimitive_ = nullptr;
                    em->selectedReplayBlock_.Clear(); em->selectedReplaySeekbar_ = false;
                    if (em->model3DEditor_) em->model3DEditor_->SetSelectedObject(nullptr);
                    if (em->mapEditor_) em->mapEditor_->GetContext()->SetSelectedTool(0);
                }
                ImGui::PopStyleColor();
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            }
        }

        if (em->currentMode_ == EditorManager::EditorMode::Animation && !em->forceShowGlobalSettings_) {
            if (em->animationEditor_) {
                em->animationEditor_->SetSelectedTargets(em->selectedObject_, em->selectedGameObject_, em->selectedPrimitive_);
                em->animationEditor_->DrawInspectorUI(sceneManager);
            }
        } else if (em->currentMode_ == EditorManager::EditorMode::GPUParticle && !em->forceShowGlobalSettings_) {
            if (em->gpuParticleEditor_) { em->gpuParticleEditor_->DrawInspectorUI(sceneManager); }
        } else if (em->selectedReplayBlock_.IsValid() && !em->forceShowGlobalSettings_) {
            DrawReplayNodeProperties(em);
        } else if (em->model3DEditor_ && em->model3DEditor_->GetSelectedObject() && !em->forceShowGlobalSettings_) {
            em->model3DEditor_->DrawInspectorUI(sceneManager);
        } else if (em->selectedGameObject_ && !em->forceShowGlobalSettings_) {
            em->selectedGameObject_->DisplayImGui();
        } else if (em->selectedObject_ && !em->forceShowGlobalSettings_) {
            em->selectedObject_->DisplayImGui("Object Properties");
        } else if (em->selectedParticle_ && !em->forceShowGlobalSettings_) {
            em->selectedParticle_->DrawImGui();
        } else if (em->isPlayerSelected_ && sceneManager && sceneManager->GetCurrentScene() &&
                   sceneManager->GetCurrentScene()->GetPlayer() && !em->forceShowGlobalSettings_) {
            sceneManager->GetCurrentScene()->GetPlayer()->DisplayImGui();
        } else if (em->selectedPrimitive_ && !em->forceShowGlobalSettings_) {
            em->selectedPrimitive_->DisplayImGui("Primitive Properties");
        } else {
            if (em->currentMode_ == EditorManager::EditorMode::Replay) {
                DrawReplayGlobalSettings(em, sceneManager);
            } else {
                bool handled = false;
                if (em->mapEditor_) {
                    handled = em->mapEditor_->DrawInspectorUI(sceneManager);
                }
                if (!handled) {
                    DrawGlobalSettings(em, modelCommon, gameCamera, debugCamera);
                }
            }
        }

        IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
        if (activeScene) {
            activeScene->DisplayImGui(em->selectedPrimitive_);
        }
    }
    ImGui::End();
}

void EditorInspector::DrawReplayNodeProperties(EditorManager* em) {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "キー入力ノード プロパティ");
    ImGui::Separator(); ImGui::Spacing();
    const char* trackNames[7] = { "左移動 (L)", "右移動 (R)", "上移動 (W)", "下移動 (S)", "ジャンプ (J)", "ダッシュ (D)", "壁張り付き (C)" };
    ImGui::Text("トラック: %s", trackNames[em->selectedReplayBlock_.trackIdx]);
    ImGui::Spacing();

    int startF = em->selectedReplayBlock_.startFrame, endF = em->selectedReplayBlock_.endFrame;
    int duration = endF - startF;
    float startSec = startF / 60.0f, durationSec = duration / 60.0f;
    static ReplayData inspectorOldData;
    static EditorManager::SelectedReplayBlock inspectorOldBlock;
    bool changed = false;

    auto CheckEdit = [&]() {
        if (ImGui::IsItemActivated()) { inspectorOldData = ReplayManager::GetInstance()->GetCurrentReplay(); inspectorOldBlock = em->selectedReplayBlock_; }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
            ReplayData oldD = inspectorOldData;
            EditorManager::SelectedReplayBlock newBlock = em->selectedReplayBlock_;
            EditorManager::SelectedReplayBlock oldB = inspectorOldBlock;
            em->PushActionCommand(
                [oldD, oldB, em]() { auto mgr = ReplayManager::GetInstance(); mgr->SetCurrentReplay(oldD); mgr->RebuildMmlFromCurrentFrames(); em->selectedReplayBlock_ = oldB; },
                [newData, newBlock, em]() { auto mgr = ReplayManager::GetInstance(); mgr->SetCurrentReplay(newData); mgr->RebuildMmlFromCurrentFrames(); em->selectedReplayBlock_ = newBlock; }
            );
        }
    };

    if (ImGui::DragInt("開始フレーム", &startF, 1.0f, 0, 99999)) { if (startF < 0) startF = 0; endF = startF + duration; changed = true; }
    CheckEdit(); ImGui::TextDisabled(" (開始時刻: %.2f 秒)", startSec);

    if (ImGui::DragInt("長さ (フレーム数)", &duration, 1.0f, 1, 99999)) { if (duration < 1) duration = 1; endF = startF + duration; changed = true; }
    CheckEdit(); ImGui::TextDisabled(" (継続時間: %.2f 秒)", durationSec);

    if (ImGui::DragInt("終了フレーム", &endF, 1.0f, startF + 1, 99999)) { if (endF <= startF) endF = startF + 1; duration = endF - startF; changed = true; }
    CheckEdit();

    if (changed) {
        ReplayManager::GetInstance()->ModifyBlockRange(em->selectedReplayBlock_.trackIdx, em->selectedReplayBlock_.startFrame, em->selectedReplayBlock_.endFrame, startF, endF);
        em->selectedReplayBlock_.startFrame = startF; em->selectedReplayBlock_.endFrame = endF;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    auto replayMgrInstance = ReplayManager::GetInstance();
    ReplayData curReplay = replayMgrInstance->GetCurrentReplay();
    auto& currentJitters = curReplay.jitters;
    int currentJitterVal = 0;
    for (const auto& j : currentJitters) {
        if (j.keyIdx == em->selectedReplayBlock_.trackIdx && j.startFrame == em->selectedReplayBlock_.startFrame) { currentJitterVal = j.maxJitter; break; }
    }
    if (ImGui::DragInt("手振れ/Jitter強度 (±F)", &currentJitterVal, 1.0f, 0, 30)) {
        if (currentJitterVal < 0) currentJitterVal = 0;
        bool found = false;
        for (auto& j : currentJitters) {
            if (j.keyIdx == em->selectedReplayBlock_.trackIdx && j.startFrame == em->selectedReplayBlock_.startFrame) { j.maxJitter = currentJitterVal; found = true; break; }
        }
        if (!found && currentJitterVal > 0) {
            currentJitters.push_back({ em->selectedReplayBlock_.trackIdx, em->selectedReplayBlock_.startFrame, em->selectedReplayBlock_.endFrame, currentJitterVal });
        }
        replayMgrInstance->SetCurrentReplay(curReplay);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生時に設定フレーム数の範囲内でキー入力をブレ（揺らぎ）させる強度を設定します");

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("再生位置をここに合わせる", ImVec2(-1, 28))) { ReplayManager::GetInstance()->SetCurrentFrame(em->selectedReplayBlock_.startFrame); }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("ノードを削除", ImVec2(-1, 28))) {
        ReplayData oldData = ReplayManager::GetInstance()->GetCurrentReplay();
        auto oldBlock = em->selectedReplayBlock_;
        ReplayManager::GetInstance()->DeleteBlockRange(em->selectedReplayBlock_.trackIdx, em->selectedReplayBlock_.startFrame, em->selectedReplayBlock_.endFrame);
        em->selectedReplayBlock_.Clear();
        ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
        auto newBlock = em->selectedReplayBlock_;
        em->PushActionCommand(
            [oldData, oldBlock, em]() { auto mgr = ReplayManager::GetInstance(); mgr->SetCurrentReplay(oldData); mgr->RebuildMmlFromCurrentFrames(); em->selectedReplayBlock_ = oldBlock; },
            [newData, newBlock, em]() { auto mgr = ReplayManager::GetInstance(); mgr->SetCurrentReplay(newData); mgr->RebuildMmlFromCurrentFrames(); em->selectedReplayBlock_ = newBlock; }
        );
    }
    ImGui::PopStyleColor(2);
}

void EditorInspector::DrawReplayGlobalSettings(EditorManager* em, SceneManager* sceneManager) {
    ImGui::TextColored(ImVec4(0.55f, 0.4f, 1.0f, 1.0f), "リプレイ グローバル設定 (手振れ補正 & 高速デバッグ)");
    ImGui::Separator(); ImGui::Spacing();
    auto replayMgrInst = ReplayManager::GetInstance();
    if (ImGui::CollapsingHeader("再生・補正設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool isSnap = replayMgrInst->IsSnapEnabled(), isLoop = replayMgrInst->IsLoopPlay(), isInterp = replayMgrInst->IsInterpolationEnabled();
        if (ImGui::Checkbox("位置補正 (手振れ補正/スナップ)", &isSnap)) { replayMgrInst->SetSnapEnabled(isSnap); }
        if (ImGui::Checkbox("ループ再生", &isLoop)) { replayMgrInst->SetLoopPlay(isLoop); }
        if (ImGui::Checkbox("座標補間 (スムーズ再生)", &isInterp)) { replayMgrInst->SetInterpolationEnabled(isInterp); }
    }

    if (ImGui::CollapsingHeader("完全決定論デバッグ & 高速自動モンキーテスト", ImGuiTreeNodeFlags_DefaultOpen)) {
        int seed = static_cast<int>(replayMgrInst->GetRandomSeed());
        if (ImGui::DragInt("乱数シード (Seed)", &seed, 1.0f, 0, 999999)) { replayMgrInst->SetRandomSeed(static_cast<uint32_t>((std::max)(0, seed))); }
        static int testIterations = 20, testJitterChance = 5;
        ImGui::DragInt("テスト試行回数", &testIterations, 1.0f, 1, 500);
        ImGui::SliderInt("Jitter発生確率 (%)", &testJitterChance, 0, 50);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
        if (ImGui::Button("高速自動モンキーテストを実行", ImVec2(-1, 30))) {
            IScene* s = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
            replayMgrInst->ExecuteFastMonkeyTest(s ? s->GetMapChip() : nullptr, testIterations, testJitterChance);
        }
        ImGui::PopStyleColor();
        const auto& logs = replayMgrInst->GetMonkeyTestLogs();
        if (!logs.empty()) {
            ImGui::Spacing(); ImGui::Text("テスト実行ログ (%d 件)", (int)logs.size());
            ImGui::BeginChild("MonkeyTestLogArea", ImVec2(0, 120), true);
            for (const auto& logLine : logs) {
                if (logLine.find("[BUG") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", logLine.c_str());
                else if (logLine.find("[SUCCESS]") != std::string::npos) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", logLine.c_str());
                else ImGui::TextUnformatted(logLine.c_str());
            }
            ImGui::EndChild();
            if (ImGui::Button("ログをクリア")) { replayMgrInst->ClearMonkeyTestLogs(); }
        }
    }

    if (ImGui::CollapsingHeader("ステージ難易度自動スコアリング", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("リプレイ難易度を解析", ImVec2(-1, 28))) {
            IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
            replayMgrInst->AnalyzeReplayDifficulty(replayMgrInst->GetCurrentReplay().frames, activeScene ? activeScene->GetMapChip() : nullptr);
        }
        const auto& score = replayMgrInst->GetLastAnalyzedScore();
        ImGui::Text("操作速度: %.1f | 精度: %.1f | 停滞: %.2f秒", score.averageAPM, score.maxPrecisionScore, score.stagnationDuration);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "総合推定難易度スコア: %.1f", score.finalCalculatedDifficulty);
    }

    if (ImGui::CollapsingHeader("ギミック自動進化AI (遺伝的アルゴリズム)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float targetDifficulty = 50.0f; static int generations = 20;
        ImGui::SliderFloat("目標難易度", &targetDifficulty, 10.0f, 100.0f, "%.1f");
        ImGui::SliderInt("進化世代数", &generations, 5, 100);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.7f, 1.0f));
        if (ImGui::Button("自動進化を実行", ImVec2(-1, 30))) {
            auto* evAI = replayMgrInst->GetLevelEvolutionAI();
            if (evAI) {
                IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
                evAI->RunEvolution(activeScene ? activeScene->GetMapChip() : nullptr, replayMgrInst, targetDifficulty, generations);
            }
        }
        ImGui::PopStyleColor();
        auto* evAI = replayMgrInst->GetLevelEvolutionAI();
        if (evAI && !evAI->GetEvolutionLogs().empty()) {
            ImGui::Spacing(); ImGui::Text("進化実行ログ");
            ImGui::BeginChild("EvolutionLogArea", ImVec2(0, 120), true);
            for (const auto& logLine : evAI->GetEvolutionLogs()) {
                if (logLine.find("[SUCCESS]") != std::string::npos) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", logLine.c_str());
                else if (logLine.find("[WARN]") != std::string::npos || logLine.find("[ERROR]") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", logLine.c_str());
                else ImGui::TextUnformatted(logLine.c_str());
            }
            ImGui::EndChild();
            if (ImGui::Button("ログをクリア##GA")) { evAI->ClearLogs(); }
        }
    }

    if (ImGui::CollapsingHeader("物理ベースA* (詰みチェック & AIルート表示)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int maxAStarNodes = 30000;
        ImGui::DragFloat2("スタート座標 (X, Y)", em->aStarStartPos_, 0.5f);
        if (ImGui::Button("マップのスタート位置から自動取得", ImVec2(-1, 0))) {
            IScene* s = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
            if (s && s->GetMapChip()) em->UpdateAStarPositionsFromMap(s->GetMapChip(), sceneManager);
        }
        ImGui::Spacing();
        ImGui::DragFloat2("ゴール座標 (X, Y)", em->aStarGoalPos_, 0.5f);
        if (ImGui::Button("マップのゴールブロック位置から自動取得", ImVec2(-1, 0))) {
            IScene* s = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
            if (s && s->GetMapChip()) em->UpdateAStarPositionsFromMap(s->GetMapChip(), sceneManager);
        }
        ImGui::Spacing();
        ImGui::DragInt("探索上限ノード数", &maxAStarNodes, 1000, 1000, 150000);
        bool showAI = replayMgrInst->IsShowAIGhost();
        if (ImGui::Checkbox("AIルートをゴースト表示", &showAI)) { replayMgrInst->SetShowAIGhost(showAI); }

        bool isSearching = replayMgrInst->IsAISearching();
        if (isSearching) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        std::string btnText = isSearching ? "AI探索中... (バックグラウンド計算中)" : "物理A* AI探索を実行 (詰みチェック)";
        if (ImGui::Button(btnText.c_str(), ImVec2(-1, 30))) {
            IScene* s = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
            MapChip2D* mapChip = s ? s->GetMapChip() : nullptr;
            Vector3 sPos = { em->aStarStartPos_[0], em->aStarStartPos_[1], 0.0f }, gPos = { em->aStarGoalPos_[0], em->aStarGoalPos_[1], 0.0f };
            PlayerParams params{};
            if (s && s->GetPlayer()) { params = s->GetPlayer()->GetParams(); }
            replayMgrInst->ExecuteAStarAsync(sPos, gPos, mapChip, params, maxAStarNodes);
        }
        ImGui::PopStyleColor();
        if (isSearching) ImGui::EndDisabled();

        const auto& statusMsg = replayMgrInst->GetAIPathStatusMsg();
        if (!statusMsg.empty()) {
            ImGui::Spacing();
            if (statusMsg.find("[詰み]") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", statusMsg.c_str());
            else ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", statusMsg.c_str());
        }
    }
}

void EditorInspector::DrawGlobalSettings(EditorManager* em, ModelCommon* modelCommon,
                                        GameCamera* gameCamera, DebugCamera* debugCamera) {
    ImGui::Text("グローバル表示設定");
    ImGui::Separator();
    ImGui::Checkbox("オブジェクトを表示 (モデル)", &em->showObjects_);
    ImGui::Checkbox("エフェクトを表示 (パーティクル/プリミティブ)", &em->showEffects_);
    {
        bool showColliders = CollisionManager::GetInstance()->IsDebugDrawEnabled(), oldShow = showColliders;
        if (ImGui::Checkbox("当たり判定のワイヤー表示 (コライダー)", &showColliders)) {
            CollisionManager::GetInstance()->SetDebugDrawEnabled(showColliders); em->SaveSceneConfig();
            em->PushActionCommand([=](){ CollisionManager::GetInstance()->SetDebugDrawEnabled(oldShow); em->SaveSceneConfig(); },
                                  [=](){ CollisionManager::GetInstance()->SetDebugDrawEnabled(showColliders); em->SaveSceneConfig(); });
        }
        if (showColliders) {
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("形状タイプごとの色分け:\n・緑: AABB (矩形)\n・オレンジ: OBB (回転ボックス)\n・水色: Sphere (球)");
            ImGui::Indent();
            bool depthTest = CollisionManager::GetInstance()->IsDepthTestEnabled(), oldDepth = depthTest;
            if (ImGui::Checkbox("遮蔽 (ブロックの奥で隠す)", &depthTest)) {
                CollisionManager::GetInstance()->SetDepthTestEnabled(depthTest); em->SaveSceneConfig();
                em->PushActionCommand([=](){ CollisionManager::GetInstance()->SetDepthTestEnabled(oldDepth); em->SaveSceneConfig(); },
                                      [=](){ CollisionManager::GetInstance()->SetDepthTestEnabled(depthTest); em->SaveSceneConfig(); });
            }
            ImGui::Unindent();
        }
    }
    ImGui::Spacing(); ImGui::Text("アウトライン設定"); ImGui::Separator();
    {
        auto dxCommon = DirectXCommon::GetInstance();
        if (dxCommon) {
            bool postOutline = dxCommon->IsDepthBasedOutlineEnabled(), oldPost = postOutline;
            if (ImGui::Checkbox("ポストエフェクト・アウトライン (深度)", &postOutline)) {
                dxCommon->SetDepthBasedOutlineEnabled(postOutline); em->SaveSceneConfig();
                em->PushActionCommand([=](){ dxCommon->SetDepthBasedOutlineEnabled(oldPost); em->SaveSceneConfig(); }, 
                                      [=](){ dxCommon->SetDepthBasedOutlineEnabled(postOutline); em->SaveSceneConfig(); });
            }
            bool outline = dxCommon->IsOutlineEnabled(), oldOut = outline;
            if (ImGui::Checkbox("メッシュ・アウトライン (ワイヤー/押し出し)", &outline)) {
                dxCommon->SetOutlineEnabled(outline); em->SaveSceneConfig();
                em->PushActionCommand([=](){ dxCommon->SetOutlineEnabled(oldOut); em->SaveSceneConfig(); }, 
                                      [=](){ dxCommon->SetOutlineEnabled(outline); em->SaveSceneConfig(); });
            }
        }
    }
    ImGui::Spacing(); ImGui::Text("グローバル設定 (ライティング)"); ImGui::Separator();
    if (em->lightEditor_) {
        ImGui::Text("現在の設定: %s | 環境光: %.2f | スポットライト: %zu / %d",
                    em->lightEditor_->GetCurrentFileName().c_str(), em->lightEditor_->GetAmbientIntensity(),
                    em->lightEditor_->GetSpotLights().size(), kMaxSpotLights);
        if (ImGui::Button("ライトエディターを開く", ImVec2(-1, 30))) {
            em->showLightEditor_ = true; em->showSpotLightPanel_ = true; em->activeMainTab_ = "ライトエディター";
            em->currentMode_ = EditorManager::EditorMode::Light; em->forceShowGlobalSettings_ = false;
            em->focusActiveTabCountdown_ = 5; em->focusSpotLightTabCountdown_ = 5; em->SaveSceneConfig();
        }
    }
    ImGui::Separator();
    bool oldFog = em->enableFog_;
    if (ImGui::Checkbox("フォグエフェクトを有効化", &em->enableFog_)) {
        em->SaveLightingConfig(modelCommon); bool newFog = em->enableFog_;
        em->PushActionCommand([=](){ em->enableFog_ = oldFog; em->SaveLightingConfig(modelCommon); }, 
                              [=](){ em->enableFog_ = newFog; em->SaveLightingConfig(modelCommon); });
    }
    ImGui::Spacing();
    DrawCameraSettings(em, gameCamera, debugCamera);
}

void EditorInspector::DrawCameraSettings(EditorManager* em, GameCamera* gameCamera, DebugCamera* debugCamera) {
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "カメラ設定 (デバッグカメラ)");
    ImGui::Separator();
    if (debugCamera) {
        if (em->activeMainTab_ == "マップチップ画面") {
            ImGui::TextDisabled("※現在『マップチップ画面』のため2Dマップ用カメラが表示されています。");
        }
        ImGui::Checkbox("デバッグカメラを使用 (F3)", &em->useDebugCamera_);
        ImGui::SameLine();
        if (em->useDebugCamera_) { ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[アクティブ]"); }
        else { ImGui::TextDisabled("[非アクティブ]"); }

        Vector3 dbgPos = debugCamera->GetTranslation();
        float dbgPosArr[3] = { dbgPos.x, dbgPos.y, dbgPos.z };
        if (ImGui::DragFloat3("カメラ座標 (Position)##DebugCam", dbgPosArr, 0.1f, -10000.0f, 10000.0f, "%.2f")) {
            debugCamera->SetTranslation({ dbgPosArr[0], dbgPosArr[1], dbgPosArr[2] });
            debugCamera->UpdateMatrix();
            em->useDebugCamera_ = true;
        }

        Vector3 dbgRot = debugCamera->GetRotation();
        float dbgRotDeg[3] = { dbgRot.x * 180.0f / 3.14159265f, dbgRot.y * 180.0f / 3.14159265f, dbgRot.z * 180.0f / 3.14159265f };
        if (ImGui::DragFloat3("カメラ角度 (Rotation)##DebugCam", dbgRotDeg, 0.5f, -180.0f, 180.0f, "%.1f")) {
            debugCamera->SetRotation({ dbgRotDeg[0] * 3.14159265f / 180.0f, dbgRotDeg[1] * 3.14159265f / 180.0f, dbgRotDeg[2] * 3.14159265f / 180.0f });
            debugCamera->UpdateMatrix();
            em->useDebugCamera_ = true;
        }

        if (gameCamera && ImGui::Button("ゲームカメラの位置に同期##DebugCam", ImVec2(-1, 0))) {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
            debugCamera->UpdateMatrix();
        }
    } else {
        ImGui::TextDisabled("※デバッグカメラが有効ではありません。");
    }

    ImGui::Spacing();
    ImGui::Text("グローバル設定 (ゲームカメラ: 2Dゲーム用)");
    ImGui::Separator();
    if (gameCamera) {
        bool followEnabled = gameCamera->IsFollowEnabled();
        if (ImGui::Checkbox("ゲームカメラのターゲット追従", &followEnabled)) { gameCamera->SetFollowEnabled(followEnabled); }
        ImGui::SameLine();
        if (followEnabled) { ImGui::TextDisabled("(追従中)"); }
        else { ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(手動固定モード)"); }

        auto TrackFloat = [&](const char* label, float* v, float spd, float min, float max, const char* fmt, auto setter) {
            static float oldVal;
            if (ImGui::DragFloat(label, v, spd, min, max, fmt)) { setter(*v); }
            if (ImGui::IsItemActivated()) oldVal = *v;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                float nVal = *v;
                em->PushActionCommand([=](){ setter(oldVal); }, [=](){ setter(nVal); });
            }
        };

        Vector3 pos = gameCamera->GetTranslation();
        float posArr[3] = { pos.x, pos.y, pos.z };
        static Vector3 oldPos;
        if (ImGui::DragFloat3("ゲームカメラ座標 (Position)##GameCam", posArr, 0.1f, -10000.0f, 10000.0f, "%.2f")) {
            pos = { posArr[0], posArr[1], posArr[2] };
            gameCamera->SetTranslation(pos);
            gameCamera->SetFollowEnabled(false);
            gameCamera->UpdateMatrix();
        }
        if (ImGui::IsItemActivated()) oldPos = pos;
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Vector3 newPos = { posArr[0], posArr[1], posArr[2] };
            em->PushActionCommand([=](){ gameCamera->SetTranslation(oldPos); gameCamera->UpdateMatrix(); },
                                  [=](){ gameCamera->SetTranslation(newPos); gameCamera->UpdateMatrix(); });
        }

        Vector3 offset = gameCamera->GetFollowOffset();
        float offsetArr[2] = { offset.x, offset.y };
        if (ImGui::DragFloat2("追従オフセット (Offset X/Y)##GameCam", offsetArr, 0.1f, -1000.0f, 1000.0f, "%.2f")) {
            gameCamera->SetFollowOffset({ offsetArr[0], offsetArr[1], 0.0f });
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) { gameCamera->SaveConfig(); }

        float scale = gameCamera->GetScale();
        TrackFloat("カメラスケール (Zoom)##GameCam", &scale, 0.01f, 0.1f, 10.0f, "%.2f", [=](float v){
            gameCamera->SetScale(v); gameCamera->SaveConfig(); em->SaveSceneConfig();
        });

        Vector3 rot = gameCamera->GetRotation();
        float rotDeg[3] = { rot.x * 180.0f / 3.14159265f, rot.y * 180.0f / 3.14159265f, rot.z * 180.0f / 3.14159265f };
        static Vector3 oldRot;
        if (ImGui::DragFloat3("カメラ角度 (Rotation)##GameCam", rotDeg, 0.5f, -180.0f, 180.0f, "%.1f")) {
            rot = { rotDeg[0] * 3.14159265f / 180.0f, rotDeg[1] * 3.14159265f / 180.0f, rotDeg[2] * 3.14159265f / 180.0f };
            gameCamera->SetRotation(rot); gameCamera->SetBaseRotation(rot);
        }
        if (ImGui::IsItemActivated()) oldRot = rot;
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Vector3 newRot = rot;
            em->PushActionCommand([=](){ gameCamera->SetRotation(oldRot); gameCamera->SetBaseRotation(oldRot); gameCamera->SaveConfig(); em->SaveSceneConfig(); },
                                  [=](){ gameCamera->SetRotation(newRot); gameCamera->SetBaseRotation(newRot); gameCamera->SaveConfig(); em->SaveSceneConfig(); });
        }

        float follow = gameCamera->GetFollowLerp();
        TrackFloat("追従速度 (FollowLerp)##GameCam", &follow, 0.005f, 0.0f, 1.0f, "%.3f", [=](float v){ gameCamera->SetFollowLerp(v); gameCamera->SaveConfig(); em->SaveSceneConfig(); });
        float trans = gameCamera->GetTransitionLerp();
        TrackFloat("遷移速度 (TransitionLerp)##GameCam", &trans, 0.005f, 0.0f, 1.0f, "%.3f", [=](float v){ gameCamera->SetTransitionLerp(v); gameCamera->SaveConfig(); em->SaveSceneConfig(); });

        ImGui::Spacing();
        if (ImGui::Button("カメラ設定をJSON保存 (Save)", ImVec2(-1, 0))) { gameCamera->SaveConfig(); }
        if (ImGui::Button("カメラ設定をJSON読込 (Load)", ImVec2(-1, 0))) { gameCamera->LoadConfig(); }
    } else {
        ImGui::TextDisabled("※ゲームカメラが有効ではありません。");
    }
}
#endif
