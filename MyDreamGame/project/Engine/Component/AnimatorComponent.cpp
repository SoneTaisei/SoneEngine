#include "AnimatorComponent.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/Quaternion.h"
#include "Renderer/SkeletonDebugRenderer.h"
#include "TransformComponent.h"
#include "GameObject/GameObject.h"
#include "Core/TimeManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Editor/Replay/ReplayManager.h"
#include "Renderer/SrvManager.h"
#include "Core/Utility/LogManager.h"
#include <cmath>
#include <iostream>

void AnimatorComponent::Initialize() {
    animationTime_ = 0.0f;
    isFinished_ = false;
}

AnimatorComponent::~AnimatorComponent() {
    if (skinCluster_.paletteSrvHandle.first.ptr != 0) {
        SrvManager::GetInstance()->Free(skinCluster_.paletteSrvHandle.first, skinCluster_.paletteSrvHandle.second);
        skinCluster_.paletteSrvHandle.first = {};
        skinCluster_.paletteSrvHandle.second = {};
    }
}

void AnimatorComponent::CrossFade(const Animation& targetAnim, float duration) {
    if (!hasSkeleton_ || skeleton_.joints.empty() || duration <= 0.0f) {
        SetAnimation(targetAnim);
        SetTime(0.0f);
        isTransitioning_ = false;
        return;
    }

    // 現在のスケルトンの各ジョイント姿勢をスナップショットとして保存
    transitionFromPose_.resize(skeleton_.joints.size());
    for (size_t i = 0; i < skeleton_.joints.size(); ++i) {
        transitionFromPose_[i].translate = skeleton_.joints[i].transform.translate;
        transitionFromPose_[i].rotate = skeleton_.joints[i].transform.rotate;
        transitionFromPose_[i].scale = skeleton_.joints[i].transform.scale;
    }

    animation_ = targetAnim;
    animationTime_ = 0.0f;
    isFinished_ = false;
    isPlaying_ = true;

    transitionDuration_ = duration;
    transitionTimer_ = 0.0f;
    isTransitioning_ = true;
}

void AnimatorComponent::Update() {
    bool isPlayingOrReplaying = false;
#ifdef USE_IMGUI
    if (EditorManager::IsPlaying()) {
        isPlayingOrReplaying = true;
    }
#else
    isPlayingOrReplaying = true;
#endif
    if (ReplayManager::GetInstance()->IsPlaying()) {
        isPlayingOrReplaying = true;
    }

    bool isAnimActive = isPlayingOrReplaying && !ReplayManager::GetInstance()->IsPaused();
    float animDeltaTime = isAnimActive ? TimeManager::GetInstance().GetDeltaTime() : 0.0f;

    if (isPlaying_ && animation_.duration > 0.0f) {
        bool finished = false;
        animationTime_ = AdvanceAnimationTime(animationTime_, animation_.duration, animDeltaTime, wrapMode_, &finished);
        if (finished && wrapMode_ != AnimationWrapMode::Loop) {
            isFinished_ = true;
            isPlaying_ = false;
        }
    }

    if (isTransitioning_) {
        transitionTimer_ += animDeltaTime;
        if (transitionTimer_ >= transitionDuration_) {
            isTransitioning_ = false;
        }
    }

    if (hasSkeleton_) {
        UpdateSkeletonAndSkinCluster();
    } else {
        // 単一ノード（旧方式）のアニメーション
        if (animation_.duration > 0.0f && animation_.nodeAnimations.find(targetNodeName_) != animation_.nodeAnimations.end()) {
            const NodeAnimation& nodeAnim = animation_.nodeAnimations.at(targetNodeName_);

            Vector3 currentTranslate = nodeAnim.translate.empty() ? Vector3{0.0f, 0.0f, 0.0f} : CalculateValue(nodeAnim.translate, animationTime_);
            Quaternion currentRotate = nodeAnim.rotate.empty() ? Quaternion{0.0f, 0.0f, 0.0f, 1.0f} : CalculateValue(nodeAnim.rotate, animationTime_);
            Vector3 currentScale = nodeAnim.scale.empty() ? Vector3{1.0f, 1.0f, 1.0f} : CalculateValue(nodeAnim.scale, animationTime_);

            // GameObject has TransformComponent
            if (gameObject_) {
                auto transform = gameObject_->GetComponent<TransformComponent>();
                if (transform) {
                    transform->SetPosition(currentTranslate);
                    transform->SetRotation(currentRotate.ToEulerAngles());
                    transform->SetScale(currentScale);
                }
            }
        }
    }
}

void AnimatorComponent::UpdateSkeletonAndSkinCluster() {
    if (!hasSkeleton_) return;

    // アニメーション適用前に、全ジョイントの回転・位置・スケールを初期ポーズ（バインドポーズ）にリセット
    for (Joint& joint : skeleton_.joints) {
        joint.transform = joint.defaultTransform;
    }

    // Skeletonベースのアニメーションが設定されている場合は適用
    if (animation_.duration > 0.0f) {
        ApplyAnimation(skeleton_, animation_, animationTime_);
    }

    // クロスフェード遷移中なら、直前のスナップショットポーズと現在の新アニメーションポーズをブレンド
    if (isTransitioning_ && transitionDuration_ > 0.0f && !transitionFromPose_.empty()) {
        float rawT = std::clamp(transitionTimer_ / transitionDuration_, 0.0f, 1.0f);
        // smoothstep で滑らかな加減速補間
        float t = rawT * rawT * (3.0f - 2.0f * rawT);
        for (size_t i = 0; i < skeleton_.joints.size() && i < transitionFromPose_.size(); ++i) {
            skeleton_.joints[i].transform.translate = TransformFunctions::Lerp(
                transitionFromPose_[i].translate,
                skeleton_.joints[i].transform.translate,
                t
            );
            skeleton_.joints[i].transform.rotate = Slerp(
                transitionFromPose_[i].rotate,
                skeleton_.joints[i].transform.rotate,
                t
            );
            skeleton_.joints[i].transform.scale = TransformFunctions::Lerp(
                transitionFromPose_[i].scale,
                skeleton_.joints[i].transform.scale,
                t
            );
        }
    }

    // ジョイントのSRTオーバーライドを適用
    for (const auto& [name, overrideInfo] : jointOverrides_) {
        auto it = skeleton_.jointMap.find(name);
        if (it != skeleton_.jointMap.end()) {
            Joint& joint = skeleton_.joints[it->second];
            if (overrideInfo.hasRotate) {
                if (overrideInfo.weight >= 1.0f) {
                    joint.transform.rotate = overrideInfo.rotate;
                } else if (overrideInfo.weight > 0.0f) {
                    joint.transform.rotate = Slerp(
                        joint.transform.rotate,
                        overrideInfo.rotate,
                        overrideInfo.weight
                    );
                }
            }
            if (overrideInfo.hasTranslate) {
                if (overrideInfo.weight >= 1.0f) {
                    joint.transform.translate = overrideInfo.translate;
                } else if (overrideInfo.weight > 0.0f) {
                    joint.transform.translate = TransformFunctions::Lerp(
                        joint.transform.translate,
                        overrideInfo.translate,
                        overrideInfo.weight
                    );
                }
            }
            if (overrideInfo.hasScale) {
                if (overrideInfo.weight >= 1.0f) {
                    joint.transform.scale = overrideInfo.scale;
                } else if (overrideInfo.weight > 0.0f) {
                    joint.transform.scale = TransformFunctions::Lerp(
                        joint.transform.scale,
                        overrideInfo.scale,
                        overrideInfo.weight
                    );
                }
            }
        } else {
#ifdef _DEBUG
            OutputDebugStringA(("Joint not found for override: " + name + "\n").c_str());
#endif
        }
    }

    ::Update(skeleton_); // 骨格空間のローカル・ワールド行列を計算
    ::Update(skinCluster_, skeleton_); // スキニングパレット行列をGPU用バッファに書き込み
}


#include "Renderer/DirectXCommon/DirectXCommon.h"

void AnimatorComponent::SetModelData(const ModelData& modelData) {
    skeleton_ = CreateSkeleton(modelData.rootNode);
    hasSkeleton_ = true;

    // スケルトンの全ジョイント名を出力
    LogManager::GetInstance()->AddLog(LogLevel::Info, "===== Skeleton Joints List =====");
    for (const auto& joint : skeleton_.joints) {
        std::string logLine = "Joint Index: " + std::to_string(joint.index) + ", Name: " + joint.name;
        LogManager::GetInstance()->AddLog(LogLevel::Info, logLine);
    }

    debugRenderer_ = std::make_unique<SkeletonDebugRenderer>();
    debugRenderer_->Initialize();
    
    if (skinCluster_.paletteSrvHandle.first.ptr != 0) {
        SrvManager::GetInstance()->Free(skinCluster_.paletteSrvHandle.first, skinCluster_.paletteSrvHandle.second);
        skinCluster_.paletteSrvHandle.first = {};
        skinCluster_.paletteSrvHandle.second = {};
    }

    skinCluster_ = CreateSkinCluster(DirectXCommon::GetInstance()->GetDevice(), skeleton_, modelData);

    // 初期化直後にバインドポーズのスキニングパレットをGPUバッファに書き込む
    UpdateSkeletonAndSkinCluster();
}

void AnimatorComponent::DrawDebug(const Matrix4x4& worldMatrix) {
    if (hasSkeleton_ && debugRenderer_) {
        debugRenderer_->Draw(skeleton_, worldMatrix);
    }
}
