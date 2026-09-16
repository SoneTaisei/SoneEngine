#include "PhysicsAStar.h"
#include "Game2D/MapChip2D.h"
#include <cmath>
#include <algorithm>
#include <limits>

void PhysicsAStar::BuildDistanceField(MapChip2D* mapChip, const Vector3& goalPos) {
    if (!mapChip) return;

    mapW_ = mapChip->GetWidth();
    mapH_ = mapChip->GetHeight();
    if (mapW_ <= 0 || mapH_ <= 0) return;

    distanceField_.assign(mapH_, std::vector<float>(mapW_, 9999.0f));

    int goalX = mapChip->WorldToChipX(goalPos.x);
    int goalY = mapChip->WorldToChipY(goalPos.y);

    std::queue<std::pair<int, int>> q;

    if (goalX >= 0 && goalX < mapW_ && goalY >= 0 && goalY < mapH_) {
        distanceField_[goalY][goalX] = 0.0f;
        q.push({ goalX, goalY });
    } else {
        for (int y = 0; y < mapH_; ++y) {
            for (int x = 0; x < mapW_; ++x) {
                if (mapChip->GetChipType(x, y) == MapChip2D::ChipType::kGoal) {
                    distanceField_[y][x] = 0.0f;
                    q.push({ x, y });
                }
            }
        }
    }

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        float curDist = distanceField_[cy][cx];

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < mapW_ && ny >= 0 && ny < mapH_) {
                MapChip2D::ChipType type = mapChip->GetChipType(nx, ny);
                if (type == MapChip2D::ChipType::kBlock || type == MapChip2D::ChipType::kDeathBlock) {
                    continue;
                }

                if (distanceField_[ny][nx] > curDist + 1.0f) {
                    distanceField_[ny][nx] = curDist + 1.0f;
                    q.push({ nx, ny });
                }
            }
        }
    }
}

float PhysicsAStar::CalculateHeuristic(float cx, float cy, float gx, float gy, const PlayerParams& params, MapChip2D* mapChip) const {
    if (!mapChip || distanceField_.empty()) {
        float dx = std::abs(cx - gx);
        float dy = std::abs(cy - gy);
        return (dx / params.moveSpeed_) * 60.0f + (dy / params.jumpPower_) * 60.0f;
    }

    int chipX = mapChip->WorldToChipX(cx);
    int chipY = mapChip->WorldToChipY(cy);

    if (chipX >= 0 && chipX < mapW_ && chipY >= 0 && chipY < mapH_) {
        float d = distanceField_[chipY][chipX];
        if (d < 9000.0f) {
            return (d * mapChip->GetChipSize() / params.moveSpeed_) * 60.0f;
        }
    }

    float dx = std::abs(cx - gx);
    float dy = std::abs(cy - gy);
    return (dx / params.moveSpeed_) * 60.0f + (dy / params.jumpPower_) * 60.0f + 100.0f;
}

uint64_t PhysicsAStar::EncodeStateHash(float x, float y, float vx, float vy, bool isGrounded, bool isWallL, bool isWallR) const {
    int32_t qx = static_cast<int32_t>(std::round(x * 4.0f));
    int32_t qy = static_cast<int32_t>(std::round(y * 4.0f));
    int32_t qvx = static_cast<int32_t>(std::round(vx * 2.0f));
    int32_t qvy = static_cast<int32_t>(std::round(vy * 2.0f));

    uint64_t hash = 0;
    hash |= (static_cast<uint64_t>(qx & 0xFFFF) << 48);
    hash |= (static_cast<uint64_t>(qy & 0xFFFF) << 32);
    hash |= (static_cast<uint64_t>(qvx & 0xFF) << 24);
    hash |= (static_cast<uint64_t>(qvy & 0xFF) << 16);
    hash |= (isGrounded ? 1ULL : 0ULL) << 0;
    hash |= (isWallL ? 1ULL : 0ULL) << 1;
    hash |= (isWallR ? 1ULL : 0ULL) << 2;

    return hash;
}

std::vector<MacroActionType> PhysicsAStar::GetAvailableActions(const PhysicsState& state) const {
    std::vector<MacroActionType> actions;

    if (state.isGrounded) {
        actions.push_back(MacroActionType::RunRight);
        actions.push_back(MacroActionType::RunRightLong);
        actions.push_back(MacroActionType::RunLeft);
        actions.push_back(MacroActionType::RunLeftLong);
        actions.push_back(MacroActionType::JumpRight);
        actions.push_back(MacroActionType::JumpLeft);
        actions.push_back(MacroActionType::JumpNeutral);
        actions.push_back(MacroActionType::JumpShortRight);
        actions.push_back(MacroActionType::JumpShortLeft);
    } else {
        actions.push_back(MacroActionType::RunRight);
        actions.push_back(MacroActionType::RunLeft);
        actions.push_back(MacroActionType::FallWait);
    }

    return actions;
}

bool PhysicsAStar::CheckCollisionAtAABB(float x, float y, const PlayerParams& params, MapChip2D* mapChip) const {
    if (!mapChip) return false;

    float minX = x - params.halfWidth_;
    float maxX = x + params.halfWidth_;
    float minY = y - params.halfHeight_;
    float maxY = y + params.halfHeight_;

    int startChipX = mapChip->WorldToChipX(minX + 0.001f);
    int endChipX   = mapChip->WorldToChipX(maxX - 0.001f);
    int startChipY = mapChip->WorldToChipY(minY + 0.001f);
    int endChipY   = mapChip->WorldToChipY(maxY - 0.001f);

    for (int cy = startChipY; cy <= endChipY; ++cy) {
        for (int cx = startChipX; cx <= endChipX; ++cx) {
            MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
            if (type == MapChip2D::ChipType::kBlock || type == MapChip2D::ChipType::kDeathBlock) {
                return true;
            }
        }
    }
    return false;
}

bool PhysicsAStar::CheckGoalReached(float x, float y, const PlayerParams& params, const Vector3& goalPos, MapChip2D* mapChip) const {
    float dx = x - goalPos.x;
    float dy = y - goalPos.y;
    if ((dx * dx + dy * dy) < 4.0f) {
        return true;
    }

    if (mapChip) {
        float minX = x - params.halfWidth_;
        float maxX = x + params.halfWidth_;
        float minY = y - params.halfHeight_;
        float maxY = y + params.halfHeight_;

        int startChipX = mapChip->WorldToChipX(minX);
        int endChipX   = mapChip->WorldToChipX(maxX);
        int startChipY = mapChip->WorldToChipY(minY);
        int endChipY   = mapChip->WorldToChipY(maxY);

        for (int cy = startChipY; cy <= endChipY; ++cy) {
            for (int cx = startChipX; cx <= endChipX; ++cx) {
                if (mapChip->GetChipType(cx, cy) == MapChip2D::ChipType::kGoal) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool PhysicsAStar::SimulateMacroAction(PhysicsState& state, MacroActionType action, MapChip2D* mapChip, const PlayerParams& params) const {
    state.pathSegment.clear();

    float inputDirX = 0.0f;
    bool isJumpAction = false;
    bool isShortJump = false;
    int targetSteps = 0;

    switch (action) {
    case MacroActionType::RunRight:
        inputDirX = 1.0f;
        targetSteps = 8;
        break;
    case MacroActionType::RunRightLong:
        inputDirX = 1.0f;
        targetSteps = 20;
        break;
    case MacroActionType::RunLeft:
        inputDirX = -1.0f;
        targetSteps = 8;
        break;
    case MacroActionType::RunLeftLong:
        inputDirX = -1.0f;
        targetSteps = 20;
        break;
    case MacroActionType::JumpRight:
        inputDirX = 1.0f;
        isJumpAction = true;
        targetSteps = 28;
        break;
    case MacroActionType::JumpLeft:
        inputDirX = -1.0f;
        isJumpAction = true;
        targetSteps = 28;
        break;
    case MacroActionType::JumpNeutral:
        inputDirX = 0.0f;
        isJumpAction = true;
        targetSteps = 28;
        break;
    case MacroActionType::JumpShortRight:
        inputDirX = 1.0f;
        isJumpAction = true;
        isShortJump = true;
        targetSteps = 16;
        break;
    case MacroActionType::JumpShortLeft:
        inputDirX = -1.0f;
        isJumpAction = true;
        isShortJump = true;
        targetSteps = 16;
        break;
    case MacroActionType::FallWait:
        inputDirX = 0.0f;
        targetSteps = 16;
        break;
    default:
        return false;
    }

    if (targetSteps <= 0) targetSteps = 8;

    // --- 発動条件チェック ---
    if (isJumpAction) {
        if (!state.isGrounded) return false;
        state.vy = isShortJump ? (params.jumpPower_ * 0.65f) : params.jumpPower_;
        state.isGrounded = false;
    }

    // 1サブステップごとの物理運動および衝突レスポンス
    for (int step = 0; step < targetSteps; ++step) {
        state.vx = inputDirX * params.moveSpeed_;

        // 重力適用
        if (!state.isGrounded) {
            state.vy += params.gravity_ * FIXED_DELTA_TIME;
            if (state.vy < params.maxFallSpeed_) state.vy = params.maxFallSpeed_;
        }

        // --- Y軸移動と衝突解決 ---
        float moveY = state.vy * FIXED_DELTA_TIME;
        float nextY = state.y + moveY;
        if (CheckCollisionAtAABB(state.x, nextY, params, mapChip)) {
            if (state.vy < 0.0f) {
                state.isGrounded = true;
            }
            state.vy = 0.0f;
        } else {
            state.y = nextY;
        }

        // --- X軸移動と衝突解決 ---
        float moveX = state.vx * FIXED_DELTA_TIME;
        float nextX = state.x + moveX;
        if (CheckCollisionAtAABB(nextX, state.y, params, mapChip)) {
            if (state.vx > 0.0f) {
                state.isTouchingWallRight = true;
            } else if (state.vx < 0.0f) {
                state.isTouchingWallLeft = true;
            }
            state.vx = 0.0f;
        } else {
            state.x = nextX;
        }

        // 接地・壁接触フラグの精密チェック
        state.isGrounded = CheckCollisionAtAABB(state.x, state.y - 0.02f, params, mapChip);
        state.isTouchingWallLeft = CheckCollisionAtAABB(state.x - 0.02f, state.y, params, mapChip);
        state.isTouchingWallRight = CheckCollisionAtAABB(state.x + 0.02f, state.y, params, mapChip);

        // 座標履歴の更新
        state.pathSegment.push_back(Vector3{ state.x, state.y, 0.0f });

        // 死亡判定
        if (state.y < -3.0f) {
            return false;
        }

        if (mapChip) {
            int cx = mapChip->WorldToChipX(state.x);
            int cy = mapChip->WorldToChipY(state.y);
            if (cy < -2 || mapChip->GetChipType(cx, cy) == MapChip2D::ChipType::kDeathBlock) {
                return false;
            }
        }

        // 通常走行中に壁に当たって前進できなくなった場合は早期終了
        if (!isJumpAction && (state.isTouchingWallLeft || state.isTouchingWallRight) && step > 2) {
            break;
        }

        // ジャンプ中、途中で着地した場合はアクション終了
        if (isJumpAction && step > 3 && state.isGrounded) {
            break;
        }
    }

    return !state.pathSegment.empty();
}

bool PhysicsAStar::FindValidPath(
    const Vector3& startPos,
    const Vector3& goalPos,
    MapChip2D* mapChip,
    std::vector<Vector3>& outPathPositions,
    const PlayerParams& params,
    int maxNodes
) {
    outPathPositions.clear();

    // 1. ゴールからの地形最短距離場（Distance Field）を構築
    BuildDistanceField(mapChip, goalPos);

    std::vector<PhysicsState> nodePool;
    nodePool.reserve(maxNodes);

    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, std::greater<std::pair<float, int>>> openSet;
    std::unordered_set<uint64_t> closedSet;

    PhysicsState startState;
    startState.x = startPos.x;
    startState.y = startPos.y;
    startState.vx = 0.0f;
    startState.vy = 0.0f;
    startState.isGrounded = false;
    startState.frameCount = 0;
    startState.parentIndex = -1;

    // 初期位置のスタック回避
    if (mapChip) {
        if (CheckCollisionAtAABB(startState.x, startState.y, params, mapChip)) {
            while (startState.y < 5000.0f && CheckCollisionAtAABB(startState.x, startState.y, params, mapChip)) {
                startState.y += 0.05f;
            }
        }
        startState.isGrounded = CheckCollisionAtAABB(startState.x, startState.y - 0.02f, params, mapChip);
        startState.isTouchingWallLeft = CheckCollisionAtAABB(startState.x - 0.02f, startState.y, params, mapChip);
        startState.isTouchingWallRight = CheckCollisionAtAABB(startState.x + 0.02f, startState.y, params, mapChip);
    } else {
        startState.isGrounded = true;
    }

    startState.heuristic = CalculateHeuristic(startState.x, startState.y, goalPos.x, goalPos.y, params, mapChip);
    startState.pathSegment.push_back(Vector3{ startState.x, startState.y, 0.0f });

    nodePool.push_back(startState);
    openSet.push({ startState.TotalCost(), 0 });

    int goalNodeIndex = -1;
    float minGoalDistSq = 999999.0f;
    int closestNodeIndex = 0;

    while (!openSet.empty() && static_cast<int>(nodePool.size()) < maxNodes) {
        auto [currentCost, currentIndex] = openSet.top();
        openSet.pop();

        const PhysicsState current = nodePool[currentIndex];

        float dx = current.x - goalPos.x;
        float dy = current.y - goalPos.y;
        float distSq = dx * dx + dy * dy;
        if (distSq < minGoalDistSq) {
            minGoalDistSq = distSq;
            closestNodeIndex = currentIndex;
        }

        if (CheckGoalReached(current.x, current.y, params, goalPos, mapChip)) {
            goalNodeIndex = currentIndex;
            break;
        }

        uint64_t hash = EncodeStateHash(current.x, current.y, current.vx, current.vy, current.isGrounded, current.isTouchingWallLeft, current.isTouchingWallRight);
        if (closedSet.find(hash) != closedSet.end()) {
            continue;
        }
        closedSet.insert(hash);

        std::vector<MacroActionType> actions = GetAvailableActions(current);

        for (auto act : actions) {
            PhysicsState nextState = current;
            if (SimulateMacroAction(nextState, act, mapChip, params)) {
                uint64_t nextHash = EncodeStateHash(nextState.x, nextState.y, nextState.vx, nextState.vy, nextState.isGrounded, nextState.isTouchingWallLeft, nextState.isTouchingWallRight);
                if (closedSet.find(nextHash) != closedSet.end()) {
                    continue;
                }

                nextState.frameCount = current.frameCount + static_cast<int>(nextState.pathSegment.size());
                nextState.heuristic = CalculateHeuristic(nextState.x, nextState.y, goalPos.x, goalPos.y, params, mapChip);
                nextState.parentIndex = currentIndex;

                int nextIndex = static_cast<int>(nodePool.size());
                nodePool.push_back(nextState);
                openSet.push({ nextState.TotalCost(), nextIndex });
            }
        }
    }

    int reconstructIndex = (goalNodeIndex != -1) ? goalNodeIndex : closestNodeIndex;

    std::vector<Vector3> reversePath;
    int curr = reconstructIndex;
    while (curr != -1) {
        const auto& seg = nodePool[curr].pathSegment;
        for (auto it = seg.rbegin(); it != seg.rend(); ++it) {
            reversePath.push_back(*it);
        }
        curr = nodePool[curr].parentIndex;
    }

    std::reverse(reversePath.begin(), reversePath.end());
    outPathPositions = std::move(reversePath);

    return (goalNodeIndex != -1);
}
