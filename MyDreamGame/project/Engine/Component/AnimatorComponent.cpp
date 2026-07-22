#include "AnimatorComponent.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Renderer/SkeletonDebugRenderer.h"
#include "TransformComponent.h"
#include "GameObject/GameObject.h"
#include "Core/TimeManager.h"
#include <cmath>

void AnimatorComponent::Initialize() {
    animationTime_ = 0.0f;
}

void AnimatorComponent::Update() {
    if (!isPlaying_ || animation_.duration <= 0.0f) return;

    animationTime_ += TimeManager::GetInstance().GetDeltaTime();
    animationTime_ = std::fmod(animationTime_, animation_.duration); // Loop playback

    if (hasSkeleton_) {
        // Skeletonベースのアニメーション
        ApplyAnimation(skeleton_, animation_, animationTime_);
        ::Update(skeleton_); // 骨格空間のローカル・ワールド行列を計算
        ::Update(skinCluster_, skeleton_); // 今回はGPUスキニングは保留
    } else {
        // 単一ノード（旧方式）のアニメーション
        if (animation_.nodeAnimations.find(targetNodeName_) != animation_.nodeAnimations.end()) {
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


#include "Renderer/DirectXCommon/DirectXCommon.h"

void AnimatorComponent::SetModelData(const ModelData& modelData) {
    skeleton_ = CreateSkeleton(modelData.rootNode);
    hasSkeleton_ = true;
    debugRenderer_ = std::make_unique<SkeletonDebugRenderer>();
    debugRenderer_->Initialize();
    
    skinCluster_ = CreateSkinCluster(DirectXCommon::GetInstance()->GetDevice(), skeleton_, modelData);
}

void AnimatorComponent::DrawDebug(const Matrix4x4& worldMatrix) {
    if (hasSkeleton_ && debugRenderer_) {
        debugRenderer_->Draw(skeleton_, worldMatrix);
    }
}
