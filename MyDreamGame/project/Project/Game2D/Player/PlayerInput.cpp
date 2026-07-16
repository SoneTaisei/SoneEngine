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

    // 移動入力（Y軸）※主にはしごやダッシュ方向指定用
    outState.moveY = 0.0f;
    if (keyboard->IsKeyDown(DIK_S) || keyboard->IsKeyDown(DIK_DOWN)) {
        outState.moveY -= 1.0f;
    }
    if (keyboard->IsKeyDown(DIK_W) || keyboard->IsKeyDown(DIK_UP)) {
        outState.moveY += 1.0f;
    }

    // 各種アクションキー
    outState.isJumpPressed = keyboard->IsKeyPressed(DIK_SPACE);
    outState.isJumpReleased = keyboard->IsKeyReleased(DIK_SPACE);
    outState.isDashPressed = keyboard->IsKeyPressed(DIK_J);
    outState.isClingHeld = keyboard->IsKeyDown(DIK_K);
}
