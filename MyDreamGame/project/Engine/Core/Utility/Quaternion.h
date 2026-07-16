#pragma once

#include <cmath>
#include "Vector3.h"

struct Quaternion {
    float x;
    float y;
    float z;
    float w;

    Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    Vector3 ToEulerAngles() const;
};

Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t);
