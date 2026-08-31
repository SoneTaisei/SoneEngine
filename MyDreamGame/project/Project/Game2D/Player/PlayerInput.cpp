#include "PlayerInput.h"
#include "Input/KeyboardInput.h"

void PlayerInput::Update(InputState& outState) {
    KeyboardInput* keyboard = KeyboardInput::GetInstance();

    // 移動入力（X軸）
    outState.moveX = 0.0f;
    if (keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT)) {
        outState.moveX -= 1.0f;
    }
    if (keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT)) {
        outState.moveX += 1.0f;
    }

    // 各種アクションキー
    outState.isJumpPressed = keyboard->IsKeyPressed(DIK_SPACE);
    outState.isJumpHeld = keyboard->IsKeyDown(DIK_SPACE);
}
