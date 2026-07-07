#pragma once
#include "Core/Utility/Structs.h"
#include "PlayerState.h"
#include "PlayerConfig.h"
#include "PlayerInput.h"
#include "PlayerVisuals.h"
#include "../MapChip2D.h"

class Player2D;

class PlayerPhysics {
public:
    PlayerPhysics() = default;
    ~PlayerPhysics() = default;

    void Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, MapChip2D& map, PlayerVisuals& visuals_, float deltaTime);

    AABB2D GetAABB(const PlayerState& state_, const PlayerParams& params_) const;
    bool CheckAABBCollision(const AABB2D& a, const AABB2D& b) const;

    void SimulateCollisions(PlayerState& state_, const PlayerParams& params_, MapChip2D& map, Player2D* player);

private:
    void HandleInputLogic(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime);
    void ApplyGravity(PlayerState& state_, const PlayerParams& params_, float deltaTime);

    void ResolveCollisionY(PlayerState& state_, const PlayerParams& params_, const MapChip2D& map);
    void ResolveStaticCollisionY(PlayerState& state_, const PlayerParams& params_, const MapChip2D& map, AABB2D& aabb);
    void ResolveDynamicCollisionY(PlayerState& state_, const PlayerParams& params_, const MapChip2D& map, AABB2D& aabb);

    void ResolveCollisionX(PlayerState& state_, const PlayerParams& params_, const MapChip2D& map);
    void ResolveStaticCollisionX(PlayerState& state_, const PlayerParams& params_, const MapChip2D& map, AABB2D& aabb);
    void ResolveDynamicCollisionX(PlayerState& state_, const PlayerParams& params_, const MapChip2D& map, AABB2D& aabb);
};
