#pragma once

class Player2D;
class MapChip2D;

/// <summary>
/// プレイヤー状態の基底クラス（ポリモーフィズム用）
/// </summary>
class IPlayerState {
public:
    virtual ~IPlayerState() = default;

    // 状態に入った時の処理
    virtual void Enter(Player2D* player) {}

    // 毎フレームの更新処理
    virtual void Update(Player2D* player, MapChip2D& map, float deltaTime) = 0;

    // 状態から抜ける時の処理
    virtual void Exit(Player2D* player) {}
};
