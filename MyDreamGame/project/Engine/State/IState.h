#pragma once

/// <summary>
/// 汎用ステートパターンの基底クラス（エンジン共通）
/// TOwner: ステートを持つオーナー型（例: Player2D, Enemy など）
/// </summary>
template <typename TOwner>
class IState {
public:
    virtual ~IState() = default;

    // ステート開始時
    virtual void Enter(TOwner* owner) {}

    // 毎フレームの更新処理
    virtual void Update(TOwner* owner, float deltaTime) = 0;

    // ステート終了時
    virtual void Exit(TOwner* owner) {}
};
