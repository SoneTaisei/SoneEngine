#pragma once
#include "Core/Utility/Structs.h"
#include "PlayerState.h"
#include "PlayerConfig.h"
#include "PlayerInput.h"
#include "PlayerVisuals.h"


class Player2D;

class PlayerPhysics {
public:
    PlayerPhysics() = default;
    ~PlayerPhysics() = default;

    void Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player);

    AABB2D GetAABB(const PlayerState& state_, const PlayerParams& params_) const;
    bool CheckAABBCollision(const AABB2D& a, const AABB2D& b) const;

    void SimulateCollisions(PlayerState& state_, const PlayerParams& params_, Player2D* player);

private:
    void HandleInputLogic(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player);
    void ApplyGravity(PlayerState& state_, const PlayerParams& params_, float deltaTime);

    void ResolveCollisionY(PlayerState& state_, const PlayerParams& params_);
    void ResolveStaticCollisionY(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb);
    void ResolveDynamicCollisionY(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb);

    void ResolveCollisionX(PlayerState& state_, const PlayerParams& params_);
    void ResolveStaticCollisionX(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb);
    void ResolveDynamicCollisionX(PlayerState& state_, const PlayerParams& params_, AABB2D& aabb);
};
