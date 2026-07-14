#include "AnimatorComponent.h"
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

    if (animation_.nodeAnimations.find(targetNodeName_) != animation_.nodeAnimations.end()) {
        const NodeAnimation& nodeAnim = animation_.nodeAnimations.at(targetNodeName_);

        Vector3 currentTranslate = nodeAnim.translate.empty() ? Vector3{0.0f, 0.0f, 0.0f} : CalculateValue(nodeAnim.translate, animationTime_);
        Quaternion currentRotate = nodeAnim.rotate.empty() ? Quaternion{0.0f, 0.0f, 0.0f, 1.0f} : CalculateValue(nodeAnim.rotate, animationTime_);
        Vector3 currentScale = nodeAnim.scale.empty() ? Vector3{1.0f, 1.0f, 1.0f} : CalculateValue(nodeAnim.scale, animationTime_);

        // GameObject has TransformComponent
        if (gameObject_) {
            auto transform = gameObject_->GetComponent<TransformComponent>();
            if (transform) {
                // Apply animated transform
                // Assuming we want to override the base transform
                transform->SetPosition(currentTranslate);
                transform->SetRotation(currentRotate.ToEulerAngles());
                transform->SetScale(currentScale);
            }
        }
    }
}
