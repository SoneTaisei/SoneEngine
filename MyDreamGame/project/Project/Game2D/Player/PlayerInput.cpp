#include "PlayerInput.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include <algorithm>
#include <cmath>

void PlayerInput::Update(InputState& outState) {
    KeyboardInput* keyboard = KeyboardInput::GetInstance();
    GamepadInput* gamepad = GamepadInput::GetInstance();

    // 移動入力（X軸）
    float keyMoveX = 0.0f;
    if (keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT)) {
        keyMoveX -= 1.0f;
    }
    if (keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT)) {
        keyMoveX += 1.0f;
    }

    float padMoveX = 0.0f;
    if (gamepad->IsDPadLeft()) {
        padMoveX -= 1.0f;
    }
    if (gamepad->IsDPadRight()) {
        padMoveX += 1.0f;
    }
    Vector2 stick = gamepad->GetLeftStick();
    if (std::abs(stick.x) > 0.05f) {
        padMoveX += stick.x;
    }

    outState.moveX = std::clamp(keyMoveX + padMoveX, -1.0f, 1.0f);

    // 移動入力（Y軸）※主にはしごやダッシュ方向指定用
    float keyMoveY = 0.0f;
    if (keyboard->IsKeyDown(DIK_S) || keyboard->IsKeyDown(DIK_DOWN)) {
        keyMoveY -= 1.0f;
    }
    if (keyboard->IsKeyDown(DIK_W) || keyboard->IsKeyDown(DIK_UP)) {
        keyMoveY += 1.0f;
    }

    float padMoveY = 0.0f;
    if (gamepad->IsDPadDown()) {
        padMoveY -= 1.0f;
    }
    if (gamepad->IsDPadUp()) {
        padMoveY += 1.0f;
    }
    if (std::abs(stick.y) > 0.05f) {
        padMoveY += stick.y;
    }

    outState.moveY = std::clamp(keyMoveY + padMoveY, -1.0f, 1.0f);

    // ジャンプ（キーボード SPACE / パッド Aボタン）
    outState.isJumpPressed = keyboard->IsKeyPressed(DIK_SPACE) || gamepad->IsButtonPressed(GamepadButton::A);
    outState.isJumpReleased = keyboard->IsKeyReleased(DIK_SPACE) || gamepad->IsButtonReleased(GamepadButton::A);

    // ダッシュ（キーボード J / パッド Xボタン）
    outState.isDashPressed = keyboard->IsKeyPressed(DIK_J) || gamepad->IsButtonPressed(GamepadButton::X);

    // 崖つかまり・壁つかみ（キーボード K / パッド RB または RT）
    bool isClingPad = gamepad->IsButtonDown(GamepadButton::RB) || (gamepad->GetRightTrigger() > 0.3f);
    outState.isClingHeld = keyboard->IsKeyDown(DIK_K) || isClingPad;
}

