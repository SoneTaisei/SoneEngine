#include "Quaternion.h"

Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t) {
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

    Quaternion q3 = q2;
    // 内積が負の場合、最短経路を通るように反転する
    if (dot < 0.0f) {
        q3.x = -q2.x;
        q3.y = -q2.y;
        q3.z = -q2.z;
        q3.w = -q2.w;
        dot = -dot;
    }

    const float DOT_THRESHOLD = 0.9995f;
    if (dot > DOT_THRESHOLD) {
        // 近すぎる場合は線形補間（Lerp）して正規化する
        Quaternion result = {
            q1.x + t * (q3.x - q1.x),
            q1.y + t * (q3.y - q1.y),
            q1.z + t * (q3.z - q1.z),
            q1.w + t * (q3.w - q1.w)
        };
        float len = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
        if (len > 0.0f) {
            result.x /= len;
            result.y /= len;
            result.z /= len;
            result.w /= len;
        }
        return result;
    }

    float theta_0 = std::acos(dot);
    float theta = theta_0 * t;

    float sin_theta = std::sin(theta);
    float sin_theta_0 = std::sin(theta_0);

    float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;

    return {
        q1.x * s0 + q3.x * s1,
        q1.y * s0 + q3.y * s1,
        q1.z * s0 + q3.z * s1,
        q1.w * s0 + q3.w * s1
    };
}

#include <algorithm>

Vector3 Quaternion::ToEulerAngles() const {
    Vector3 angles;
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    angles.x = std::atan2(sinr_cosp, cosr_cosp);

    float val = 2.0f * (w * y - x * z);
    val = std::clamp(val, -1.0f, 1.0f);
    angles.y = std::asin(val);

    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    angles.z = std::atan2(siny_cosp, cosy_cosp);

    return angles;
}

Quaternion Quaternion::operator*(const Quaternion& other) const {
    return Quaternion(
        w * other.x + x * other.w + y * other.z - z * other.y,
        w * other.y - x * other.z + y * other.w + z * other.x,
        w * other.z + x * other.y - y * other.x + z * other.w,
        w * other.w - x * other.x - y * other.y - z * other.z
    );
}
