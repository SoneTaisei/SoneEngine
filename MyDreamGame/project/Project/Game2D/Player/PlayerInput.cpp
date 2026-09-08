#include "PlayerInput.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include <algorithm>

void PlayerInput::Update(InputState& outState) {
    KeyboardInput* keyboard = KeyboardInput::GetInstance();
    GamepadInput* pad = GamepadInput::GetInstance();

    // 移動入力（X軸）
    outState.moveX = 0.0f;
    if (keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT)) {
        outState.moveX -= 1.0f;
    }
    if (keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT)) {
        outState.moveX += 1.0f;
    }

    // ゲームパッド入力（スティック & D-Pad）
    if (pad && pad->IsConnected()) {
        float stickX = pad->GetLeftStick().x;
        if (std::abs(stickX) > 0.0f) {
            outState.moveX += stickX;
        }
        if (pad->IsDPadLeft()) {
            outState.moveX -= 1.0f;
        }
        if (pad->IsDPadRight()) {
            outState.moveX += 1.0f;
        }
    }

    // -1.0f 〜 1.0f にクランプ
    outState.moveX = std::clamp(outState.moveX, -1.0f, 1.0f);

    // 各種アクションキー（ジャンプ: SPACE / パッド Aボタン）
    bool padJumpPressed = pad && pad->IsButtonPressed(GamepadButton::A);
    bool padJumpHeld = pad && pad->IsButtonDown(GamepadButton::A);

    outState.isJumpPressed = keyboard->IsKeyPressed(DIK_SPACE) || padJumpPressed;
    outState.isJumpHeld = keyboard->IsKeyDown(DIK_SPACE) || padJumpHeld;
}
