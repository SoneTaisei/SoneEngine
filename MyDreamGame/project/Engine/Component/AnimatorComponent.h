#pragma once
#include "IComponent.h"
#include "Core/Utility/Animation.h"
#include "Core/Utility/Structs.h"
#include "Renderer/SkeletonDebugRenderer.h"
#include <memory>
#include <string>

class AnimatorComponent : public IComponent {
public:
    void Initialize() override;
    void Update() override;

    void SetAnimation(const Animation& animation) { animation_ = animation; }
    void Play() { isPlaying_ = true; }
    void Stop() { isPlaying_ = false; }
    void SetTime(float time) { animationTime_ = time; }
    void SetTargetNodeName(const std::string& name) { targetNodeName_ = name; }
    void SetJointRotationOverride(const std::string& name, const std::optional<Quaternion>& rotation) {
        if (rotation) {
            jointOverrides_[name] = *rotation;
        } else {
            jointOverrides_.erase(name);
        }
    }
    void ClearJointOverrides() { jointOverrides_.clear(); }

    void SetModelData(const ModelData& modelData);
    void DrawDebug(const Matrix4x4& worldMatrix);

    const Skeleton& GetSkeleton() const { return skeleton_; }
    const SkinCluster& GetSkinCluster() const { return skinCluster_; }


private:
    Animation animation_;
    float animationTime_ = 0.0f;
    bool isPlaying_ = true;
    std::string targetNodeName_ = "Cube";

    Skeleton skeleton_;
    SkinCluster skinCluster_;
    bool hasSkeleton_ = false;
    std::map<std::string, Quaternion> jointOverrides_;
public:
    bool HasSkeleton() const { return hasSkeleton_; }
    std::unique_ptr<SkeletonDebugRenderer> debugRenderer_;

};
