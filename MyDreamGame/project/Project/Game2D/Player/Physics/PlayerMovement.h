#pragma once
#include "../PlayerState.h"
#include "../PlayerConfig.h"
#include "../PlayerInput.h"

/// <summary>
/// プレイヤーのタイマー更新、水平移動、重力・壁登り、足場速度追跡を担当するクラス
/// </summary>
class PlayerMovement {
public:
    void UpdateTimers(PlayerState& state, const PlayerParams& params, const InputState& input, float deltaTime);
    void UpdateHorizontal(PlayerState& state, const PlayerParams& params, const InputState& input, float deltaTime);
    void ApplyGravityAndClimb(PlayerState& state, const PlayerParams& params, const InputState& input, float deltaTime);
    void UpdatePlatformVelocity(PlayerState& state);
};
