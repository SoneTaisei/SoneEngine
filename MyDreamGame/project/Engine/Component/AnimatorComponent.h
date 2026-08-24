#pragma once
#include "IComponent.h"
#include "Core/Utility/Animation.h"
#include "Core/Utility/Structs.h"
#include "Renderer/SkeletonDebugRenderer.h"
#include <memory>
#include <string>

struct JointOverride {
    Quaternion rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 translate = { 0.0f, 0.0f, 0.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    bool hasRotate = false;
    bool hasTranslate = false;
    bool hasScale = false;
    float weight = 1.0f;
};

class AnimatorComponent : public IComponent {
public:
    void Initialize() override;
    void Update() override;

    void SetAnimation(const Animation& animation) { animation_ = animation; }
    void Play() { isPlaying_ = true; }
    void Stop() { isPlaying_ = false; }
    void SetTime(float time) { animationTime_ = time; }
    void SetTargetNodeName(const std::string& name) { targetNodeName_ = name; }
    void SetJointRotationOverride(const std::string& name, const std::optional<Quaternion>& rotation, float weight = 1.0f) {
        if (rotation) {
            auto& ov = jointOverrides_[name];
            ov.rotate = *rotation;
            ov.hasRotate = true;
            ov.weight = weight;
        } else {
            auto it = jointOverrides_.find(name);
            if (it != jointOverrides_.end()) {
                it->second.hasRotate = false;
                if (!it->second.hasTranslate && !it->second.hasScale) {
                    jointOverrides_.erase(it);
                }
            }
        }
    }
    void SetJointTranslationOverride(const std::string& name, const std::optional<Vector3>& translation, float weight = 1.0f) {
        if (translation) {
            auto& ov = jointOverrides_[name];
            ov.translate = *translation;
            ov.hasTranslate = true;
            ov.weight = weight;
        } else {
            auto it = jointOverrides_.find(name);
            if (it != jointOverrides_.end()) {
                it->second.hasTranslate = false;
                if (!it->second.hasRotate && !it->second.hasScale) {
                    jointOverrides_.erase(it);
                }
            }
        }
    }
    void SetJointScaleOverride(const std::string& name, const std::optional<Vector3>& scale, float weight = 1.0f) {
        if (scale) {
            auto& ov = jointOverrides_[name];
            ov.scale = *scale;
            ov.hasScale = true;
            ov.weight = weight;
        } else {
            auto it = jointOverrides_.find(name);
            if (it != jointOverrides_.end()) {
                it->second.hasScale = false;
                if (!it->second.hasRotate && !it->second.hasTranslate) {
                    jointOverrides_.erase(it);
                }
            }
        }
    }
    void ClearJointOverrides() { jointOverrides_.clear(); }

    void SetModelData(const ModelData& modelData);
    void UpdateSkeletonAndSkinCluster();
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
    std::map<std::string, JointOverride> jointOverrides_;
public:
    bool HasSkeleton() const { return hasSkeleton_; }
    std::unique_ptr<SkeletonDebugRenderer> debugRenderer_;

};
