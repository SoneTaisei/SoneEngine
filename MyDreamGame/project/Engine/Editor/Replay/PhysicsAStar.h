#pragma once
#include "Core/Utility/Structs.h"
#include "Game2D/Player/PlayerConfig.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <cstdint>

class MapChip2D;

enum class MacroActionType {
    RunRight = 0,
    RunRightLong,
    RunLeft,
    RunLeftLong,
    JumpRight,
    JumpLeft,
    JumpNeutral,
    JumpShortRight,
    JumpShortLeft,
    LongJumpRight,
    LongJumpLeft,
    JumpDashRight,
    JumpDashUpRight,
    JumpDashLeft,
    JumpDashUpLeft,
    LowJumpDashRight,
    LowJumpDashLeft,
    FallWait,
    DashRight,
    DashLeft,
    DashUpRight,
    DashUpLeft,
    DashUp,
    WallJumpRight,
    WallJumpLeft,
    WallClimbUp,
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
    bool isTouchingWallLeft = false;
    bool isTouchingWallRight = false;
    bool canDash = true;
    float stamina = 110.0f;

    int frameCount = 0;       // g(n): スタートからの消費フレーム数
    float heuristic = 0.0f;   // h(n): ゴールまでの推定残数フレーム
    int parentIndex = -1;     // 親ノードのインデックス

    // このアクションで進んだ1フレーム毎の座標履歴（パス復元用）
    std::vector<Vector3> pathSegment;

    // 長大ステージでゴール方向へ一直線に進む Weighted A* コスト評価
    float TotalCost() const { return (static_cast<float>(frameCount) * 0.15f) + (heuristic * 2.8f); }

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
    bool FindValidPath(
        const Vector3& startPos,
        const Vector3& goalPos,
        MapChip2D* mapChip,
        std::vector<Vector3>& outPathPositions,
        const PlayerParams& params = PlayerParams{},
        int maxNodes = 30000
    );

private:
    void BuildDistanceField(MapChip2D* mapChip, const Vector3& goalPos);
    float CalculateHeuristic(float cx, float cy, float gx, float gy, const PlayerParams& params, MapChip2D* mapChip) const;
    uint64_t EncodeStateHash(float x, float y, float vx, float vy, bool isGrounded, bool isWallL, bool isWallR) const;

    std::vector<MacroActionType> GetAvailableActions(const PhysicsState& state) const;
    bool SimulateMacroAction(PhysicsState& state, MacroActionType action, MapChip2D* mapChip, const PlayerParams& params) const;
    bool CheckCollisionAtAABB(float x, float y, const PlayerParams& params, MapChip2D* mapChip) const;
    bool CheckGoalReached(float x, float y, const PlayerParams& params, const Vector3& goalPos, MapChip2D* mapChip) const;

private:
    const float FIXED_DELTA_TIME = 1.0f / 60.0f;
    std::vector<std::vector<float>> distanceField_;
    int mapW_ = 0;
    int mapH_ = 0;
};


