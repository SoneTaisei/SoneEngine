#pragma once
#include "Core/Utility/Structs.h"

/// <summary>
/// プレイヤーの操作入力状態を表す構造体
/// 特定の入力デバイス（キーボードやパッド）に依存しない論理的な意図を持つ
/// </summary>
struct InputState {
    float moveX = 0.0f; // -1.0f (左) 〜 1.0f (右)
    float moveY = 0.0f; // -1.0f (下) 〜 1.0f (上)
    
    bool isJumpPressed = false;   // ジャンプキーが押された瞬間
    bool isJumpReleased = false;  // ジャンプキーが離された瞬間
    bool isDashPressed = false;   // ダッシュキーが押された瞬間
    bool isClingHeld = false;     // 壁つかみキーが押されている間
};

/// <summary>
/// プレイヤーの入力を監視し、論理的なInputStateに変換するクラス
/// </summary>
class PlayerInput {
public:
    PlayerInput() = default;
    ~PlayerInput() = default;

    /// <summary>
    /// 入力デバイスの状態を読み取り、InputStateを更新する
    /// </summary>
    void Update(InputState& outState);
};
