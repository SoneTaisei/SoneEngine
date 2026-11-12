#include "Animation.h"
#include "TransformFunctions.h"
#include "Core/Utility/DxResourceUtils.h"
#include "Core/Utility/LogManager.h"
#include "Renderer/SrvManager.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <set>
#include <algorithm>

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

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    Animation animation; // create animation
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_ConvertToLeftHanded | aiProcess_GlobalScale);
    assert(scene && scene->mNumAnimations != 0); // assert animation exists
    aiAnimation* animationAssimp = scene->mAnimations[0]; // use first animation
    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // convert to seconds

    // loop channels for node animations
    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        
        // Translate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z}; // Converter already handled by Assimp
            nodeAnimation.translate.push_back(keyframe);
        }

        // Rotate
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w}; // Converter already handled by Assimp
            nodeAnimation.rotate.push_back(keyframe);
        }

        // Scale
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // convert to seconds
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.scale.push_back(keyframe);
        }
    }
    return animation;
}

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename, const std::string& animationName) {
    Animation animation;
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_ConvertToLeftHanded | aiProcess_GlobalScale);
    if (!scene || scene->mNumAnimations == 0) {
        return animation;
    }

    aiAnimation* animationAssimp = nullptr;
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        if (std::string(scene->mAnimations[i]->mName.C_Str()) == animationName) {
            animationAssimp = scene->mAnimations[i];
            break;
        }
    }

    if (!animationAssimp) {
        animationAssimp = scene->mAnimations[0]; // Fallback to first animation if name not found
    }

    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.translate.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w};
            nodeAnimation.rotate.push_back(keyframe);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
            keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
            nodeAnimation.scale.push_back(keyframe);
        }
    }

    // デバッグログ: ロードされたチャンネル名を出力
    std::string keysLog = "Loaded animation [" + animationName + "] channels: ";
    for (const auto& [key, val] : animation.nodeAnimations) {
        keysLog += key + ", ";
    }
    LogManager::GetInstance()->AddLog(LogLevel::Info, keysLog);

    return animation;
}

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = TransformFunctions::MakeIdentity4x4();
    joint.transform = node.transform;
    joint.defaultTransform = node.transform;
    joint.index = int32_t(joints.size());
    joint.parent = parent;
    joints.push_back(joint);
    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }
    return joint.index;
}

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);
    return skeleton;
}

void Update(Skeleton& skeleton) {
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = TransformFunctions::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
        if (joint.parent) {
            joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
    static std::set<std::string> reportedMissingNodes;
    for (Joint& joint : skeleton.joints) {
        if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
            const NodeAnimation& rootNodeAnimation = (*it).second;
            if (!rootNodeAnimation.translate.empty()) {
                joint.transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime);
            } else {
                joint.transform.translate = joint.defaultTransform.translate;
            }

            if (!rootNodeAnimation.rotate.empty()) {
                joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime);
            } else {
                joint.transform.rotate = joint.defaultTransform.rotate;
            }

            if (!rootNodeAnimation.scale.empty()) {
                joint.transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime);
            } else {
                joint.transform.scale = joint.defaultTransform.scale;
            }
        } else {
            joint.transform = joint.defaultTransform;
            if (reportedMissingNodes.find(joint.name) == reportedMissingNodes.end()) {
                reportedMissingNodes.insert(joint.name);
                LogManager::GetInstance()->AddLog(LogLevel::Info, "Animation channel not found for joint: " + joint.name);
            }
        }
    }
}

SkinCluster CreateSkinCluster(Microsoft::WRL::ComPtr<ID3D12Device> device, const Skeleton& skeleton, const ModelData& modelData) {
    SkinCluster skinCluster;

    // paletteResourceの生成 (関節数分のマトリックス配列)
    skinCluster.paletteResource = DxResourceUtils::CreateBufferResource(device, sizeof(WellForGPU) * skeleton.joints.size());
    WellForGPU* mappedPalette = nullptr;
    skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
    skinCluster.mappedPalette = {mappedPalette, skeleton.joints.size()};

    // SRVの生成
    SrvManager::GetInstance()->Allocate(&skinCluster.paletteSrvHandle.first, &skinCluster.paletteSrvHandle.second, "CreateSkinCluster");

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
    srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);

    device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &srvDesc, skinCluster.paletteSrvHandle.first);

    // influenceResourceの生成 (頂点ごとのウェイトデータ)
    skinCluster.influenceResource = DxResourceUtils::CreateBufferResource(device, sizeof(VertexInfluence) * modelData.vertices.size());
    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.mappedInfluence = {mappedInfluence, modelData.vertices.size()};

    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // InverseBindPoseMatrix の保存
    skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
    std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), TransformFunctions::MakeIdentity4x4);

    // ウェイト情報のパース
    for (const auto& jointWeight : modelData.skinClusterData) {
        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) {
            LogManager::GetInstance()->AddLog(LogLevel::Warning, "[SkinCluster] Joint NOT FOUND in skeleton: " + jointWeight.first);
            continue; // そのボーンは存在しない
        }

        int32_t jointIndex = it->second;
        LogManager::GetInstance()->AddLog(LogLevel::Info, "[SkinCluster] Bound joint [" + jointWeight.first + "] -> index " + std::to_string(jointIndex) + ", weights count: " + std::to_string(jointWeight.second.vertexWeights.size()));
        skinCluster.inverseBindPoseMatrices[jointIndex] = jointWeight.second.inverseBindPoseMatrix;

        for (const auto& weightInfo : jointWeight.second.vertexWeights) {
            uint32_t vIndex = weightInfo.vertexIndex;
            if (vIndex >= modelData.vertices.size()) continue;

            // 空いているウェイトスロットを探す
            for (uint32_t slot = 0; slot < kNumMaxInfluence; ++slot) {
                if (skinCluster.mappedInfluence[vIndex].weights[slot] == 0.0f) {
                    skinCluster.mappedInfluence[vIndex].weights[slot] = weightInfo.weight;
                    skinCluster.mappedInfluence[vIndex].jointIndices[slot] = jointIndex;
                    break;
                }
            }
        }
    }

    return skinCluster;
}

void Update(SkinCluster& skinCluster, const Skeleton& skeleton) {
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
        
        Matrix4x4 inverseBindPose = skinCluster.inverseBindPoseMatrices[jointIndex];
        Matrix4x4 skeletonSpaceMatrix = skeleton.joints[jointIndex].skeletonSpaceMatrix;

        // パレットに格納する行列 = inverseBindPose * skeletonSpaceMatrix
        Matrix4x4 paletteMatrix = inverseBindPose * skeletonSpaceMatrix;
        skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = paletteMatrix;
        
        // 法線用の逆転置行列
        skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = TransformFunctions::Transpose(TransformFunctions::Inverse(paletteMatrix));
    }
}
