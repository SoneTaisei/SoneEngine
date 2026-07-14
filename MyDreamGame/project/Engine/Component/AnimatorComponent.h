#pragma once
#include "IComponent.h"
#include "Core/Utility/Animation.h"
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

private:
    Animation animation_;
    float animationTime_ = 0.0f;
    bool isPlaying_ = true;
    std::string targetNodeName_ = "Cube"; // Default node name for AnimatedCube.gltf
};
