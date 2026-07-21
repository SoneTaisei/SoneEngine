#pragma once
#include "Core/Utility/Structs.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <cstdint>

class MapChip2D;

enum class MacroActionType {
    RunRight = 0,
    RunLeft,
    JumpRight,
    JumpLeft,
    JumpNeutral,
    FallWait,
    DashRight,
    DashLeft,
    Count
};

/// <summary>
/// 物理ベースA*探索におけるノード
/// </summary>
struct PhysicsState {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    bool isGrounded = false;

    int frameCount = 0;       // g(n): スタートからの消費フレーム数
    float heuristic = 0.0f;   // h(n): ゴールまでの推定残数フレーム
    int parentIndex = -1;     // 親ノードのインデックス

    // このアクションで進んだ1フレーム毎の座標履歴（パス復元用）
    std::vector<Vector3> pathSegment;

    float TotalCost() const { return static_cast<float>(frameCount) + heuristic; }

    bool operator>(const PhysicsState& other) const {
        return TotalCost() > other.TotalCost();
    }
};

/// <summary>
/// マクロアクション（操作の塊）と物理演算による超高速A*探索クラス
/// </summary>
class PhysicsAStar {
public:
    PhysicsAStar() = default;
    ~PhysicsAStar() = default;

    /// <summary>
    /// マクロアクション単位で長大ステージのルートを高速探索する
    /// </summary>
    bool FindValidPath(const Vector3& startPos, const Vector3& goalPos, MapChip2D* mapChip, std::vector<Vector3>& outPathPositions, int maxNodes = 10000);

private:
    float CalculateHeuristic(float cx, float cy, float gx, float gy) const;
    uint64_t EncodeStateHash(float x, float y, float vx, float vy, bool isGrounded) const;

    bool SimulateMacroAction(PhysicsState& state, MacroActionType action, MapChip2D* mapChip) const;
    bool CheckCollisionAt(float x, float y, MapChip2D* mapChip) const;

private:
    const float FIXED_DELTA_TIME = 1.0f / 60.0f;
    const float MOVE_SPEED = 5.0f;
    const float JUMP_POWER = 17.0f;
    const float GRAVITY = -40.0f;
    const float MAX_FALL_SPEED = -15.0f;
    const float PLAYER_SIZE = 0.8f;
};
