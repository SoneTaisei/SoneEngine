#include "PhysicsAStar.h"
#include "Game2D/MapChip2D.h"
#include "Core/Utility/LogManager.h"
#include <cmath>
#include <algorithm>
#include <format>
#include <deque>

void PhysicsAStar::BuildDistanceField(MapChip2D* mapChip, const Vector3& goalPos) {
    if (!mapChip) return;

    mapW_ = mapChip->GetWidth();
    mapH_ = mapChip->GetHeight();
    distanceField_.assign(mapH_, std::vector<float>(mapW_, 99999.0f));

    int goalChipX = mapChip->WorldToChipX(goalPos.x);
    int goalChipY = mapChip->WorldToChipY(goalPos.y);

    if (goalChipX < 0 || goalChipX >= mapW_ || goalChipY < 0 || goalChipY >= mapH_) {
        return;
    }

    std::deque<std::pair<int, int>> queue;
    distanceField_[goalChipY][goalChipX] = 0.0f;
    queue.push_back({ goalChipX, goalChipY });

    const int dx[8] = { 1, -1, 0, 0, 1, -1, 1, -1 };
    const int dy[8] = { 0, 0, 1, -1, 1, 1, -1, -1 };
    const float dCost[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.414f, 1.414f, 1.414f, 1.414f };
    float chipSize = mapChip->GetChipSize();

    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop_front();

        float currentDist = distanceField_[cy][cx];

        for (int i = 0; i < 8; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < mapW_ && ny >= 0 && ny < mapH_) {
                MapChip2D::ChipType type = mapChip->GetChipType(nx, ny);
                if (type == MapChip2D::ChipType::kBlock || type == MapChip2D::ChipType::kDeathBlock) {
                    continue; // 壁やトゲは通れない
                }

                float newDist = currentDist + dCost[i] * chipSize;
                if (newDist < distanceField_[ny][nx]) {
                    distanceField_[ny][nx] = newDist;
                    queue.push_back({ nx, ny });
                }
            }
        }
    }
}

float PhysicsAStar::CalculateHeuristic(float cx, float cy, float gx, float gy, const PlayerParams& params, MapChip2D* mapChip) const {
    float dist = 0.0f;

    if (mapChip && !distanceField_.empty()) {
        int chipX = mapChip->WorldToChipX(cx);
        int chipY = mapChip->WorldToChipY(cy);

        if (chipX >= 0 && chipX < mapW_ && chipY >= 0 && chipY < mapH_) {
            float fieldDist = distanceField_[chipY][chipX];
            if (fieldDist < 90000.0f) {
                dist = fieldDist;
            }
        }
    }

    if (dist <= 0.0f) {
        float dx = cx - gx;
        float dy = cy - gy;
        dist = std::sqrt(dx * dx + dy * dy);
    }

    float speed = (params.moveSpeed_ > 0.1f) ? params.moveSpeed_ : 5.0f;
    return dist / (speed * FIXED_DELTA_TIME * 14.0f);
}

uint64_t PhysicsAStar::EncodeStateHash(float x, float y, float vx, float vy, bool isGrounded, bool isWallL, bool isWallR) const {
    // 0.35m 刻みで位置を量子化
    int qx = static_cast<int>(std::round(x * 2.85f));
    int qy = static_cast<int>(std::round(y * 2.85f));
    
    // 速度の量子化
    int qvx = (vx > 0.5f) ? 1 : ((vx < -0.5f) ? -1 : 0);
    int qvy = (vy > 0.5f) ? 1 : ((vy < -0.5f) ? -1 : 0);

    uint64_t hash = 0;
    hash |= (static_cast<uint64_t>(qx & 0xFFFF));
    hash |= (static_cast<uint64_t>(qy & 0xFFFF) << 16);
    hash |= (static_cast<uint64_t>((qvx + 2) & 0x07) << 32);
    hash |= (static_cast<uint64_t>((qvy + 2) & 0x07) << 35);
    hash |= (static_cast<uint64_t>(isGrounded ? 1 : 0) << 38);
    hash |= (static_cast<uint64_t>(isWallL ? 1 : 0) << 39);
    hash |= (static_cast<uint64_t>(isWallR ? 1 : 0) << 40);

    return hash;
}

std::vector<MacroActionType> PhysicsAStar::GetAvailableActions(const PhysicsState& state) const {
    std::vector<MacroActionType> actions;
    actions.reserve(16);

    // 1. 地上（接地中）
    if (state.isGrounded) {
        actions.push_back(MacroActionType::RunRightLong);
        actions.push_back(MacroActionType::RunRight);
        actions.push_back(MacroActionType::JumpRight);
        actions.push_back(MacroActionType::LongJumpRight);
        actions.push_back(MacroActionType::JumpShortRight);

        actions.push_back(MacroActionType::RunLeftLong);
        actions.push_back(MacroActionType::RunLeft);
        actions.push_back(MacroActionType::JumpLeft);
        actions.push_back(MacroActionType::LongJumpLeft);
        actions.push_back(MacroActionType::JumpShortLeft);

        actions.push_back(MacroActionType::JumpNeutral);

        // 谷を一発で飛び越えるコンボ
        if (state.canDash) {
            actions.push_back(MacroActionType::LowJumpDashRight); // 天井が低い場所用の低空ダッシュ
            actions.push_back(MacroActionType::JumpDashRight);
            actions.push_back(MacroActionType::JumpDashUpRight);
            actions.push_back(MacroActionType::DashRight);

            actions.push_back(MacroActionType::LowJumpDashLeft);
            actions.push_back(MacroActionType::JumpDashLeft);
            actions.push_back(MacroActionType::JumpDashUpLeft);
            actions.push_back(MacroActionType::DashLeft);
        }
    }
    // 2. 左壁に接触中
    else if (state.isTouchingWallLeft) {
        actions.push_back(MacroActionType::WallJumpRight);
        if (state.stamina > 10.0f) {
            actions.push_back(MacroActionType::WallClimbUp);
        }
        actions.push_back(MacroActionType::FallWait);
        actions.push_back(MacroActionType::RunRight);

        if (state.canDash) {
            actions.push_back(MacroActionType::DashUpRight);
            actions.push_back(MacroActionType::DashRight);
        }
    }
    // 3. 右壁に接触中
    else if (state.isTouchingWallRight) {
        actions.push_back(MacroActionType::WallJumpLeft);
        if (state.stamina > 10.0f) {
            actions.push_back(MacroActionType::WallClimbUp);
        }
        actions.push_back(MacroActionType::FallWait);
        actions.push_back(MacroActionType::RunLeft);

        if (state.canDash) {
            actions.push_back(MacroActionType::DashUpLeft);
            actions.push_back(MacroActionType::DashLeft);
        }
    }
    // 4. 空中（壁なし）
    else {
        actions.push_back(MacroActionType::RunRight);
        actions.push_back(MacroActionType::RunLeft);
        actions.push_back(MacroActionType::FallWait);

        if (state.canDash) {
            actions.push_back(MacroActionType::DashRight);
            actions.push_back(MacroActionType::DashUpRight);
            actions.push_back(MacroActionType::DashLeft);
            actions.push_back(MacroActionType::DashUpLeft);
            actions.push_back(MacroActionType::DashUp);
        }
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
    // 1. 指定ゴール座標との距離判定 (半径 2.0m)
    float dx = x - goalPos.x;
    float dy = y - goalPos.y;
    if ((dx * dx + dy * dy) < 4.0f) {
        return true;
    }

    // 2. マップ上の kGoal チップとの AABB 接触判定
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

    int targetSteps = 8;
    bool isJumpAction = false;
    bool isShortJump = false;
    bool isDashAction = false;
    bool isWallJump = false;
    bool isWallClimb = false;
    bool isJumpDash = false;
    bool isLowJumpDash = false;

    float inputDirX = 0.0f;
    float dashDirX = 0.0f;
    float dashDirY = 0.0f;

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
    case MacroActionType::LongJumpRight:
        inputDirX = 1.0f;
        isJumpAction = true;
        targetSteps = 38; // 大滞空ジャンプ
        break;
    case MacroActionType::LongJumpLeft:
        inputDirX = -1.0f;
        isJumpAction = true;
        targetSteps = 38;
        break;
    case MacroActionType::JumpDashRight:
        inputDirX = 1.0f;
        dashDirX = 1.0f; dashDirY = 0.0f;
        isJumpAction = true;
        isJumpDash = true;
        targetSteps = 38;
        break;
    case MacroActionType::JumpDashUpRight:
        inputDirX = 1.0f;
        dashDirX = 0.7071f; dashDirY = 0.7071f;
        isJumpAction = true;
        isJumpDash = true;
        targetSteps = 38;
        break;
    case MacroActionType::JumpDashLeft:
        inputDirX = -1.0f;
        dashDirX = -1.0f; dashDirY = 0.0f;
        isJumpAction = true;
        isJumpDash = true;
        targetSteps = 38;
        break;
    case MacroActionType::JumpDashUpLeft:
        inputDirX = -1.0f;
        dashDirX = -0.7071f; dashDirY = 0.7071f;
        isJumpAction = true;
        isJumpDash = true;
        targetSteps = 38;
        break;
    case MacroActionType::LowJumpDashRight:
        inputDirX = 1.0f;
        dashDirX = 1.0f; dashDirY = 0.0f;
        isJumpAction = true;
        isLowJumpDash = true;
        targetSteps = 32;
        break;
    case MacroActionType::LowJumpDashLeft:
        inputDirX = -1.0f;
        dashDirX = -1.0f; dashDirY = 0.0f;
        isJumpAction = true;
        isLowJumpDash = true;
        targetSteps = 32;
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
    case MacroActionType::DashRight:
        dashDirX = 1.0f; dashDirY = 0.0f;
        isDashAction = true;
        targetSteps = 16;
        break;
    case MacroActionType::DashLeft:
        dashDirX = -1.0f; dashDirY = 0.0f;
        isDashAction = true;
        targetSteps = 16;
        break;
    case MacroActionType::DashUpRight:
        dashDirX = 0.7071f; dashDirY = 0.7071f;
        isDashAction = true;
        targetSteps = 16;
        break;
    case MacroActionType::DashUpLeft:
        dashDirX = -0.7071f; dashDirY = 0.7071f;
        isDashAction = true;
        targetSteps = 16;
        break;
    case MacroActionType::DashUp:
        dashDirX = 0.0f; dashDirY = 1.0f;
        isDashAction = true;
        targetSteps = 16;
        break;
    case MacroActionType::WallJumpRight:
        isWallJump = true;
        inputDirX = 1.0f;
        targetSteps = 24;
        break;
    case MacroActionType::WallJumpLeft:
        isWallJump = true;
        inputDirX = -1.0f;
        targetSteps = 24;
        break;
    case MacroActionType::WallClimbUp:
        isWallClimb = true;
        targetSteps = 16;
        break;
    default:
        return false;
    }

    if (targetSteps <= 0) targetSteps = 8;

    // --- 発動条件チェック ---
    if (isJumpAction) {
        if (!state.isGrounded) return false;
        if (isLowJumpDash) {
            state.vy = params.jumpPower_ * 0.5f; // 低空ダッシュ用低ジャンプ
        } else {
            state.vy = isShortJump ? (params.jumpPower_ * 0.65f) : params.jumpPower_;
        }
        state.isGrounded = false;
    }

    if (isJumpDash || isLowJumpDash) {
        if (!state.canDash) return false;
    }

    if (isWallJump) {
        if (inputDirX > 0.0f && !state.isTouchingWallLeft) return false;
        if (inputDirX < 0.0f && !state.isTouchingWallRight) return false;

        if (inputDirX > 0.0f) {
            state.vx = params.wallJumpPower_.x;
            state.vy = params.wallJumpPower_.y;
        } else {
            state.vx = -params.wallJumpPower_.x;
            state.vy = params.wallJumpPower_.y;
        }
        state.isGrounded = false;
        state.isTouchingWallLeft = false;
        state.isTouchingWallRight = false;
    }

    if (isWallClimb) {
        if (!state.isTouchingWallLeft && !state.isTouchingWallRight) return false;
        if (state.stamina <= 10.0f) return false;
    }

    if (isDashAction) {
        if (!state.canDash) return false;
        state.canDash = false;
        state.vx = dashDirX * params.dashSpeed_;
        state.vy = dashDirY * params.dashSpeed_;
    }

    // 地上にいればスタミナとダッシュ回復
    if (state.isGrounded) {
        state.stamina = params.maxStamina_;
        state.canDash = true;
    }

    // 1サブステップごとの物理運動および衝突レスポンス
    for (int step = 0; step < targetSteps; ++step) {
        // 1. 通常ジャンプダッシュ (12ステップ目でダッシュ点火)
        if (isJumpDash) {
            if (step == 12 && state.canDash) {
                state.canDash = false;
                state.vx = dashDirX * params.dashSpeed_;
                state.vy = dashDirY * params.dashSpeed_;
            } else if (step > 12 && step <= 21) {
                // ダッシュ持続中（無重力）
            } else {
                state.vx = inputDirX * params.moveSpeed_;
                state.vy += params.gravity_ * FIXED_DELTA_TIME;
                if (state.vy < params.maxFallSpeed_) state.vy = params.maxFallSpeed_;
            }
        }
        // 2. 低空ジャンプダッシュ (5ステップ目で即ダッシュ点火: 低い天井をくぐる用)
        else if (isLowJumpDash) {
            if (step == 5 && state.canDash) {
                state.canDash = false;
                state.vx = dashDirX * params.dashSpeed_;
                state.vy = dashDirY * params.dashSpeed_;
            } else if (step > 5 && step <= 14) {
                // ダッシュ持続中（無重力）
            } else {
                state.vx = inputDirX * params.moveSpeed_;
                state.vy += params.gravity_ * FIXED_DELTA_TIME;
                if (state.vy < params.maxFallSpeed_) state.vy = params.maxFallSpeed_;
            }
        }
        // 3. 壁登り
        else if (isWallClimb) {
            state.stamina -= params.staminaConsumeClimb_ * FIXED_DELTA_TIME;
            if (state.stamina <= 0.0f) {
                state.stamina = 0.0f;
                return false;
            }
            state.vy = params.wallClimbSpeed_;
            state.vx = 0.0f;
        }
        // 4. 通常移動
        else if (!isDashAction && !isWallJump) {
            state.vx = inputDirX * params.moveSpeed_;
        }

        // 重力適用 (ダッシュ中でない場合)
        if (!state.isGrounded && !isDashAction && !isWallClimb && !isJumpDash && !isLowJumpDash) {
            state.vy += params.gravity_ * FIXED_DELTA_TIME;
            if (state.vy < params.maxFallSpeed_) state.vy = params.maxFallSpeed_;
        }

        // --- Y軸移動と衝突解決 ---
        float moveY = state.vy * FIXED_DELTA_TIME;
        float nextY = state.y + moveY;
        if (CheckCollisionAtAABB(state.x, nextY, params, mapChip)) {
            if (state.vy < 0.0f) {
                state.isGrounded = true;
                state.canDash = true;
                state.stamina = params.maxStamina_;
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

        // 死亡判定（トゲブロック、またはマップ最下層よりさらに下への落下で即死判定）
        if (state.y < -3.0f) {
            return false; // 奈落に落ちた瞬間即座に失敗
        }

        if (mapChip) {
            int cx = mapChip->WorldToChipX(state.x);
            int cy = mapChip->WorldToChipY(state.y);
            if (cy < -2 || mapChip->GetChipType(cx, cy) == MapChip2D::ChipType::kDeathBlock) {
                return false;
            }
        }

        // 通常走行中に壁に当たって前進できなくなった場合は早期終了
        if (!isJumpAction && !isDashAction && !isWallClimb && (state.isTouchingWallLeft || state.isTouchingWallRight) && step > 2) {
            break;
        }

        // ジャンプ中、途中で着地した場合はアクション終了
        if (isJumpAction && step > 3 && state.isGrounded) {
            break;
        }
    }

    // ダッシュ終了時の慣性補正
    if (isDashAction || isJumpDash || isLowJumpDash) {
        if (state.vy > params.dashEndUpwardVelocity_) {
            state.vy = params.dashEndUpwardVelocity_;
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
    startState.canDash = true;
    startState.stamina = params.maxStamina_;
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

        // ゴール判定（距離またはゴールブロックAABB接触）
        if (CheckGoalReached(current.x, current.y, params, goalPos, mapChip)) {
            goalNodeIndex = currentIndex;
            break;
        }

        uint64_t hash = EncodeStateHash(current.x, current.y, current.vx, current.vy, current.isGrounded, current.isTouchingWallLeft, current.isTouchingWallRight);
        if (closedSet.count(hash)) continue;
        closedSet.insert(hash);

        // 状態に応じた有効なアクションのみを展開（分岐数を大幅に削減）
        std::vector<MacroActionType> availableActions = GetAvailableActions(current);

        for (MacroActionType actionType : availableActions) {
            PhysicsState nextState = current;

            if (SimulateMacroAction(nextState, actionType, mapChip, params)) {
                nextState.frameCount = current.frameCount + static_cast<int>(nextState.pathSegment.size());
                nextState.parentIndex = currentIndex;

                uint64_t nextHash = EncodeStateHash(nextState.x, nextState.y, nextState.vx, nextState.vy, nextState.isGrounded, nextState.isTouchingWallLeft, nextState.isTouchingWallRight);
                if (!closedSet.count(nextHash)) {
                    nextState.heuristic = CalculateHeuristic(nextState.x, nextState.y, goalPos.x, goalPos.y, params, mapChip);
                    int nextIndex = static_cast<int>(nodePool.size());
                    nodePool.push_back(nextState);
                    openSet.push({ nextState.TotalCost(), nextIndex });
                }
            }
        }
    }

    if (goalNodeIndex != -1) {
        std::vector<Vector3> fullPath;
        int curr = goalNodeIndex;
        while (curr != -1) {
            const auto& seg = nodePool[curr].pathSegment;
            for (auto it = seg.rbegin(); it != seg.rend(); ++it) {
                fullPath.push_back(*it);
            }
            curr = nodePool[curr].parentIndex;
        }
        std::reverse(fullPath.begin(), fullPath.end());
        outPathPositions = fullPath;

        std::string successMsg = std::format(
            "[PhysicsAStar] 高速ルート探索成功！ (推定時間: {:.2f}秒 / 生成ノード数: {})",
            outPathPositions.size() / 60.0f, nodePool.size()
        );
        LogManager::GetInstance()->AddLog(LogLevel::Info, successMsg);
        return true;
    } else {
        float closestDist = std::sqrt(minGoalDistSq);
        std::string reason = (static_cast<int>(nodePool.size()) >= maxNodes) 
            ? "探索ノード数の上限(" + std::to_string(maxNodes) + ")に到達" 
            : "すべての移動ルートが手詰まり/死 (" + std::to_string(nodePool.size()) + " ノード)";

        std::string debugMsg = std::format(
            "[PhysicsAStar 詳細] ゴール到達不可\n"
            "  - 原因: {}\n"
            "  - 最接近座標: X:{:.2f}, Y:{:.2f} (ゴールまで残り {:.2f} m)\n"
            "  - 検索生成ノード数: {} / {}",
            reason, nodePool[closestNodeIndex].x, nodePool[closestNodeIndex].y,
            closestDist, nodePool.size(), maxNodes
        );
        LogManager::GetInstance()->AddLog(LogLevel::Warning, debugMsg);
    }

    return false;
}
