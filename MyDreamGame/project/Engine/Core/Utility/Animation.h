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

inline Quaternion MakeRotHelper(const Vector3& axis, float angle) {
    float sinH = std::sin(angle / 2.0f);
    float cosH = std::cos(angle / 2.0f);
    return Quaternion{ axis.x * sinH, axis.y * sinH, axis.z * sinH, cosH };
}

inline Quaternion MakeEulerQuat(float x, float y, float z) {
    return MakeRotHelper(Vector3{ 1.0f, 0.0f, 0.0f }, x) *
           MakeRotHelper(Vector3{ 0.0f, 1.0f, 0.0f }, y) *
           MakeRotHelper(Vector3{ 0.0f, 0.0f, 1.0f }, z);
}

enum class AnimationWrapMode {
    Loop,          // ループ再生（末尾に達したら先頭に戻って繰り返す）
    Once,          // 1回再生（末尾に達したら先頭に戻って停止）
    HoldLastFrame  // 最後のフレームで停止（末尾に達したら最終フレームの姿勢を維持）
};

inline float AdvanceAnimationTime(float currentTime, float duration, float deltaTime, AnimationWrapMode wrapMode, bool* outFinished = nullptr) {
    if (outFinished) *outFinished = false;
    if (duration <= 0.0f) return 0.0f;

    float newTime = currentTime + deltaTime;
    switch (wrapMode) {
    case AnimationWrapMode::Loop:
        if (newTime >= duration) {
            newTime = std::fmod(newTime, duration);
            if (outFinished) *outFinished = true;
        }
        break;
    case AnimationWrapMode::Once:
        if (newTime >= duration) {
            newTime = 0.0f;
            if (outFinished) *outFinished = true;
        }
        break;
    case AnimationWrapMode::HoldLastFrame:
        if (newTime >= duration) {
            newTime = duration;
            if (outFinished) *outFinished = true;
        }
        break;
    }
    return newTime;
}

bool SaveAnimationToJsonFile(const Animation& animation, const std::string& filepath);
bool LoadAnimationFromJsonFile(Animation& outAnimation, const std::string& filepath);
Animation CreateDefaultWallClimbAnimation();
Animation CreateDefaultAirDashAnimation();
