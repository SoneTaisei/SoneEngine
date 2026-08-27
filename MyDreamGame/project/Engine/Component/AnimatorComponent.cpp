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
#include <cmath>
#include <iostream>

void AnimatorComponent::Initialize() {
    animationTime_ = 0.0f;
    isFinished_ = false;
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

    // スケルトンの全ジョイント名を出力（Visual Studioの出力ウィンドウで確認できるようにする）
    OutputDebugStringA("===== Skeleton Joints List =====\n");
    for (const auto& joint : skeleton_.joints) {
        std::string logLine = "Joint Index: " + std::to_string(joint.index) + ", Name: " + joint.name + "\n";
        OutputDebugStringA(logLine.c_str());
    }
    OutputDebugStringA("================================\n");

    debugRenderer_ = std::make_unique<SkeletonDebugRenderer>();
    debugRenderer_->Initialize();
    
    skinCluster_ = CreateSkinCluster(DirectXCommon::GetInstance()->GetDevice(), skeleton_, modelData);

    // 初期化直後にバインドポーズのスキニングパレットをGPUバッファに書き込む
    UpdateSkeletonAndSkinCluster();
}

void AnimatorComponent::DrawDebug(const Matrix4x4& worldMatrix) {
    if (hasSkeleton_ && debugRenderer_) {
        debugRenderer_->Draw(skeleton_, worldMatrix);
    }
}
