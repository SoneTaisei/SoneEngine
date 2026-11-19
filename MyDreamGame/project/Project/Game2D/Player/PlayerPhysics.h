#pragma once
#include "Core/Utility/Structs.h"
#include "PlayerState.h"
#include "PlayerConfig.h"
#include "PlayerInput.h"
#include "PlayerVisuals.h"


#include <array>

class Player2D;

class PlayerPhysics {
public:
    struct HitPart {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float halfWidth = 0.4f;
        float halfHeight = 0.4f;
        bool allowWallTouch = false;
    };

    PlayerPhysics() = default;
    ~PlayerPhysics() = default;

    void Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player);

    AABB2D GetAABB(const PlayerState& state_, const PlayerParams& params_) const;
    AABB2D GetBodyAABB(const PlayerState& state_, const PlayerParams& params_) const;
    AABB2D GetHeadAABB(const PlayerState& state_, const PlayerParams& params_) const;
    bool CheckAABBCollision(const AABB2D& a, const AABB2D& b) const;

    void SimulateCollisions(PlayerState& state_, const PlayerParams& params_, Player2D* player);

private:
    std::array<HitPart, 2> BuildHitParts(const PlayerState& state_, const PlayerParams& params_, bool usePreviousWallTouch = false) const;
    static AABB2D MakePartAABB(const Vector3& position, const HitPart& part);

    void HandleInputLogic(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player);
    void ApplyGravity(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime);

    void ResolveCollisionY(PlayerState& state_, const PlayerParams& params_, float prevPosY, float moveDeltaY);
    void ResolveStaticCollisionY(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset = 0.0f);
    void ResolveDynamicCollisionY(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosY, float moveDeltaY, float prevBottomOffset = 0.0f);

    void ResolveCollisionX(PlayerState& state_, const PlayerParams& params_, float prevPosX, float moveDeltaX);
    void ResolveStaticCollisionX(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosX, float moveDeltaX);
    void ResolveDynamicCollisionX(PlayerState& state_, const PlayerParams& params_, const HitPart& part, float prevPosX, float moveDeltaX);
};
