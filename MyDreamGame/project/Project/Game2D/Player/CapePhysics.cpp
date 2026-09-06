#include "CapePhysics.h"
#include <algorithm>

namespace {
    // 単位ベクトル化
    Vector3 Normalize(const Vector3& v) {
        float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq > 0.000001f) {
            float invLen = 1.0f / std::sqrt(lenSq);
            return { v.x * invLen, v.y * invLen, v.z * invLen };
        }
        return { 0.0f, -1.0f, 0.0f };
    }

    float Length(const Vector3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
}

Quaternion CapePhysics::FromToRotation(const Vector3& fromVec, const Vector3& toVec) {
    Vector3 from = Normalize(fromVec);
    Vector3 to = Normalize(toVec);

    float dot = from.x * to.x + from.y * to.y + from.z * to.z;
    if (dot >= 0.99999f) {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (dot <= -0.99999f) {
        Vector3 axis = TransformFunctions::Cross(from, { 1.0f, 0.0f, 0.0f });
        if (axis.x * axis.x + axis.y * axis.y + axis.z * axis.z < 0.001f) {
            axis = TransformFunctions::Cross(from, { 0.0f, 1.0f, 0.0f });
        }
        axis = Normalize(axis);
        return Quaternion(axis.x, axis.y, axis.z, 0.0f);
    }

    Vector3 cross = TransformFunctions::Cross(from, to);
    Quaternion q;
    q.x = cross.x;
    q.y = cross.y;
    q.z = cross.z;
    q.w = 1.0f + dot;

    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 0.00001f) {
        q.x /= len;
        q.y /= len;
        q.z /= len;
        q.w /= len;
    }
    return q;
}

void CapePhysics::Initialize(AnimatorComponent* animator) {
    chains_.clear();
    if (!animator) return;

    const Skeleton& skeleton = animator->GetSkeleton();
    if (skeleton.joints.empty()) return;

    // 怪盗ゴーストのマント全5チェーン（各3ノード）
    const std::vector<std::vector<std::string>> chainDefinitions = {
        { "マント中央_1", "ボーン.015", "ボーン.014" },
        { "マント中央_2", "ボーン.009", "ボーン.008" },
        { "マント中央_3", "ボーン.005", "ボーン.004" },
        { "マント中央_4", "ボーン.012", "ボーン.011" },
        { "マント中央_5", "ボーン.018", "ボーン.017" }
    };

    for (size_t cIdx = 0; cIdx < chainDefinitions.size(); ++cIdx) {
        CapeChain chain;
        chain.chainName = "Chain_" + std::to_string(cIdx + 1);

        for (size_t nIdx = 0; nIdx < chainDefinitions[cIdx].size(); ++nIdx) {
            const std::string& bName = chainDefinitions[cIdx][nIdx];
            auto it = skeleton.jointMap.find(bName);
            if (it == skeleton.jointMap.end()) continue;

            CapeNode node;
            node.boneName = bName;
            node.jointIndex = it->second;

            const Joint& joint = skeleton.joints[node.jointIndex];
            if (joint.parent) {
                node.parentJointIndex = *joint.parent;
            }

            node.localBindOffset = joint.defaultTransform.translate;
            node.boneLength = Length(node.localBindOffset);
            if (node.boneLength < 0.01f) {
                node.boneLength = 0.15f; // 安全フォールバック長
            }

            chain.nodes.push_back(node);
        }

        if (!chain.nodes.empty()) {
            chains_.push_back(chain);
        }
    }

    isReady_ = !chains_.empty();
}

void CapePhysics::Reset(AnimatorComponent* animator, const Matrix4x4& modelWorldMatrix) {
    if (!animator || chains_.empty()) return;

    const Skeleton& skeleton = animator->GetSkeleton();

    for (auto& chain : chains_) {
        for (auto& node : chain.nodes) {
            if (node.jointIndex >= 0 && node.jointIndex < (int32_t)skeleton.joints.size()) {
                const Joint& joint = skeleton.joints[node.jointIndex];
                Matrix4x4 jointWorld = joint.skeletonSpaceMatrix * modelWorldMatrix;
                Vector3 worldPos = TransformFunctions::EulerTransform({ 0.0f, 0.0f, 0.0f }, jointWorld);
                node.currentPos = worldPos;
                node.prevPos = worldPos;
                node.isInitialized = true;
            }
        }
    }
}

void CapePhysics::Update(AnimatorComponent* animator, const Matrix4x4& modelWorldMatrix, const Vector3& playerVelocity, float deltaTime) {
    if (!isReady_ || !animator) return;

    float dt = std::clamp(deltaTime, 0.001f, 0.05f);
    windTime_ += dt;

    const Skeleton& skeleton = animator->GetSkeleton();
    if (skeleton.joints.empty()) return;

    // 全ノードが未初期化なら初期化実行
    if (!chains_.empty() && !chains_[0].nodes.empty() && !chains_[0].nodes[0].isInitialized) {
        Reset(animator, modelWorldMatrix);
    }

    // プレイヤー移動による反作用慣性力（ダッシュやジャンプでのなびき）
    Vector3 inertiaForce = {
        -playerVelocity.x * params_.inertiaFactor * 1.5f,
        -playerVelocity.y * params_.inertiaFactor * 0.8f,
        -playerVelocity.z * params_.inertiaFactor * 1.5f
    };

    // マントの各チェーンをシミュレーション
    for (size_t cIdx = 0; cIdx < chains_.size(); ++cIdx) {
        auto& chain = chains_[cIdx];
        if (chain.nodes.empty()) continue;

        // チェーン固有の微小な位相ズレそよ風（待機時の生きているような波打ち）
        float chainPhase = static_cast<float>(cIdx) * 1.25f;
        Vector3 windForce = {
            std::sin(windTime_ * 2.8f + chainPhase) * params_.windStrength,
            std::cos(windTime_ * 3.5f + chainPhase) * (params_.windStrength * 0.4f),
            std::sin(windTime_ * 2.2f + chainPhase * 0.8f) * (params_.windStrength * 0.6f)
        };

        Vector3 totalForce = TransformFunctions::AddV(params_.gravity, TransformFunctions::AddV(inertiaForce, windForce));

        for (size_t i = 0; i < chain.nodes.size(); ++i) {
            CapeNode& node = chain.nodes[i];
            if (node.jointIndex < 0 || node.jointIndex >= (int32_t)skeleton.joints.size()) continue;

            // 親のワールド位置を取得
            Vector3 parentWorldPos{};
            Matrix4x4 parentWorldMatrix = modelWorldMatrix;

            if (node.parentJointIndex >= 0 && node.parentJointIndex < (int32_t)skeleton.joints.size()) {
                const Joint& parentJoint = skeleton.joints[node.parentJointIndex];
                parentWorldMatrix = parentJoint.skeletonSpaceMatrix * modelWorldMatrix;
                parentWorldPos = TransformFunctions::EulerTransform({ 0.0f, 0.0f, 0.0f }, parentWorldMatrix);
            } else {
                parentWorldPos = TransformFunctions::EulerTransform({ 0.0f, 0.0f, 0.0f }, modelWorldMatrix);
            }

            // 親ボーンの姿勢に基づく本来の目標位置（バインド時の相対位置）
            Vector3 targetWorldPos = TransformFunctions::EulerTransform(node.localBindOffset, parentWorldMatrix);

            // 1. Verlet積分による慣性移動
            Vector3 velocity = TransformFunctions::SubtractV(node.currentPos, node.prevPos);
            velocity = TransformFunctions::MultiplyV(params_.damping, velocity);
            node.prevPos = node.currentPos;
            
            Vector3 accelStep = TransformFunctions::MultiplyV(dt * dt, totalForce);
            node.currentPos = TransformFunctions::AddV(node.currentPos, TransformFunctions::AddV(velocity, accelStep));

            // 2. 復元バネ力（本来の形状に戻ろうとする力）
            node.currentPos = TransformFunctions::Lerp(node.currentPos, targetWorldPos, params_.stiffness);

            // 3. 距離拘束（親ノードとの長さを一定に保つ）
            Vector3 toNode = TransformFunctions::SubtractV(node.currentPos, parentWorldPos);
            float currentDist = Length(toNode);
            if (currentDist > 0.0001f) {
                Vector3 dir = Normalize(toNode);
                node.currentPos = TransformFunctions::AddV(parentWorldPos, TransformFunctions::MultiplyV(node.boneLength, dir));
            } else {
                node.currentPos = targetWorldPos;
            }

            // 4. 親ボーン（または自身）の回転を計算してオーバーライド
            // 親ボーンのローカル空間における理想方向ベクトルと現在の実際方向ベクトル
            Vector3 idealDirLocal = Normalize(node.localBindOffset);
            
            // ワールド空間の方向ベクトルを親ボーンのローカル空間に引き戻す
            Matrix4x4 parentWorldInv = TransformFunctions::Inverse(parentWorldMatrix);
            Vector3 currentParentLocal = TransformFunctions::EulerTransform(parentWorldPos, parentWorldInv);
            Vector3 currentNodeLocal = TransformFunctions::EulerTransform(node.currentPos, parentWorldInv);
            Vector3 currentDirLocal = Normalize(TransformFunctions::SubtractV(currentNodeLocal, currentParentLocal));

            Quaternion deltaRot = FromToRotation(idealDirLocal, currentDirLocal);

            // マントボーンのみ回転をオーバーライド（「体」などの本体ボーンは絶対に上書きしない）
            if (node.parentJointIndex >= 0 && node.parentJointIndex < (int32_t)skeleton.joints.size()) {
                const Joint& parentJoint = skeleton.joints[node.parentJointIndex];
                if (parentJoint.name == "体" || parentJoint.name == "Body" || 
                    parentJoint.name == "プレイヤー" || parentJoint.name == "Root" || 
                    parentJoint.name == "ボーン") {
                    // 親がキャラクター本体（体など）の場合は、体は絶対に回転させず、マントの根元ボーン自身を回転させる
                    if (node.jointIndex >= 0 && node.jointIndex < (int32_t)skeleton.joints.size()) {
                        const Joint& selfJoint = skeleton.joints[node.jointIndex];
                        Quaternion targetRot = selfJoint.defaultTransform.rotate * deltaRot;
                        animator->SetJointRotationOverride(node.boneName, targetRot, 0.95f);
                    }
                } else {
                    // 親もマントボーンであれば親ボーンを回転させる
                    Quaternion targetRot = parentJoint.defaultTransform.rotate * deltaRot;
                    animator->SetJointRotationOverride(parentJoint.name, targetRot, 0.95f);
                }
            }
        }
    }
}
