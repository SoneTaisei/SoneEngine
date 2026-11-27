#pragma once
#include "../PlayerState.h"
#include "../PlayerConfig.h"
#include "../PlayerInput.h"
#include "../PlayerVisuals.h"

class Player2D;

/// <summary>
/// プレイヤーのダッシュ開始判定・方向計算・継続タイマー更新を担当するクラス
/// </summary>
class PlayerDash {
public:
    void TryStartDash(PlayerState& state, const PlayerParams& params, const InputState& input, PlayerVisuals& visuals, Player2D* player);
    void UpdateDashTimer(PlayerState& state, const PlayerParams& params, float deltaTime);
};
