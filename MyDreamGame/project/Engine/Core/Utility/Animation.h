#pragma once

#include "Vector3.h"
#include "Quaternion.h"
#include <vector>
#include <map>
#include <string>

template <typename tValue>
struct Keyframe {
    float time;
    tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

struct NodeAnimation {
    std::vector<KeyframeVector3> translate;
    std::vector<KeyframeQuaternion> rotate;
    std::vector<KeyframeVector3> scale;
};

struct Animation {
    float duration; // アニメーション全体の尺（単位は秒）
    std::map<std::string, NodeAnimation> nodeAnimations;
};

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
