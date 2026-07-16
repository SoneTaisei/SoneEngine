#include "SkeletonDebugRenderer.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"


void SkeletonDebugRenderer::Initialize() {
    jointSpheres_.clear();
    boneCylinders_.clear();
}

void SkeletonDebugRenderer::Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix) {
    if (skeleton.joints.empty()) return;

    size_t jointCount = skeleton.joints.size();

    // 球体(関節)はモンスターボールのように見えて邪魔なため描画しない
    // jointSpheres_.clear();

    while (boneCylinders_.size() < jointCount) {
        auto cylinder = std::make_unique<PrimitiveObject>();
        cylinder->Initialize(DirectXCommon::GetInstance()->GetDevice(), PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Cylinder));
        cylinder->GetMaterial().color = Vector4{0.15f, 0.15f, 0.15f, 1.0f}; // 暗めのグレー
        boneCylinders_.push_back(std::move(cylinder));
    }

    for (size_t i = 0; i < jointCount; ++i) {
        const Joint& joint = skeleton.joints[i];

        // ワールド空間でのジョイントの行列
        Matrix4x4 jointWorld = joint.skeletonSpaceMatrix * worldMatrix;
        Vector3 pos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        // Sphereの描画は行わない

        // 親がいる場合はBone(Cylinder)を描画
        if (joint.parent) {
            const Joint& parent = skeleton.joints[*joint.parent];
            Matrix4x4 parentWorld = parent.skeletonSpaceMatrix * worldMatrix;
            Vector3 parentPos = { parentWorld.m[3][0], parentWorld.m[3][1], parentWorld.m[3][2] };

            Vector3 diff = { pos.x - parentPos.x, pos.y - parentPos.y, pos.z - parentPos.z };
            float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            if (length > 0.001f) {
                Vector3 dir = { diff.x / length, diff.y / length, diff.z / length };

                // Y軸からdirへ向く回転行列を作成
                // CylinderはデフォルトでY軸に伸びている
                Vector3 up = {0.0f, 1.0f, 0.0f};
                Vector3 axis = {up.y * dir.z - up.z * dir.y, up.z * dir.x - up.x * dir.z, up.x * dir.y - up.y * dir.x};
                float axisLen = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
                
                Matrix4x4 rotateMatrix;
                if (axisLen < 0.0001f) {
                    // ほぼ平行
                    rotateMatrix = TransformFunctions::MakeIdentity4x4();
                    if (up.y * dir.y < 0.0f) {
                        // 180度反転
                        rotateMatrix = TransformFunctions::MakeRoteZMatrix(3.14159265f);
                    }
                } else {
                    axis.x /= axisLen; axis.y /= axisLen; axis.z /= axisLen;
                    float cosTheta = up.x * dir.x + up.y * dir.y + up.z * dir.z;
                    float sinTheta = axisLen;
                    
                    // ロドリゲスの回転公式による行列作成
                    rotateMatrix.m[0][0] = cosTheta + axis.x * axis.x * (1 - cosTheta);
                    rotateMatrix.m[0][1] = axis.x * axis.y * (1 - cosTheta) - axis.z * sinTheta;
                    rotateMatrix.m[0][2] = axis.x * axis.z * (1 - cosTheta) + axis.y * sinTheta;
                    rotateMatrix.m[0][3] = 0.0f;
                    
                    rotateMatrix.m[1][0] = axis.y * axis.x * (1 - cosTheta) + axis.z * sinTheta;
                    rotateMatrix.m[1][1] = cosTheta + axis.y * axis.y * (1 - cosTheta);
                    rotateMatrix.m[1][2] = axis.y * axis.z * (1 - cosTheta) - axis.x * sinTheta;
                    rotateMatrix.m[1][3] = 0.0f;
                    
                    rotateMatrix.m[2][0] = axis.z * axis.x * (1 - cosTheta) - axis.y * sinTheta;
                    rotateMatrix.m[2][1] = axis.z * axis.y * (1 - cosTheta) + axis.x * sinTheta;
                    rotateMatrix.m[2][2] = cosTheta + axis.z * axis.z * (1 - cosTheta);
                    rotateMatrix.m[2][3] = 0.0f;
                    
                    rotateMatrix.m[3][0] = 0.0f; rotateMatrix.m[3][1] = 0.0f; rotateMatrix.m[3][2] = 0.0f; rotateMatrix.m[3][3] = 1.0f;
                }

                Vector3 center = { (pos.x + parentPos.x) * 0.5f, (pos.y + parentPos.y) * 0.5f, (pos.z + parentPos.z) * 0.5f };
                
                Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix({0.05f, length, 0.05f});
                Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(center);
                
                Matrix4x4 finalMatrix = TransformFunctions::Multiply(TransformFunctions::Multiply(scaleMatrix, rotateMatrix), translateMatrix);
                
                boneCylinders_[i]->SetOverrideMatrix(true, finalMatrix);
                
                boneCylinders_[i]->Update();
                boneCylinders_[i]->Draw();
            }
        }
    }
}
