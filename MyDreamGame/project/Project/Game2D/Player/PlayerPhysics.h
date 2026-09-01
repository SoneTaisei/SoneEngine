#pragma once
#include "Core/Utility/Structs.h"
#include "PlayerState.h"
#include "PlayerConfig.h"
#include "PlayerInput.h"

class Player2D;
class MapChip2D;

class PlayerPhysics {
public:
    PlayerPhysics() = default;
    ~PlayerPhysics() = default;

    void Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime, Player2D* player, MapChip2D* mapChip);

    AABB2D GetAABB(const PlayerState& state_, const PlayerParams& params_) const;
    bool CheckAABBCollision(const AABB2D& a, const AABB2D& b) const;

private:
    void HandleMovement(PlayerState& state_, const PlayerParams& params_, const InputState& input_, float deltaTime, Player2D* player);
    void ApplyGravity(PlayerState& state_, const PlayerParams& params_, float deltaTime);

    void ResolveCollisionX(PlayerState& state_, const PlayerParams& params_, MapChip2D* mapChip, Player2D* player);
    void ResolveCollisionY(PlayerState& state_, const PlayerParams& params_, MapChip2D* mapChip, Player2D* player);
    void CheckBlockInteractions(PlayerState& state_, const PlayerParams& params_, Player2D* player, MapChip2D* mapChip);
};
