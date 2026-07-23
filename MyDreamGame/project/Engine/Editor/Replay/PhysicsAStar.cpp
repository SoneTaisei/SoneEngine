#include "PhysicsAStar.h"
#include "Game2D/MapChip2D.h"
#include "Core/Utility/LogManager.h"
#include <cmath>
#include <algorithm>
#include <format>

float PhysicsAStar::CalculateHeuristic(float cx, float cy, float gx, float gy) const {
    float dx = cx - gx;
    float dy = cy - gy;
    float dist = std::sqrt(dx * dx + dy * dy);
    return dist / (MOVE_SPEED * FIXED_DELTA_TIME * 8.0f); // 8フレームのアクション単位で距離を見積もり
}

uint64_t PhysicsAStar::EncodeStateHash(float x, float y, float vx, float vy, bool isGrounded) const {
    // 0.6m 刻みで量子化して重複展開を防ぐ
    int qx = static_cast<int>(std::round(x * 1.6f));
    int qy = static_cast<int>(std::round(y * 1.6f));
    int qvx = (vx > 0.1f) ? 1 : ((vx < -0.1f) ? -1 : 0);
    int qvy = (vy > 0.1f) ? 1 : ((vy < -0.1f) ? -1 : 0);
    int qg = isGrounded ? 1 : 0;

    uint64_t hash = 0;
    hash |= (static_cast<uint64_t>(qx & 0xFFFF));
    hash |= (static_cast<uint64_t>(qy & 0xFFFF) << 16);
    hash |= (static_cast<uint64_t>((qvx + 2) & 0x07) << 32);
    hash |= (static_cast<uint64_t>((qvy + 2) & 0x07) << 35);
    hash |= (static_cast<uint64_t>(qg & 0x01) << 38);

    return hash;
}

bool PhysicsAStar::CheckCollisionAt(float x, float y, MapChip2D* mapChip) const {
    if (!mapChip) return false;

    float halfW = PLAYER_SIZE * 0.35f;
    float halfH = PLAYER_SIZE * 0.40f;
    float corners[4][2] = {
        { x - halfW, y - halfH },
        { x + halfW, y - halfH },
        { x - halfW, y + halfH },
        { x + halfW, y + halfH }
    };

    for (int i = 0; i < 4; ++i) {
        int cx = mapChip->WorldToChipX(corners[i][0]);
        int cy = mapChip->WorldToChipY(corners[i][1]);

        MapChip2D::ChipType type = mapChip->GetChipType(cx, cy);
        if (type == MapChip2D::ChipType::kBlock || type == MapChip2D::ChipType::kDeathBlock) {
            return true;
        }
    }
    return false;
}

bool PhysicsAStar::SimulateMacroAction(PhysicsState& state, MacroActionType action, MapChip2D* mapChip) const {
    state.pathSegment.clear();

    int targetSteps = 8;
    bool isJumpAction = false;
    bool isDashAction = false;
    float inputDirX = 0.0f;

    switch (action) {
    case MacroActionType::RunRight:
        inputDirX = 1.0f;
        targetSteps = 8;
        break;
    case MacroActionType::RunLeft:
        inputDirX = -1.0f;
        targetSteps = 8;
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
    case MacroActionType::FallWait:
        inputDirX = 0.0f;
        targetSteps = 15;
        break;
    case MacroActionType::DashRight:
        inputDirX = 1.0f;
        isDashAction = true;
        targetSteps = 8;
        break;
    case MacroActionType::DashLeft:
        inputDirX = -1.0f;
        isDashAction = true;
        targetSteps = 8;
        break;
    default:
        return false;
    }

    // ジャンプアクション発動（接地している場合）
    if (isJumpAction) {
        if (!state.isGrounded) return false; // 空中ではジャンプ不可
        state.vy = JUMP_POWER;
        state.isGrounded = false;
    }

    for (int step = 0; step < targetSteps; ++step) {
        if (isDashAction) {
            state.vx = inputDirX * MOVE_SPEED * 2.2f;
        } else {
            state.vx = inputDirX * MOVE_SPEED;
        }

        // 重力適用
        if (!state.isGrounded && !isDashAction) {
            state.vy += GRAVITY * FIXED_DELTA_TIME;
            if (state.vy < MAX_FALL_SPEED) state.vy = MAX_FALL_SPEED;
        }

        // X軸移動＆判定
        float moveX = state.vx * FIXED_DELTA_TIME;
        float nextX = state.x + moveX;
        if (CheckCollisionAt(nextX, state.y, mapChip)) {
            state.vx = 0.0f;
        } else {
            state.x = nextX;
        }

        // Y軸移動＆判定
        float moveY = state.vy * FIXED_DELTA_TIME;
        float nextY = state.y + moveY;
        if (CheckCollisionAt(state.x, nextY, mapChip)) {
            if (state.vy < 0.0f) {
                state.isGrounded = true;
            }
            state.vy = 0.0f;
        } else {
            state.y = nextY;
            if (!CheckCollisionAt(state.x, state.y - 0.1f, mapChip)) {
                state.isGrounded = false;
            }
        }

        // 座標を追加
        state.pathSegment.push_back(Vector3{ state.x, state.y, 0.0f });

        // 死亡判定
        if (mapChip) {
            int cx = mapChip->WorldToChipX(state.x);
            int cy = mapChip->WorldToChipY(state.y);
            if (mapChip->GetChipType(cx, cy) == MapChip2D::ChipType::kDeathBlock || state.y < -50.0f) {
                return false;
            }
        }

        // ジャンプアクション中に途中で着地した場合はアクションをその場で終了
        if (isJumpAction && step > 3 && state.isGrounded) {
            break;
        }
    }

    return !state.pathSegment.empty();
}

bool PhysicsAStar::FindValidPath(const Vector3& startPos, const Vector3& goalPos, MapChip2D* mapChip, std::vector<Vector3>& outPathPositions, int maxNodes) {
    outPathPositions.clear();

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

    // 初期位置が空中だった場合、下方に着地させて空中浮遊を防止する
    if (mapChip) {
        // 既に地形に埋まっている場合は少し上に逃がす
        if (CheckCollisionAt(startState.x, startState.y, mapChip)) {
            while (startState.y < 5000.0f && CheckCollisionAt(startState.x, startState.y, mapChip)) {
                startState.y += 0.1f;
            }
        }

        // 下方の地面を探索
        float testY = startState.y;
        float groundY = -999.0f;
        const float step = 0.02f; // 2cm刻みで高精度に地面を探す
        const float maxDist = 30.0f; // 最大30m下まで
        for (float dist = 0.0f; dist < maxDist; dist += step) {
            float checkY = testY - dist;
            if (CheckCollisionAt(startState.x, checkY, mapChip)) {
                groundY = checkY + step; // 衝突する直前の安全な接地座標
                break;
            }
        }

        if (groundY > -900.0f) {
            startState.y = groundY;
            startState.isGrounded = true;
        }
    } else {
        startState.isGrounded = true;
    }

    startState.heuristic = CalculateHeuristic(startState.x, startState.y, goalPos.x, goalPos.y);
    // 補正後の座標をゴースト及び軌跡の最初の点として登録する
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

        // ゴール接近判定 (半径 0.8 unit)
        if (distSq < 0.64f) {
            goalNodeIndex = currentIndex;
            break;
        }

        uint64_t hash = EncodeStateHash(current.x, current.y, current.vx, current.vy, current.isGrounded);
        if (closedSet.count(hash)) continue;
        closedSet.insert(hash);

        // 各種マクロアクションのブ展开
        for (int a = 0; a < static_cast<int>(MacroActionType::Count); ++a) {
            PhysicsState nextState = current;
            MacroActionType actionType = static_cast<MacroActionType>(a);

            if (SimulateMacroAction(nextState, actionType, mapChip)) {
                nextState.frameCount = current.frameCount + static_cast<int>(nextState.pathSegment.size());
                nextState.parentIndex = currentIndex;

                uint64_t nextHash = EncodeStateHash(nextState.x, nextState.y, nextState.vx, nextState.vy, nextState.isGrounded);
                if (!closedSet.count(nextHash)) {
                    nextState.heuristic = CalculateHeuristic(nextState.x, nextState.y, goalPos.x, goalPos.y);
                    int nextIndex = static_cast<int>(nodePool.size());
                    nodePool.push_back(nextState);
                    openSet.push({ nextState.TotalCost(), nextIndex });
                }
            }
        }
    }

    if (goalNodeIndex != -1) {
        // 親を辿って全中間フレーム座標を復元
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
            "[PhysicsAStar] マクロA*ルート探索成功！ (必要時間: {:.2f}秒 / 生成ノード数: {})",
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
