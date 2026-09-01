#include "VerletPhysics2D.h"
#include "Game2D/MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"
#include <algorithm>
#include <cmath>

namespace {
    // 最近接点との距離がこれ未満なら「中心がブロック内部」とみなして面方向へ押し出す
    constexpr float kEpsilon = 1e-6f;
}

void VerletPhysics2D::Integrate(std::vector<VerletNode>& nodes, const Vector3& gravity, float damping, float dt) {
    for (auto& node : nodes) {
        if (node.invMass <= 0.0f) {
            // 固定ノードは動かさない（速度もゼロに保つ）
            node.prevPos = node.pos;
            continue;
        }
        // 暗黙の速度 = pos - prevPos
        Vector3 velocity = node.pos - node.prevPos;
        Vector3 next = node.pos + velocity * damping + gravity * (dt * dt);
        node.prevPos = node.pos;
        node.pos = next;
    }
}

void VerletPhysics2D::SolveDistanceConstraint(VerletNode& a, VerletNode& b, float restLength) {
    float wSum = a.invMass + b.invMass;
    if (wSum <= 0.0f) {
        return; // 両端固定なら何もしない
    }

    Vector3 d = b.pos - a.pos;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < kEpsilon) {
        return; // 完全に重なっている場合は方向が定まらないのでスキップ
    }

    float diff = (len - restLength) / len;
    float wA = a.invMass / wSum;
    float wB = b.invMass / wSum;

    // invMassの重み付けにより固定ノード(invMass=0)は自動的に動かない
    a.pos += d * (diff * wA);
    b.pos -= d * (diff * wB);
}

void VerletPhysics2D::CollideNodeWithMap(VerletNode& node, MapChip2D* map, float friction) {
    if (!map || node.invMass <= 0.0f) {
        return;
    }

    const float r = node.radius;
    const float chipSize = map->GetChipSize();

    // ノード周辺のチップだけを調べる
    int minCx = map->WorldToChipX(node.pos.x - r);
    int maxCx = map->WorldToChipX(node.pos.x + r);
    int minCy = map->WorldToChipY(node.pos.y - r);
    int maxCy = map->WorldToChipY(node.pos.y + r);

    for (int cy = minCy; cy <= maxCy; ++cy) {
        for (int cx = minCx; cx <= maxCx; ++cx) {
            // 鎖が衝突するのは通常ブロックのみ
            // （kOneWayBlockの一方通行判定はプレイヤー移動専用なのですり抜け、kDeathBlockも鎖には無害）
            if (map->GetChipType(cx, cy) != MapChip2D::ChipType::kBlock) {
                continue;
            }

            // ChipToWorldX/Y はチップの左下を返す仕様
            float left = map->ChipToWorldX(cx);
            float bottom = map->ChipToWorldY(cy);
            AABB2D chipBox = { left, bottom + chipSize, left + chipSize, bottom };

            CollideNodeWithAABB(node, chipBox, friction);
        }
    }
}

bool VerletPhysics2D::CollideNodeWithAABB(VerletNode& node, const AABB2D& aabb, float friction) {
    if (node.invMass <= 0.0f) {
        return false;
    }

    const float r = node.radius;

    // AABB上の最近接点
    float closestX = std::clamp(node.pos.x, aabb.left, aabb.right);
    float closestY = std::clamp(node.pos.y, aabb.bottom, aabb.top);

    float dx = node.pos.x - closestX;
    float dy = node.pos.y - closestY;
    float distSq = dx * dx + dy * dy;

    if (distSq >= r * r) {
        return false; // 接触なし
    }

    if (distSq > kEpsilon * kEpsilon) {
        // 通常の押し出し（最近接点方式なので角でも法線が連続的に回り、滑らかに巻き付く）
        float dist = std::sqrt(distSq);
        float push = (r - dist) / dist;
        node.pos.x += dx * push;
        node.pos.y += dy * push;
    } else {
        // 中心がAABB内部に入った場合：最も近い面の外向き法線方向へ押し出す
        // （この分岐を忘れると高速時にNaNや貫通が起きる）
        float pushLeft = (node.pos.x - aabb.left) + r;   // -x方向への押し出し量
        float pushRight = (aabb.right - node.pos.x) + r;  // +x方向
        float pushDown = (node.pos.y - aabb.bottom) + r;  // -y方向
        float pushUp = (aabb.top - node.pos.y) + r;       // +y方向

        float minPush = (std::min)({ pushLeft, pushRight, pushDown, pushUp });
        if (minPush == pushLeft) {
            node.pos.x -= pushLeft;
        } else if (minPush == pushRight) {
            node.pos.x += pushRight;
        } else if (minPush == pushDown) {
            node.pos.y -= pushDown;
        } else {
            node.pos.y += pushUp;
        }
    }

    ApplyContactFriction(node, friction);
    return true;
}

void VerletPhysics2D::ApplyVelocity(VerletNode& node, const Vector3& velocity, float dt, float influence) {
    if (node.invMass <= 0.0f || influence <= 0.0f) {
        return;
    }
    // prevPosを後ろへずらす = 暗黙速度にvelocityを加える（Verlet流の速度注入）
    node.prevPos -= velocity * (dt * influence);
}

void VerletPhysics2D::ResetVelocities(std::vector<VerletNode>& nodes) {
    for (auto& node : nodes) {
        node.prevPos = node.pos;
    }
}

void VerletPhysics2D::ClampToPlaneZ(std::vector<VerletNode>& nodes) {
    for (auto& node : nodes) {
        node.pos.z = 0.0f;
        node.prevPos.z = 0.0f;
    }
}

void VerletPhysics2D::ApplyContactFriction(VerletNode& node, float friction) {
    if (friction <= 0.0f) {
        return;
    }
    // prevPosをposへ近づける = 暗黙速度を削る（Verletならではの1行摩擦）
    node.prevPos = TransformFunctions::Lerp(node.prevPos, node.pos, friction);
}
