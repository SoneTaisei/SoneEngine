#ifdef USE_IMGUI
#include "AnimationEditorContext.h"
#include "Editor/EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Effect/ParticleManager.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "GameObject/GameObject.h"
#include "Input/KeyboardInput.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/SrvManager.h"
#include "Scene/IScene.h"
#include "Scene/SceneManager.h"
#include "Scenes/AnimationPreviewScene.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/LogManager.h"
#include "Component/TransformComponent.h"
#include "Component/AnimatorComponent.h"
#include "Core/Utility/Animation.h"
#include "Resource/Model/Model.h"
#include "Game2D/Player/Player2D.h"

#include <imgui.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

AnimationEditorContext::AnimationEditorContext() {
    Initialize();
}

void AnimationEditorContext::Initialize() {
}

AnimatorComponent* AnimationEditorContext::GetTargetAnimator(SceneManager* sceneManager) {
    if (selectedGameObject_) {
        auto* a = selectedGameObject_->GetComponent<AnimatorComponent>();
        if (a) return a;
    }
    if (selectedObject_) {
        auto* a = selectedObject_->GetAnimator();
        if (a) return a;
    }
    if (sceneManager && sceneManager->GetCurrentScene()) {
        auto* scene = sceneManager->GetCurrentScene();
        for (auto& go : scene->GetGameObjects()) {
            if (go) {
                auto* a = go->GetComponent<AnimatorComponent>();
                if (a && a->HasSkeleton()) return a;
            }
        }
        for (auto* obj : scene->GetObjects()) {
            if (obj && obj->GetAnimator() && obj->GetAnimator()->HasSkeleton()) {
                return obj->GetAnimator();
            }
        }
        auto* player = scene->GetPlayer();
        if (player && player->GetAnimator()) {
            return player->GetAnimator();
        }
    }
    return nullptr;
}

void AnimationEditorContext::RefreshAnimationJointList(SceneManager* sceneManager) {
    animJointTreeNodes_.clear();
    animJointRootIndices_.clear();
    currentJointList_.clear();
    
    // 1. シーン内のスケルトンを持つAnimatorを検索
    AnimatorComponent* animator = GetTargetAnimator(sceneManager);
    
    if (animator && animator->HasSkeleton()) {
        const auto& joints = animator->GetSkeleton().joints;
        int32_t numJoints = static_cast<int32_t>(joints.size());
        animJointTreeNodes_.resize(numJoints);

        for (int32_t i = 0; i < numJoints; ++i) {
            const auto& j = joints[i];
            currentJointList_.push_back(j.name);

            animJointTreeNodes_[i].name = j.name;
            animJointTreeNodes_[i].jointIndex = j.index;
            animJointTreeNodes_[i].parentIndex = j.parent.has_value() ? *j.parent : -1;
            animJointTreeNodes_[i].children = j.children;
            animJointTreeNodes_[i].depth = 0;
        }

        // 親を持たないジョイント（ルート候補）を収集
        for (int32_t i = 0; i < numJoints; ++i) {
            if (animJointTreeNodes_[i].parentIndex < 0) {
                animJointRootIndices_.push_back(i);
            }
        }
        if (animJointRootIndices_.empty() && animator->GetSkeleton().root >= 0 && animator->GetSkeleton().root < numJoints) {
            animJointRootIndices_.push_back(animator->GetSkeleton().root);
        } else if (animJointRootIndices_.empty() && numJoints > 0) {
            animJointRootIndices_.push_back(0);
        }

        // 各ノードの深さ (depth) を再帰的に計算
        std::function<void(int32_t, int)> calcDepth = [&](int32_t nodeIdx, int d) {
            if (nodeIdx < 0 || nodeIdx >= numJoints) return;
            animJointTreeNodes_[nodeIdx].depth = d;
            for (int32_t childIdx : animJointTreeNodes_[nodeIdx].children) {
                calcDepth(childIdx, d + 1);
            }
        };
        for (int32_t rootIdx : animJointRootIndices_) {
            calcDepth(rootIdx, 0);
        }

        // 開閉フラグの初期化（未設定のノードについて設定: ルートのみ展開し、子は閉じる）
        for (int32_t i = 0; i < numJoints; ++i) {
            const std::string& name = animJointTreeNodes_[i].name;
            if (animJointExpanded_.find(name) == animJointExpanded_.end()) {
                bool isRoot = (animJointTreeNodes_[i].parentIndex < 0);
                animJointExpanded_[name] = isRoot;
            }
        }
    }
    
    // フォールバック（デフォルトジョイント）
    if (currentJointList_.empty()) {
        static const char* defaultJoints[] = {
            "Hips_01",
            "LeftArm_09",
            "RightArm_014",
            "LeftForeArm_010",
            "RightForeArm_015",
            "LeftUpLeg_019",
            "RightUpLeg_024",
            "LeftLeg_020",
            "RightLeg_025",
            "LeftFoot_021",
            "RightFoot_026"
        };
        for (int i = 0; i < 11; ++i) {
            std::string name = defaultJoints[i];
            currentJointList_.push_back(name);

            AnimJointTreeNode node;
            node.name = name;
            node.jointIndex = i;
            node.parentIndex = (i == 0) ? -1 : 0;
            node.depth = (i == 0) ? 0 : 1;
            if (i == 0) {
                for (int c = 1; c < 11; ++c) node.children.push_back(c);
                animJointRootIndices_.push_back(0);
            }
            animJointTreeNodes_.push_back(node);
            if (animJointExpanded_.find(name) == animJointExpanded_.end()) {
                animJointExpanded_[name] = (i == 0);
            }
        }
    }
    
    // 選択中ジョイント名がリストにない場合はリストの先頭にする
    bool found = false;
    for (const auto& name : currentJointList_) {
        if (name == animEditorSelectedJointName_) {
            found = true;
            break;
        }
    }
    if (!found && !currentJointList_.empty()) {
        animEditorSelectedJointName_ = currentJointList_[0];
    }
}

void AnimationEditorContext::ScanAnimationFiles() {
    availableAnimationFiles_.clear();
    const std::string animDir = "resources/json/shared/Player";
    std::filesystem::create_directories(animDir);

    // 既知の標準プリセットファイルが存在しなければ作成
    std::string wallClimbPath = animDir + "/wall_climb_animation.json";
    std::string airDashPath = animDir + "/air_dash_animation.json";

    if (!std::filesystem::exists(wallClimbPath)) {
        SaveAnimationToJsonFile(CreateDefaultWallClimbAnimation(), wallClimbPath);
    }
    if (!std::filesystem::exists(airDashPath)) {
        SaveAnimationToJsonFile(CreateDefaultAirDashAnimation(), airDashPath);
    }

    // ディレクトリ内のすべての.jsonファイルを列挙
    for (const auto& entry : std::filesystem::directory_iterator(animDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string filename = entry.path().filename().string();
            if (filename == "player_parameters.json") continue;

            std::string fullPath = entry.path().string();
            std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
            availableAnimationFiles_.push_back(fullPath);
        }
    }

    // ソート
    std::sort(availableAnimationFiles_.begin(), availableAnimationFiles_.end());

    // 現在のファイルパスがリストにない場合は先頭に設定
    if (std::find(availableAnimationFiles_.begin(), availableAnimationFiles_.end(), currentAnimFilePath_) == availableAnimationFiles_.end()) {
        if (!availableAnimationFiles_.empty()) {
            currentAnimFilePath_ = availableAnimationFiles_[0];
        } else {
            currentAnimFilePath_ = wallClimbPath;
            availableAnimationFiles_.push_back(wallClimbPath);
        }
    }
}

void AnimationEditorContext::UpdateAnimationPosePreview(SceneManager* sceneManager) {
    if (!sceneManager || !sceneManager->GetCurrentScene()) return;
    
    if (!animEditorInitialized_) {
        ScanAnimationFiles();
        if (!LoadAnimationFromJsonFile(editingAnimation_, currentAnimFilePath_)) {
            if (currentAnimFilePath_.find("wall_climb") != std::string::npos) {
                editingAnimation_ = CreateDefaultWallClimbAnimation();
            } else if (currentAnimFilePath_.find("air_dash") != std::string::npos) {
                editingAnimation_ = CreateDefaultAirDashAnimation();
            }
        }
        animEditorInitialized_ = true;
    }

    AnimatorComponent* animator = GetTargetAnimator(sceneManager);
    if (!animator) return;
    
    // 再生中の時間更新
    if (animEditorPlaying_) {
        animTempOverrides_.clear();
        float dt = TimeManager::GetInstance().GetDeltaTime();
        animEditorTime_ += dt;
        if (editingAnimation_.duration > 0.0f) {
            if (animEditorTime_ >= editingAnimation_.duration) {
                if (animEditorLoop_) {
                    animEditorTime_ = std::fmod(animEditorTime_, editingAnimation_.duration);
                } else {
                    animEditorTime_ = editingAnimation_.duration;
                    animEditorPlaying_ = false;
                }
            }
        }
    }
    
    // アニメーションをモデルに適用（アニメーションモード時または編集中）
    if (showAnimEditor_ || EditorManager::IsPlaying()) {
        animator->ClearJointOverrides();
        for (const auto& [nodeName, nodeAnim] : editingAnimation_.nodeAnimations) {
            if (!nodeAnim.rotate.empty()) {
                Quaternion rot = CalculateValue(nodeAnim.rotate, animEditorTime_);
                animator->SetJointRotationOverride(nodeName, rot, 1.0f);
            }
            if (!nodeAnim.translate.empty()) {
                Vector3 trans = CalculateValue(nodeAnim.translate, animEditorTime_);
                animator->SetJointTranslationOverride(nodeName, trans, 1.0f);
            }
            if (!nodeAnim.scale.empty()) {
                Vector3 sc = CalculateValue(nodeAnim.scale, animEditorTime_);
                animator->SetJointScaleOverride(nodeName, sc, 1.0f);
            }
        }
        // 未挿入時の一時プレビュー値を上書き反映（キー未登録でも画面上で動く）
        for (const auto& [nodeName, ov] : animTempOverrides_) {
            if (ov.rotate) animator->SetJointRotationOverride(nodeName, *ov.rotate, 1.0f);
            if (ov.translate) animator->SetJointTranslationOverride(nodeName, *ov.translate, 1.0f);
            if (ov.scale) animator->SetJointScaleOverride(nodeName, *ov.scale, 1.0f);
        }
        animator->UpdateSkeletonAndSkinCluster();
    }
}

void AnimationEditorContext::PushAnimUndoState(const std::string& desc) {
    AnimEditorSnapshot snap;
    snap.animation = editingAnimation_;
    snap.time = animEditorTime_;
    snap.selectedJointName = animEditorSelectedJointName_;
    snap.selectedKeyIndex = animEditorSelectedKeyIndex_;
    snap.tempOverrides = animTempOverrides_;
    snap.description = desc;

    animUndoStack_.push_back(snap);
    if (animUndoStack_.size() > 64) {
        animUndoStack_.erase(animUndoStack_.begin());
    }
    animRedoStack_.clear();
}

void AnimationEditorContext::BeginDragSnapshot(const std::string& desc) {
    animDragPreSnapshot_.animation = editingAnimation_;
    animDragPreSnapshot_.time = animEditorTime_;
    animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
    animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
    animDragPreSnapshot_.tempOverrides = animTempOverrides_;
    animDragPreSnapshot_.description = desc;
    hasAnimDragPreSnapshot_ = true;
}

void AnimationEditorContext::PerformAnimUndo(SceneManager* sceneManager) {
    if (animUndoStack_.empty()) return;

    // 現在の状態をRedoスタックにプッシュ
    AnimEditorSnapshot curSnap;
    curSnap.animation = editingAnimation_;
    curSnap.time = animEditorTime_;
    curSnap.selectedJointName = animEditorSelectedJointName_;
    curSnap.selectedKeyIndex = animEditorSelectedKeyIndex_;
    curSnap.tempOverrides = animTempOverrides_;
    curSnap.description = "Current";
    animRedoStack_.push_back(curSnap);

    // Undoスタックから最新のスナップショットを取り出して適用
    AnimEditorSnapshot prevSnap = animUndoStack_.back();
    animUndoStack_.pop_back();

    editingAnimation_ = prevSnap.animation;
    animEditorTime_ = prevSnap.time;
    animEditorSelectedJointName_ = prevSnap.selectedJointName;
    animEditorSelectedKeyIndex_ = prevSnap.selectedKeyIndex;
    animTempOverrides_ = prevSnap.tempOverrides; // ★一時ポーズも含めて完全復元

    UpdateAnimationPosePreview(sceneManager);
}

void AnimationEditorContext::PerformAnimRedo(SceneManager* sceneManager) {
    if (animRedoStack_.empty()) return;

    // 現在の状態をUndoスタックにプッシュ
    AnimEditorSnapshot curSnap;
    curSnap.animation = editingAnimation_;
    curSnap.time = animEditorTime_;
    curSnap.selectedJointName = animEditorSelectedJointName_;
    curSnap.selectedKeyIndex = animEditorSelectedKeyIndex_;
    curSnap.tempOverrides = animTempOverrides_;
    curSnap.description = "Current";
    animUndoStack_.push_back(curSnap);

    // Redoスタックから最新のスナップショットを取り出して適用
    AnimEditorSnapshot nextSnap = animRedoStack_.back();
    animRedoStack_.pop_back();

    editingAnimation_ = nextSnap.animation;
    animEditorTime_ = nextSnap.time;
    animEditorSelectedJointName_ = nextSnap.selectedJointName;
    animEditorSelectedKeyIndex_ = nextSnap.selectedKeyIndex;
    animTempOverrides_ = nextSnap.tempOverrides; // ★一時ポーズも含めて完全復元

    UpdateAnimationPosePreview(sceneManager);
}

void AnimationEditorContext::ClearAnimUndoRedo() {
    animUndoStack_.clear();
    animRedoStack_.clear();
    hasAnimDragPreSnapshot_ = false;
}


std::string AnimationEditorContext::FindOppositeJointName(const std::string& jointName, bool axisX, bool axisY, bool axisZ, const Skeleton* skeleton) {
    if (jointName.empty()) return "";

    // 0. ユーザー手動指定マッピングがあれば最優先で適用
    auto itCustom = customSymmetryMap_.find(jointName);
    if (itCustom != customSymmetryMap_.end() && !itCustom->second.empty()) {
        return itCustom->second;
    }

    if (!axisX && !axisY && !axisZ) return "";

    auto startsWith = [](const std::string& str, const std::string& prefix) -> bool {
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    };

    // 1. 親子関係の階層構造（Skeleton）を用いた探索
    if (skeleton && !skeleton->joints.empty()) {
        auto itJ = skeleton->jointMap.find(jointName);
        if (itJ != skeleton->jointMap.end()) {
            int32_t curIdx = itJ->second;

            // ルートまでの祖先パス（親インデックスのリスト）を構築: [curIdx, parent, grandParent, ..., root]
            std::vector<int32_t> path;
            int32_t temp = curIdx;
            while (temp >= 0 && temp < static_cast<int32_t>(skeleton->joints.size())) {
                path.push_back(temp);
                const auto& j = skeleton->joints[temp];
                if (!j.parent.has_value() || j.parent.value() < 0 || j.parent.value() >= static_cast<int32_t>(skeleton->joints.size())) {
                    break;
                }
                temp = j.parent.value();
            }

            // パスを親方向へ遡り、2つ以上の子を持つ分岐祖先（Branching Ancestor）を探す
            for (size_t p = 1; p < path.size(); ++p) {
                int32_t ancestorIdx = path[p];
                int32_t childOnPath = path[p - 1]; // 祖先の子で、現在のジョイントを含む枝
                const auto& ancestorJoint = skeleton->joints[ancestorIdx];

                if (ancestorJoint.children.size() >= 2) {
                    // 他の子（別の枝）の中から、対称となる枝 branchB を探す
                    int32_t bestOppChild = -1;
                    float bestScore = -1e9f;

                    const auto& jointA = skeleton->joints[childOnPath];
                    Vector3 posA = { jointA.skeletonSpaceMatrix.m[3][0], jointA.skeletonSpaceMatrix.m[3][1], jointA.skeletonSpaceMatrix.m[3][2] };

                    for (int32_t cIdx : ancestorJoint.children) {
                        if (cIdx == childOnPath) continue;
                        if (cIdx < 0 || cIdx >= static_cast<int32_t>(skeleton->joints.size())) continue;

                        const auto& jointB = skeleton->joints[cIdx];
                        Vector3 posB = { jointB.skeletonSpaceMatrix.m[3][0], jointB.skeletonSpaceMatrix.m[3][1], jointB.skeletonSpaceMatrix.m[3][2] };

                        float score = 0.0f;
                        // 座標の対称性スコア
                        float diff = 0.0f;
                        if (axisX) diff += std::abs(posB.x + posA.x);
                        else       diff += std::abs(posB.x - posA.x);

                        if (axisY) diff += std::abs(posB.y + posA.y);
                        else       diff += std::abs(posB.y - posA.y);

                        if (axisZ) diff += std::abs(posB.z + posA.z);
                        else       diff += std::abs(posB.z - posA.z);

                        score -= diff * 10.0f;

                        // 名前のLeft/Right等 対称性ボーナス
                        if (axisX) {
                            if ((jointA.name.find("Left") != std::string::npos && jointB.name.find("Right") != std::string::npos) ||
                                (jointA.name.find("Right") != std::string::npos && jointB.name.find("Left") != std::string::npos) ||
                                (jointA.name.find("_L") != std::string::npos && jointB.name.find("_R") != std::string::npos) ||
                                (jointA.name.find("_R") != std::string::npos && jointB.name.find("_L") != std::string::npos) ||
                                (jointA.name.find(".L") != std::string::npos && jointB.name.find(".R") != std::string::npos) ||
                                (jointA.name.find(".R") != std::string::npos && jointB.name.find(".L") != std::string::npos) ||
                                (jointA.name.find("left") != std::string::npos && jointB.name.find("right") != std::string::npos) ||
                                (jointA.name.find("right") != std::string::npos && jointB.name.find("left") != std::string::npos)) {
                                score += 100.0f;
                            }
                        }
                        if (axisY) {
                            if ((jointA.name.find("Up") != std::string::npos && jointB.name.find("Down") != std::string::npos) ||
                                (jointA.name.find("Down") != std::string::npos && jointB.name.find("Up") != std::string::npos) ||
                                (jointA.name.find("Top") != std::string::npos && jointB.name.find("Bottom") != std::string::npos) ||
                                (jointA.name.find("Bottom") != std::string::npos && jointB.name.find("Top") != std::string::npos)) {
                                score += 100.0f;
                            }
                        }
                        if (axisZ) {
                            if ((jointA.name.find("Front") != std::string::npos && jointB.name.find("Back") != std::string::npos) ||
                                (jointA.name.find("Back") != std::string::npos && jointB.name.find("Front") != std::string::npos) ||
                                (jointA.name.find("Forward") != std::string::npos && jointB.name.find("Backward") != std::string::npos) ||
                                (jointA.name.find("Backward") != std::string::npos && jointB.name.find("Forward") != std::string::npos)) {
                                score += 100.0f;
                            }
                        }

                        if (score > bestScore) {
                            bestScore = score;
                            bestOppChild = cIdx;
                        }
                    }

                    if (bestOppChild >= 0) {
                        // childOnPath から curIdx までの下降ステップ（階層深さと各階層での子インデックス）を収集
                        std::vector<int> stepIndices;
                        for (int k = static_cast<int>(p) - 1; k >= 1; --k) {
                            int32_t parentJ = path[k];
                            int32_t childJ = path[k - 1];
                            const auto& pj = skeleton->joints[parentJ];
                            int childIndexInParent = 0;
                            for (size_t ci = 0; ci < pj.children.size(); ++ci) {
                                if (pj.children[ci] == childJ) {
                                    childIndexInParent = static_cast<int>(ci);
                                    break;
                                }
                            }
                            stepIndices.push_back(childIndexInParent);
                        }

                        // bestOppChild から同じステップを下降して辿る
                        int32_t oppCursor = bestOppChild;
                        bool walkSuccess = true;
                        for (int stepIdx : stepIndices) {
                            const auto& curOppJ = skeleton->joints[oppCursor];
                            if (curOppJ.children.empty()) {
                                walkSuccess = false;
                                break;
                            }
                            if (stepIdx < static_cast<int>(curOppJ.children.size())) {
                                oppCursor = curOppJ.children[stepIdx];
                            } else {
                                oppCursor = curOppJ.children[0];
                            }
                        }

                        if (walkSuccess && oppCursor >= 0 && oppCursor < static_cast<int>(skeleton->joints.size())) {
                            return skeleton->joints[oppCursor].name;
                        }
                    }
                }
            }
        }
    }

    // 2. 階層から見つからなかった場合のフォールバック（文字列置換パターン）
    if (!currentJointList_.empty()) {
        if (axisX) { // X軸対称 (左右: Left <-> Right)
            std::vector<std::pair<std::string, std::string>> patterns = {
                { "Left", "Right" }, { "left", "right" }, { "LEFT", "RIGHT" },
                { "_L", "_R" }, { "_l", "_r" },
                { ".L", ".R" }, { ".l", ".r" },
                { "L_", "R_" }, { "l_", "r_" }
            };

            for (const auto& [pL, pR] : patterns) {
                auto posL = jointName.find(pL);
                if (posL != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posL, pL.length(), pR);
                    for (const auto& j : currentJointList_) {
                        if (j == targetFull) return j;
                    }
                    std::string prefix = jointName.substr(0, posL) + pR;
                    for (const auto& j : currentJointList_) {
                        if (startsWith(j, prefix) && j != jointName) return j;
                    }
                }

                auto posR = jointName.find(pR);
                if (posR != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posR, pR.length(), pL);
                    for (const auto& j : currentJointList_) {
                        if (j == targetFull) return j;
                    }
                    std::string prefix = jointName.substr(0, posR) + pL;
                    for (const auto& j : currentJointList_) {
                        if (startsWith(j, prefix) && j != jointName) return j;
                    }
                }
            }
        }
        if (axisY) { // Y軸対称 (上下: Up <-> Down, Top <-> Bottom)
            std::vector<std::pair<std::string, std::string>> patterns = {
                { "Up", "Down" }, { "up", "down" }, { "UP", "DOWN" },
                { "Top", "Bottom" }, { "top", "bottom" },
                { "Upper", "Lower" }, { "upper", "lower" }
            };
            for (const auto& [pU, pD] : patterns) {
                auto posU = jointName.find(pU);
                if (posU != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posU, pU.length(), pD);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posU) + pD;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
                auto posD = jointName.find(pD);
                if (posD != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posD, pD.length(), pU);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posD) + pU;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
            }
        }
        if (axisZ) { // Z軸対称 (前後: Front <-> Back)
            std::vector<std::pair<std::string, std::string>> patterns = {
                { "Front", "Back" }, { "front", "back" }, { "FRONT", "BACK" },
                { "Forward", "Backward" }, { "forward", "backward" }
            };
            for (const auto& [pF, pB] : patterns) {
                auto posF = jointName.find(pF);
                if (posF != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posF, pF.length(), pB);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posF) + pB;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
                auto posB = jointName.find(pB);
                if (posB != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posB, pB.length(), pF);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posB) + pF;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
            }
        }
    }

    return "";
}

void AnimationEditorContext::InsertSelectedJointSRTKey(SceneManager* sceneManager) {
    if (animEditorSelectedJointName_.empty()) return;

    PushAnimUndoState("選択ボーンSRTキー挿入");

    AnimatorComponent* anim = GetTargetAnimator(sceneManager);
    const Skeleton* skel = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

    Quaternion curQ = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 curT = { 0.0f, 0.0f, 0.0f };
    Vector3 curS = { 1.0f, 1.0f, 1.0f };

    if (skel) {
        auto itJ = skel->jointMap.find(animEditorSelectedJointName_);
        if (itJ != skel->jointMap.end()) {
            curQ = skel->joints[itJ->second].transform.rotate;
            curT = skel->joints[itJ->second].transform.translate;
            curS = skel->joints[itJ->second].transform.scale;
        }
    }

    NodeAnimation& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];
    if (!nodeAnim.rotate.empty()) curQ = CalculateValue(nodeAnim.rotate, animEditorTime_);
    if (!nodeAnim.translate.empty()) curT = CalculateValue(nodeAnim.translate, animEditorTime_);
    if (!nodeAnim.scale.empty()) curS = CalculateValue(nodeAnim.scale, animEditorTime_);

    auto itTemp = animTempOverrides_.find(animEditorSelectedJointName_);
    if (itTemp != animTempOverrides_.end()) {
        if (itTemp->second.translate) curT = *itTemp->second.translate;
        if (itTemp->second.rotate) curQ = *itTemp->second.rotate;
        if (itTemp->second.scale) curS = *itTemp->second.scale;
    }

    // Translation
    bool foundT = false;
    for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
        if (std::abs(nodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
            nodeAnim.translate[idx].value = curT;
            foundT = true;
            break;
        }
    }
    if (!foundT) {
        KeyframeVector3 newKf{ animEditorTime_, curT };
        auto itK = nodeAnim.translate.begin();
        while (itK != nodeAnim.translate.end() && itK->time < newKf.time) ++itK;
        nodeAnim.translate.insert(itK, newKf);
    }

    // Rotation
    bool foundR = false;
    for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
        if (std::abs(nodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
            nodeAnim.rotate[idx].value = curQ;
            animEditorSelectedKeyIndex_ = static_cast<int>(idx);
            foundR = true;
            break;
        }
    }
    if (!foundR) {
        KeyframeQuaternion newKf{ animEditorTime_, curQ };
        auto itK = nodeAnim.rotate.begin();
        while (itK != nodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
        auto ins = nodeAnim.rotate.insert(itK, newKf);
        animEditorSelectedKeyIndex_ = static_cast<int>(std::distance(nodeAnim.rotate.begin(), ins));
    }

    // Scale
    bool foundS = false;
    for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
        if (std::abs(nodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
            nodeAnim.scale[idx].value = curS;
            foundS = true;
            break;
        }
    }
    if (!foundS) {
        KeyframeVector3 newKf{ animEditorTime_, curS };
        auto itK = nodeAnim.scale.begin();
        while (itK != nodeAnim.scale.end() && itK->time < newKf.time) ++itK;
        nodeAnim.scale.insert(itK, newKf);
    }

    // 対称編集
    std::string oppJointName = FindOppositeJointName(animEditorSelectedJointName_, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, skel);
    bool hasAnySymmetryAxis = animSymmetryAxisX_ || animSymmetryAxisY_ || animSymmetryAxisZ_;
    if (animSymmetryMode_ && hasAnySymmetryAxis && !oppJointName.empty() && oppJointName != animEditorSelectedJointName_ && skel) {
        Vector3 oppS, oppT;
        Quaternion oppQ;
        if (ComputeBlenderSymmetrySRT(*skel, animEditorSelectedJointName_, oppJointName, curS, curQ, curT, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, oppS, oppQ, oppT)) {
            NodeAnimation& oppNodeAnim = editingAnimation_.nodeAnimations[oppJointName];
            // Translate
            bool foundOppT = false;
            for (size_t idx = 0; idx < oppNodeAnim.translate.size(); ++idx) {
                if (std::abs(oppNodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                    oppNodeAnim.translate[idx].value = oppT;
                    foundOppT = true;
                    break;
                }
            }
            if (!foundOppT) {
                KeyframeVector3 newKf{ animEditorTime_, oppT };
                auto itK = oppNodeAnim.translate.begin();
                while (itK != oppNodeAnim.translate.end() && itK->time < newKf.time) ++itK;
                oppNodeAnim.translate.insert(itK, newKf);
            }
            // Rotate
            bool foundOppR = false;
            for (size_t idx = 0; idx < oppNodeAnim.rotate.size(); ++idx) {
                if (std::abs(oppNodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                    oppNodeAnim.rotate[idx].value = oppQ;
                    foundOppR = true;
                    break;
                }
            }
            if (!foundOppR) {
                KeyframeQuaternion newKf{ animEditorTime_, oppQ };
                auto itK = oppNodeAnim.rotate.begin();
                while (itK != oppNodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
                oppNodeAnim.rotate.insert(itK, newKf);
            }
            // Scale
            bool foundOppS = false;
            for (size_t idx = 0; idx < oppNodeAnim.scale.size(); ++idx) {
                if (std::abs(oppNodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                    oppNodeAnim.scale[idx].value = oppS;
                    foundOppS = true;
                    break;
                }
            }
            if (!foundOppS) {
                KeyframeVector3 newKf{ animEditorTime_, oppS };
                auto itK = oppNodeAnim.scale.begin();
                while (itK != oppNodeAnim.scale.end() && itK->time < newKf.time) ++itK;
                oppNodeAnim.scale.insert(itK, newKf);
            }
            animTempOverrides_.erase(oppJointName);
        }
    }

    animTempOverrides_.erase(animEditorSelectedJointName_);
    UpdateAnimationPosePreview(sceneManager);
}

void AnimationEditorContext::InsertAllJointsSRTKey(SceneManager* sceneManager) {
    PushAnimUndoState("全ボーン全SRTキー挿入");

    AnimatorComponent* anim = GetTargetAnimator(sceneManager);
    const Skeleton* skel = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

    for (const auto& jName : currentJointList_) {
        Quaternion curQ = { 0.0f, 0.0f, 0.0f, 1.0f };
        Vector3 curT = { 0.0f, 0.0f, 0.0f };
        Vector3 curS = { 1.0f, 1.0f, 1.0f };

        if (skel) {
            auto itJ = skel->jointMap.find(jName);
            if (itJ != skel->jointMap.end()) {
                curQ = skel->joints[itJ->second].defaultTransform.rotate;
                curT = skel->joints[itJ->second].defaultTransform.translate;
                curS = skel->joints[itJ->second].defaultTransform.scale;
            }
        }

        NodeAnimation& nodeAnim = editingAnimation_.nodeAnimations[jName];
        if (!nodeAnim.rotate.empty()) curQ = CalculateValue(nodeAnim.rotate, animEditorTime_);
        if (!nodeAnim.translate.empty()) curT = CalculateValue(nodeAnim.translate, animEditorTime_);
        if (!nodeAnim.scale.empty()) curS = CalculateValue(nodeAnim.scale, animEditorTime_);

        // 一時オーバーライドがあればそれを最優先で適用
        auto itTemp = animTempOverrides_.find(jName);
        if (itTemp != animTempOverrides_.end()) {
            if (itTemp->second.translate) curT = *itTemp->second.translate;
            if (itTemp->second.rotate) curQ = *itTemp->second.rotate;
            if (itTemp->second.scale) curS = *itTemp->second.scale;
        }

        // Translation
        bool foundT = false;
        for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
            if (std::abs(nodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.translate[idx].value = curT;
                foundT = true;
                break;
            }
        }
        if (!foundT) {
            KeyframeVector3 newKf{ animEditorTime_, curT };
            auto itK = nodeAnim.translate.begin();
            while (itK != nodeAnim.translate.end() && itK->time < newKf.time) ++itK;
            nodeAnim.translate.insert(itK, newKf);
        }

        // Rotation
        bool foundR = false;
        for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
            if (std::abs(nodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.rotate[idx].value = curQ;
                foundR = true;
                break;
            }
        }
        if (!foundR) {
            KeyframeQuaternion newKf{ animEditorTime_, curQ };
            auto itK = nodeAnim.rotate.begin();
            while (itK != nodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
            nodeAnim.rotate.insert(itK, newKf);
        }

        // Scale
        bool foundS = false;
        for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
            if (std::abs(nodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.scale[idx].value = curS;
                foundS = true;
                break;
            }
        }
        if (!foundS) {
            KeyframeVector3 newKf{ animEditorTime_, curS };
            auto itK = nodeAnim.scale.begin();
            while (itK != nodeAnim.scale.end() && itK->time < newKf.time) ++itK;
            nodeAnim.scale.insert(itK, newKf);
        }
    }

    // 全一時オーバーライドを確定したのでクリア
    animTempOverrides_.clear();
    UpdateAnimationPosePreview(sceneManager);
}



#endif
