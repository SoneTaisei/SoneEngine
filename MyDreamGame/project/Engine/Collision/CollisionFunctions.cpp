#include "CollisionFunctions.h"
#include <algorithm>

namespace {
    inline float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    inline float LengthSq(const Vector3& v) {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }
    inline float Length(const Vector3& v) {
        return std::sqrt(LengthSq(v));
    }
    inline Vector3 Normalize(const Vector3& v) {
        float len = Length(v);
        if (len > 1e-6f) {
            return { v.x / len, v.y / len, v.z / len };
        }
        return { 0.0f, 0.0f, 1.0f };
    }
    inline float Clamp(float v, float minVal, float maxVal) {
        return (v < minVal) ? minVal : ((v > maxVal) ? maxVal : v);
    }
}

void VisionCone::SetForwardFromEuler(const Vector3& eulerRad) {
    float cp = std::cos(eulerRad.x);
    float sp = std::sin(eulerRad.x);
    float cy = std::cos(eulerRad.y);
    float sy = std::sin(eulerRad.y);

    forward = Normalize({ sy * cp, -sp, cy * cp });
}

bool IsPointInVisionCone(const Vector3& point, const VisionCone& cone) {
    if (cone.distance <= 0.0f || cone.halfAngleRad <= 0.0f) {
        return false;
    }

    Vector3 toTarget = point - cone.eyePosition;
    float distSq = LengthSq(toTarget);

    // 最大距離チェック（球面底）
    if (distSq > cone.distance * cone.distance) {
        return false;
    }
    // 視点そのもの
    if (distSq <= 1e-8f) {
        return true;
    }

    float dist = std::sqrt(distSq);
    float dot = Dot(toTarget, cone.forward);

    float cosAngle = dot / dist;
    float cosHalf = std::cos(cone.halfAngleRad);

    return cosAngle >= cosHalf;
}

bool IsSphereInVisionCone(const Vector3& sphereCenter, float sphereRadius, const VisionCone& cone) {
    if (cone.distance <= 0.0f || cone.halfAngleRad <= 0.0f) {
        return false;
    }
    if (sphereRadius < 0.0f) {
        return false;
    }
    if (sphereRadius == 0.0f) {
        return IsPointInVisionCone(sphereCenter, cone);
    }

    Vector3 toTarget = sphereCenter - cone.eyePosition;
    float distSq = LengthSq(toTarget);

    // 1. 最大距離チェック（球の最近接点までの直線距離）
    float maxReach = cone.distance + sphereRadius;
    if (distSq > maxReach * maxReach) {
        return false;
    }

    // 2. 視点（Apex）が球の内部にある場合
    if (distSq <= sphereRadius * sphereRadius) {
        return true;
    }

    // 3. 球の中心方向の角度判定
    float dist = std::sqrt(distSq);
    float dot = Dot(toTarget, cone.forward);
    float cosPhi = Clamp(dot / dist, -1.0f, 1.0f);
    float cosHalf = std::cos(cone.halfAngleRad);

    // 球の中心がコーン内部にある場合
    if (cosPhi >= cosHalf) {
        return true;
    }

    // 4. 球の中心がコーンの外側にある場合：側面または球面底のフチ（Rim）との接触判定
    float phi = std::acos(cosPhi);
    float sinBeta = Clamp(sphereRadius / dist, 0.0f, 1.0f);
    float beta = std::asin(sinBeta);

    // 角度的に球の表面がコーンと交差する余地がない場合
    if (phi > cone.halfAngleRad + beta) {
        return false;
    }

    // 最も近い母線上の最近接点の距離
    float deltaAngle = phi - cone.halfAngleRad;
    float tProj = dist * std::cos(deltaAngle);

    if (tProj <= cone.distance) {
        // コーン側面の有効領域（0 <= tProj <= distance）で側面と球が接触
        return true;
    }

    // tProj > cone.distance の場合：球面底のフチ（円周Rim）との接触判定
    Vector3 perp = toTarget - (cone.forward * dot);
    float perpLen = Length(perp);
    Vector3 perpDir = (perpLen > 1e-6f) ? (perp * (1.0f / perpLen)) : Vector3{ 0.0f, 1.0f, 0.0f };

    Vector3 rimPoint = cone.eyePosition +
        (cone.forward * (cone.distance * std::cos(cone.halfAngleRad))) +
        (perpDir * (cone.distance * std::sin(cone.halfAngleRad)));

    float distToRimSq = LengthSq(sphereCenter - rimPoint);
    return distToRimSq <= (sphereRadius * sphereRadius);
}

bool IsSphereInVisionCone(const SphereShape& sphere, const VisionCone& cone) {
    return IsSphereInVisionCone(sphere.center, sphere.radius, cone);
}

bool IsPointInCone(const Vector3& point, const ConeShape& cone) {
    if (cone.height <= 0.0f || cone.radius <= 0.0f) {
        return false;
    }

    Vector3 v = point - cone.apex;
    float h = Dot(v, cone.direction);

    // 高さの範囲チェック
    if (h < 0.0f || h > cone.height) {
        return false;
    }

    // その高さにおけるコーン半径
    float rh = (h / cone.height) * cone.radius;
    Vector3 axisPt = cone.apex + (cone.direction * h);
    Vector3 perp = point - axisPt;

    return LengthSq(perp) <= (rh * rh);
}

bool IsSphereInCone(const SphereShape& sphere, const ConeShape& cone) {
    if (cone.height <= 0.0f || cone.radius <= 0.0f || sphere.radius < 0.0f) {
        return false;
    }
    if (sphere.radius == 0.0f) {
        return IsPointInCone(sphere.center, cone);
    }

    // 点判定で中心が入っていれば即ヒット
    if (IsPointInCone(sphere.center, cone)) {
        return true;
    }

    // 1. 底面円盤との交差判定
    Vector3 baseCenter = cone.apex + (cone.direction * cone.height);
    Vector3 toCenter = sphere.center - baseCenter;
    float distToBasePlane = Dot(toCenter, cone.direction);

    // 底面平面からの距離が半径以内の場合
    if (std::abs(distToBasePlane) <= sphere.radius) {
        Vector3 projOnBase = sphere.center - (cone.direction * distToBasePlane);
        Vector3 baseDiff = projOnBase - baseCenter;
        float baseDistSq = LengthSq(baseDiff);
        if (baseDistSq <= cone.radius * cone.radius) {
            return true; // 底面円盤内部と接触
        }
        // 底面の円周との最近接判定
        float baseDist = std::sqrt(baseDistSq);
        Vector3 closestRim = baseCenter + (baseDiff * (cone.radius / baseDist));
        if (LengthSq(sphere.center - closestRim) <= sphere.radius * sphere.radius) {
            return true;
        }
    }

    // 2. 側面（母線）との交差判定
    Vector3 v = sphere.center - cone.apex;
    float h = Dot(v, cone.direction);
    Vector3 perp = v - (cone.direction * h);
    float perpLen = Length(perp);
    Vector3 perpDir = (perpLen > 1e-6f) ? (perp * (1.0f / perpLen)) : Vector3{ 1.0f, 0.0f, 0.0f };

    // コーン側面を母線ベクトルとして表現
    Vector3 slantDir = Normalize((cone.direction * cone.height) + (perpDir * cone.radius));
    float slantLength = std::sqrt(cone.height * cone.height + cone.radius * cone.radius);

    // 母線への投影
    float t = Dot(v, slantDir);
    t = Clamp(t, 0.0f, slantLength);

    Vector3 closestPtOnSlant = cone.apex + (slantDir * t);
    return LengthSq(sphere.center - closestPtOnSlant) <= (sphere.radius * sphere.radius);
}

#ifdef USE_IMGUI
void DrawVisionConeImGui(const char* label, VisionCone& cone) {
    if (ImGui::TreeNode(label)) {
        ImGui::DragFloat3("Eye Position", &cone.eyePosition.x, 0.1f);
        if (ImGui::DragFloat3("Forward", &cone.forward.x, 0.01f, -1.0f, 1.0f)) {
            cone.forward = Normalize(cone.forward);
        }
        ImGui::DragFloat("Distance", &cone.distance, 0.1f, 0.0f, 1000.0f);

        float angleDeg = cone.GetAngleDegree();
        if (ImGui::SliderFloat("Half Angle (Deg)", &angleDeg, 1.0f, 89.0f, "%.1f deg")) {
            cone.SetAngleDegree(angleDeg);
        }
        ImGui::TreePop();
    }
}
#endif
