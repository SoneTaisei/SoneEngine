#pragma once
#include "IPlayerState.h"

/// <summary>
/// プレイヤー通常状態（移動・ジャンプ・ダッシュ・壁登り等の操作可能）
/// </summary>
class PlayerNormalState : public IPlayerState {
public:
    PlayerNormalState() = default;
    ~PlayerNormalState() override = default;

    void Enter(Player2D* player) override;
    void Update(Player2D* player, MapChip2D& map, float deltaTime) override;
    void Exit(Player2D* player) override;
};

/// <summary>
/// プレイヤー死亡演出状態（スローモーション・ノックバック・ディゾルブ演出）
/// </summary>
class PlayerDeadState : public IPlayerState {
public:
    explicit PlayerDeadState(bool isFallDeath = false) : isFallDeath_(isFallDeath) {}
    ~PlayerDeadState() override = default;

    void Enter(Player2D* player) override;
    void Update(Player2D* player, MapChip2D& map, float deltaTime) override;
    void Exit(Player2D* player) override;

private:
    bool isFallDeath_ = false;
};

/// <summary>
/// プレイヤーリスポーン演出状態（ポップアップ拡大演出）
/// </summary>
class PlayerRespawnState : public IPlayerState {
public:
    PlayerRespawnState() = default;
    ~PlayerRespawnState() override = default;

    void Enter(Player2D* player) override;
    void Update(Player2D* player, MapChip2D& map, float deltaTime) override;
    void Exit(Player2D* player) override;
};

/// <summary>
/// プレイヤーゴール演出状態（停止・紙吹雪演出）
/// </summary>
class PlayerGoalState : public IPlayerState {
public:
    PlayerGoalState() = default;
    ~PlayerGoalState() override = default;

    void Enter(Player2D* player) override;
    void Update(Player2D* player, MapChip2D& map, float deltaTime) override;
    void Exit(Player2D* player) override;
};
