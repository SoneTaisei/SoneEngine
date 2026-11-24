#pragma once
#include "State/IState.h"

class GameScene;

// GameScene用ステートの共通基底クラス（IState<GameScene> を継承）
class IGameSceneState : public IState<GameScene> {
public:
    virtual ~IGameSceneState() = default;

    // 状態ごとの2D UI描画（StartReady / Pause / Clear 等）
    virtual void Draw2D(GameScene *owner) {}

    // この状態中、ゲーム本体を止めるか（Pauseならtrue）
    virtual bool IsPaused() const { return false; }

    // この状態中、プレイヤーが操作できるか
    virtual bool CanControl(GameScene *owner) const { return true; }

    // クリア状態か（カメラ追従解除の判定用）
    virtual bool IsClear() const { return false; }
};

// --- 4つの具体的なステート ---
class GameStartReadyState : public IGameSceneState {
public:
    void Enter(GameScene *owner) override;
    void Update(GameScene *owner, float deltaTime) override;
    void Exit(GameScene *owner) override;
    void Draw2D(GameScene *owner) override;
    bool CanControl(GameScene *owner) const override;
};

class GamePlayingState : public IGameSceneState {
public:
    void Update(GameScene *owner, float deltaTime) override;
};

class GamePauseState : public IGameSceneState {
public:
    void Enter(GameScene *owner) override;
    void Update(GameScene *owner, float deltaTime) override;
    void Draw2D(GameScene *owner) override;
    bool IsPaused() const override { return true; }
};

class GameClearState : public IGameSceneState {
public:
    void Enter(GameScene *owner) override;
    void Update(GameScene *owner, float deltaTime) override;
    void Exit(GameScene *owner) override;
    void Draw2D(GameScene *owner) override;
    bool CanControl(GameScene *owner) const override { return false; }
    bool IsClear() const override { return true; }
};