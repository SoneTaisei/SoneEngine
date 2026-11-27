#pragma once
#include "Core/Utility/Structs.h"
#include "../PlayerState.h"
#include "../PlayerConfig.h"
#include <array>

class Player2D;
class ColliderComponent;

/// <summary>
/// プレイヤーの当たり判定（胴体/頭AABB生成、Y/X軸の静的・動的ブロック衝突解決、トリガー発火）を担当するクラス
/// </summary>
class PlayerCollisionSolver {
public:
    struct HitPart {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float halfWidth = 0.4f;
        float halfHeight = 0.4f;
        bool allowWallTouch = false;
        bool allowOneWayStand = false;
    };

    void ResolveCollisionY(PlayerState& state, const PlayerParams& params, float prevPosY, float moveDeltaY);
    void ResolveCollisionX(PlayerState& state, const PlayerParams& params, float prevPosX, float moveDeltaX, float deltaTime);
    void SimulateCollisions(PlayerState& state, const PlayerParams& params, Player2D* player);

    AABB2D GetAABB(const PlayerState& state, const PlayerParams& params) const;
    AABB2D GetBodyAABB(const PlayerState& state, const PlayerParams& params) const;
    AABB2D GetHeadAABB(const PlayerState& state, const PlayerParams& params) const;

private:
    std::array<HitPart, 2> BuildHitParts(const PlayerState& state, const PlayerParams& params, bool usePreviousWallTouch = false) const;
    static AABB2D MakePartAABB(const Vector3& position, const HitPart& part);

    void ResolveStaticCollisionY(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset = 0.0f);
    void ResolveDynamicCollisionY(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset = 0.0f);

    void ResolveStaticCollisionX(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosX, float moveDeltaX);
    void ResolveDynamicCollisionX(PlayerState& state, const PlayerParams& params, const HitPart& part, float prevPosX, float moveDeltaX, float deltaTime);

    void ApplyLanding(PlayerState& state, const PlayerParams& params, const HitPart& part, const AABB2D& blockAABB, ColliderComponent* collider);
    void ApplyCeilingHit(PlayerState& state, const HitPart& part, const AABB2D& blockAABB);
    void ResetHorizontalOnWallHit(PlayerState& state, bool clearDashInertia);
};
