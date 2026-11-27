#pragma once
#include "Core/Utility/Structs.h"
#include "PlayerState.h"
#include "PlayerConfig.h"
#include "PlayerInput.h"
#include "PlayerVisuals.h"
#include "Physics/PlayerMovement.h"
#include "Physics/PlayerJump.h"
#include "Physics/PlayerDash.h"
#include "Physics/PlayerCollisionSolver.h"

class Player2D;

/// <summary>
/// プレイヤーの物理更新全体を統括するファサードクラス
/// 移動・ジャンプ・ダッシュ・衝突解決の各クラスを連携させる
/// </summary>
class PlayerPhysics {
public:
    using HitPart = PlayerCollisionSolver::HitPart;

    PlayerPhysics() = default;
    ~PlayerPhysics() = default;

    void Update(PlayerState& state_, const PlayerParams& params_, const InputState& input_, PlayerVisuals& visuals_, float deltaTime, Player2D* player);

    AABB2D GetAABB(const PlayerState& state_, const PlayerParams& params_) const;
    AABB2D GetBodyAABB(const PlayerState& state_, const PlayerParams& params_) const;
    AABB2D GetHeadAABB(const PlayerState& state_, const PlayerParams& params_) const;

    void SimulateCollisions(PlayerState& state_, const PlayerParams& params_, Player2D* player);

private:
    PlayerMovement movement_;
    PlayerJump jump_;
    PlayerDash dash_;
    PlayerCollisionSolver collisionSolver_;
};
