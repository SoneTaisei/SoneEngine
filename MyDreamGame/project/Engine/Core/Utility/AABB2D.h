#pragma once

struct AABB2D {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    // 厳密に重なっているか（辺が接しているだけなら false）
    bool Intersects(const AABB2D &other) const {
        return right > other.left && left < other.right &&
               top > other.bottom && bottom < other.top;
    }

    // 境界接触も含む判定（従来の PlayerPhysics::CheckAABBCollision 相当）
    bool IntersectsInclusive(const AABB2D &other) const {
        return !(right < other.left || left > other.right ||
                 top < other.bottom || bottom > other.top);
    }
};