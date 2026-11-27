#pragma once
#include "../PlayerState.h"
#include "../PlayerConfig.h"
#include "../PlayerInput.h"
#include "../PlayerVisuals.h"

/// <summary>
/// プレイヤーのジャンプ全般（地上・スーパー・壁キック・可変ジャンプ・足場慣性）を担当するクラス
/// </summary>
class PlayerJump {
public:
    void HandleDashCancelJump(PlayerState& state, const PlayerParams& params, PlayerVisuals& visuals);
    void HandleJump(PlayerState& state, const PlayerParams& params, const InputState& input, PlayerVisuals& visuals);

private:
    void BeginJump(PlayerState& state, const PlayerParams& params);
    void ApplyPlatformInertia(PlayerState& state);
    void ExecuteWallJump(PlayerState& state, const PlayerParams& params, PlayerVisuals& visuals, float wallSide, bool wantWallKick, bool cancelDash);
};
