#pragma once
#include "IState.h"
#include <memory>
#include <utility>

/// <summary>
/// 汎用ステートマシン（エンジン共通）
/// TOwner: ステートを持つオーナー型
/// </summary>
template <typename TOwner>
class StateMachine {
public:
    StateMachine() = default;
    ~StateMachine() = default;

    // ステートの変更
    void ChangeState(std::unique_ptr<IState<TOwner>> nextState, TOwner* owner) {
        if (currentState_) {
            currentState_->Exit(owner);
        }
        currentState_ = std::move(nextState);
        if (currentState_) {
            currentState_->Enter(owner);
        }
    }

    // 現在のステートの更新
    void Update(TOwner* owner, float deltaTime) {
        if (currentState_) {
            currentState_->Update(owner, deltaTime);
        }
    }

    // 現在のステートの取得
    IState<TOwner>* GetCurrentState() const {
        return currentState_.get();
    }

private:
    std::unique_ptr<IState<TOwner>> currentState_;
};
