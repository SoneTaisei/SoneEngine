#include "Animation.h"
#include "TransformFunctions.h"
#include <cassert>

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return TransformFunctions::Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }
    return (*keyframes.rbegin()).value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    assert(!keyframes.empty());
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }
    return (*keyframes.rbegin()).value;
}
