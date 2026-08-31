#include "Animation.h"
#include "TransformFunctions.h"
#include <cassert>

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    if (keyframes.empty()) {
        return Vector3{ 0.0f, 0.0f, 0.0f };
    }
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float dt = keyframes[nextIndex].time - keyframes[index].time;
            if (dt > 1e-5f) {
                float t = (time - keyframes[index].time) / dt;
                return TransformFunctions::Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
            } else {
                return keyframes[index].value;
            }
        }
    }
    return (*keyframes.rbegin()).value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    if (keyframes.empty()) {
        return Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
    }
    if (keyframes.size() == 1 || time <= keyframes[0].time) {
        return keyframes[0].value;
    }

    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            float dt = keyframes[nextIndex].time - keyframes[index].time;
            if (dt > 1e-5f) {
                float t = (time - keyframes[index].time) / dt;
                return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
            } else {
                return keyframes[index].value;
            }
        }
    }
    return (*keyframes.rbegin()).value;
}

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

bool SaveAnimationToJsonFile(const Animation& animation, const std::string& filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    nlohmann::json j;
    j["duration"] = animation.duration;
    
    auto nodesJson = nlohmann::json::object();
    for (const auto& [nodeName, nodeAnim] : animation.nodeAnimations) {
        nlohmann::json n;
        
        if (!nodeAnim.translate.empty()) {
            auto tArr = nlohmann::json::array();
            for (const auto& k : nodeAnim.translate) {
                tArr.push_back({
                    {"time", k.time},
                    {"x", k.value.x},
                    {"y", k.value.y},
                    {"z", k.value.z}
                });
            }
            n["translate"] = tArr;
        }

        if (!nodeAnim.rotate.empty()) {
            auto rArr = nlohmann::json::array();
            for (const auto& k : nodeAnim.rotate) {
                Vector3 euler = k.value.ToEulerAngles();
                rArr.push_back({
                    {"time", k.time},
                    {"x", k.value.x},
                    {"y", k.value.y},
                    {"z", k.value.z},
                    {"w", k.value.w},
                    {"eulerX", euler.x},
                    {"eulerY", euler.y},
                    {"eulerZ", euler.z}
                });
            }
            n["rotate"] = rArr;
        }

        if (!nodeAnim.scale.empty()) {
            auto sArr = nlohmann::json::array();
            for (const auto& k : nodeAnim.scale) {
                sArr.push_back({
                    {"time", k.time},
                    {"x", k.value.x},
                    {"y", k.value.y},
                    {"z", k.value.z}
                });
            }
            n["scale"] = sArr;
        }

        nodesJson[nodeName] = n;
    }
    j["nodes"] = nodesJson;

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;
    ofs << j.dump(4);
    ofs.close();
    return true;
}

bool LoadAnimationFromJsonFile(Animation& outAnimation, const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    try {
        nlohmann::json j;
        ifs >> j;

        outAnimation.duration = j.value("duration", 1.0f);
        outAnimation.nodeAnimations.clear();

        if (j.contains("nodes") && j["nodes"].is_object()) {
            for (auto& [nodeName, n] : j["nodes"].items()) {
                NodeAnimation nodeAnim;

                if (n.contains("translate") && n["translate"].is_array()) {
                    for (const auto& k : n["translate"]) {
                        KeyframeVector3 kf;
                        kf.time = k.value("time", 0.0f);
                        kf.value = { k.value("x", 0.0f), k.value("y", 0.0f), k.value("z", 0.0f) };
                        nodeAnim.translate.push_back(kf);
                    }
                }

                if (n.contains("rotate") && n["rotate"].is_array()) {
                    for (const auto& k : n["rotate"]) {
                        KeyframeQuaternion kf;
                        kf.time = k.value("time", 0.0f);
                        if (k.contains("x") && k.contains("y") && k.contains("z") && k.contains("w")) {
                            kf.value = { k.value("x", 0.0f), k.value("y", 0.0f), k.value("z", 0.0f), k.value("w", 1.0f) };
                            float len = std::sqrt(kf.value.x * kf.value.x + kf.value.y * kf.value.y + kf.value.z * kf.value.z + kf.value.w * kf.value.w);
                            if (len > 1e-6f) {
                                kf.value.x /= len; kf.value.y /= len; kf.value.z /= len; kf.value.w /= len;
                            }
                        } else if (k.contains("eulerX") && k.contains("eulerY") && k.contains("eulerZ")) {
                            kf.value = MakeEulerQuat(k.value("eulerX", 0.0f), k.value("eulerY", 0.0f), k.value("eulerZ", 0.0f));
                        } else {
                            kf.value = { 0.0f, 0.0f, 0.0f, 1.0f };
                        }
                        nodeAnim.rotate.push_back(kf);
                    }
                }

                if (n.contains("scale") && n["scale"].is_array()) {
                    for (const auto& k : n["scale"]) {
                        KeyframeVector3 kf;
                        kf.time = k.value("time", 0.0f);
                        kf.value = { k.value("x", 1.0f), k.value("y", 1.0f), k.value("z", 1.0f) };
                        nodeAnim.scale.push_back(kf);
                    }
                }

                outAnimation.nodeAnimations[nodeName] = nodeAnim;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

Animation CreateDefaultWallClimbAnimation() {
    Animation anim;
    anim.duration = 0.6f;

    // Hips (上下・前後の揺れ)
    {
        NodeAnimation node;
        node.rotate.push_back({ 0.0f, MakeEulerQuat(0.0f, 0.0f, -0.05f) });
        node.rotate.push_back({ 0.3f, MakeEulerQuat(0.0f, 0.0f, 0.05f) });
        node.rotate.push_back({ 0.6f, MakeEulerQuat(0.0f, 0.0f, -0.05f) });
        anim.nodeAnimations["Hips_01"] = node;
    }

    // LeftArm (左腕の上り動作)
    {
        NodeAnimation node;
        node.rotate.push_back({ 0.0f, MakeEulerQuat(-0.40f, -3.14f, 0.35f) });
        node.rotate.push_back({ 0.3f, MakeEulerQuat(-0.10f, -3.14f, 0.15f) });
        node.rotate.push_back({ 0.6f, MakeEulerQuat(-0.40f, -3.14f, 0.35f) });
        anim.nodeAnimations["LeftArm_09"] = node;
    }

    // RightArm (右腕の上り動作 - 逆位相)
    {
        NodeAnimation node;
        node.rotate.push_back({ 0.0f, MakeEulerQuat(-0.10f, 3.14f, -0.15f) });
        node.rotate.push_back({ 0.3f, MakeEulerQuat(-0.40f, 3.14f, -0.35f) });
        node.rotate.push_back({ 0.6f, MakeEulerQuat(-0.10f, 3.14f, -0.15f) });
        anim.nodeAnimations["RightArm_014"] = node;
    }

    // ForeArms (肘の曲げ伸ばし)
    {
        NodeAnimation nodeL;
        nodeL.rotate.push_back({ 0.0f, MakeEulerQuat(-0.55f, 0.0f, 0.0f) });
        nodeL.rotate.push_back({ 0.3f, MakeEulerQuat(-0.20f, 0.0f, 0.0f) });
        nodeL.rotate.push_back({ 0.6f, MakeEulerQuat(-0.55f, 0.0f, 0.0f) });
        anim.nodeAnimations["LeftForeArm_010"] = nodeL;

        NodeAnimation nodeR;
        nodeR.rotate.push_back({ 0.0f, MakeEulerQuat(-0.20f, 0.0f, 0.0f) });
        nodeR.rotate.push_back({ 0.3f, MakeEulerQuat(-0.55f, 0.0f, 0.0f) });
        nodeR.rotate.push_back({ 0.6f, MakeEulerQuat(-0.20f, 0.0f, 0.0f) });
        anim.nodeAnimations["RightForeArm_015"] = nodeR;
    }

    // Legs (脚のよじ登り動作)
    {
        NodeAnimation nodeLUp;
        nodeLUp.rotate.push_back({ 0.0f, MakeEulerQuat(-1.3f, 0.0f, 0.0f) });
        nodeLUp.rotate.push_back({ 0.3f, MakeEulerQuat(-0.7f, 0.0f, 0.0f) });
        nodeLUp.rotate.push_back({ 0.6f, MakeEulerQuat(-1.3f, 0.0f, 0.0f) });
        anim.nodeAnimations["LeftUpLeg_019"] = nodeLUp;

        NodeAnimation nodeRUp;
        nodeRUp.rotate.push_back({ 0.0f, MakeEulerQuat(-0.7f, 0.0f, 0.0f) });
        nodeRUp.rotate.push_back({ 0.3f, MakeEulerQuat(-1.3f, 0.0f, 0.0f) });
        nodeRUp.rotate.push_back({ 0.6f, MakeEulerQuat(-0.7f, 0.0f, 0.0f) });
        anim.nodeAnimations["RightUpLeg_024"] = nodeRUp;

        NodeAnimation nodeLLeg;
        nodeLLeg.rotate.push_back({ 0.0f, MakeEulerQuat(1.4f, 0.0f, 0.0f) });
        nodeLLeg.rotate.push_back({ 0.3f, MakeEulerQuat(0.9f, 0.0f, 0.0f) });
        nodeLLeg.rotate.push_back({ 0.6f, MakeEulerQuat(1.4f, 0.0f, 0.0f) });
        anim.nodeAnimations["LeftLeg_020"] = nodeLLeg;

        NodeAnimation nodeRLeg;
        nodeRLeg.rotate.push_back({ 0.0f, MakeEulerQuat(0.9f, 0.0f, 0.0f) });
        nodeRLeg.rotate.push_back({ 0.3f, MakeEulerQuat(1.4f, 0.0f, 0.0f) });
        nodeRLeg.rotate.push_back({ 0.6f, MakeEulerQuat(0.9f, 0.0f, 0.0f) });
        anim.nodeAnimations["RightLeg_025"] = nodeRLeg;
    }

    return anim;
}

Animation CreateDefaultAirDashAnimation() {
    Animation anim;
    anim.duration = 0.35f;

    // Hips (体を前傾させ、水平に突進するポーズ)
    {
        NodeAnimation node;
        node.rotate.push_back({ 0.0f, MakeEulerQuat(0.60f, 0.0f, 0.0f) });
        node.rotate.push_back({ 0.15f, MakeEulerQuat(0.75f, 0.0f, 0.0f) });
        node.rotate.push_back({ 0.35f, MakeEulerQuat(0.50f, 0.0f, 0.0f) });
        anim.nodeAnimations["Hips_01"] = node;
    }

    // Arms (腕を後ろに引いて空気抵抗を減らすようなダイナミックな突進ポーズ)
    {
        NodeAnimation nodeL;
        nodeL.rotate.push_back({ 0.0f, MakeEulerQuat(0.50f, 0.0f, 0.30f) });
        nodeL.rotate.push_back({ 0.15f, MakeEulerQuat(0.70f, 0.0f, 0.40f) });
        nodeL.rotate.push_back({ 0.35f, MakeEulerQuat(0.40f, 0.0f, 0.20f) });
        anim.nodeAnimations["LeftArm_09"] = nodeL;

        NodeAnimation nodeR;
        nodeR.rotate.push_back({ 0.0f, MakeEulerQuat(0.50f, 0.0f, -0.30f) });
        nodeR.rotate.push_back({ 0.15f, MakeEulerQuat(0.70f, 0.0f, -0.40f) });
        nodeR.rotate.push_back({ 0.35f, MakeEulerQuat(0.40f, 0.0f, -0.20f) });
        anim.nodeAnimations["RightArm_014"] = nodeR;

        NodeAnimation nodeLF;
        nodeLF.rotate.push_back({ 0.0f, MakeEulerQuat(-0.30f, 0.0f, 0.0f) });
        nodeLF.rotate.push_back({ 0.15f, MakeEulerQuat(-0.40f, 0.0f, 0.0f) });
        nodeLF.rotate.push_back({ 0.35f, MakeEulerQuat(-0.20f, 0.0f, 0.0f) });
        anim.nodeAnimations["LeftForeArm_010"] = nodeLF;

        NodeAnimation nodeRF;
        nodeRF.rotate.push_back({ 0.0f, MakeEulerQuat(-0.30f, 0.0f, 0.0f) });
        nodeRF.rotate.push_back({ 0.15f, MakeEulerQuat(-0.40f, 0.0f, 0.0f) });
        nodeRF.rotate.push_back({ 0.35f, MakeEulerQuat(-0.20f, 0.0f, 0.0f) });
        anim.nodeAnimations["RightForeArm_015"] = nodeRF;
    }

    // Legs (脚を後ろへ真っ直ぐ伸ばす)
    {
        NodeAnimation nodeLUp;
        nodeLUp.rotate.push_back({ 0.0f, MakeEulerQuat(0.30f, 0.0f, 0.10f) });
        nodeLUp.rotate.push_back({ 0.15f, MakeEulerQuat(0.40f, 0.0f, 0.15f) });
        nodeLUp.rotate.push_back({ 0.35f, MakeEulerQuat(0.20f, 0.0f, 0.05f) });
        anim.nodeAnimations["LeftUpLeg_019"] = nodeLUp;

        NodeAnimation nodeRUp;
        nodeRUp.rotate.push_back({ 0.0f, MakeEulerQuat(0.20f, 0.0f, -0.10f) });
        nodeRUp.rotate.push_back({ 0.15f, MakeEulerQuat(0.30f, 0.0f, -0.15f) });
        nodeRUp.rotate.push_back({ 0.35f, MakeEulerQuat(0.10f, 0.0f, -0.05f) });
        anim.nodeAnimations["RightUpLeg_024"] = nodeRUp;

        NodeAnimation nodeLLeg;
        nodeLLeg.rotate.push_back({ 0.0f, MakeEulerQuat(0.20f, 0.0f, 0.0f) });
        nodeLLeg.rotate.push_back({ 0.15f, MakeEulerQuat(0.30f, 0.0f, 0.0f) });
        nodeLLeg.rotate.push_back({ 0.35f, MakeEulerQuat(0.10f, 0.0f, 0.0f) });
        anim.nodeAnimations["LeftLeg_020"] = nodeLLeg;

        NodeAnimation nodeRLeg;
        nodeRLeg.rotate.push_back({ 0.0f, MakeEulerQuat(0.30f, 0.0f, 0.0f) });
        nodeRLeg.rotate.push_back({ 0.15f, MakeEulerQuat(0.40f, 0.0f, 0.0f) });
        nodeRLeg.rotate.push_back({ 0.35f, MakeEulerQuat(0.20f, 0.0f, 0.0f) });
        anim.nodeAnimations["RightLeg_025"] = nodeRLeg;
    }

    return anim;
}
