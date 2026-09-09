#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <joystickapi.h> // DIJOYSTATE2 のために追加
#include <Xinput.h>
#include "Core/Utility/Structs.h"

// ライブラリのリンク
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

enum class GamepadButton {
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    LB = 4,
    RB = 5,
    Back = 6,
    Start = 7,
    LStick = 8,
    RStick = 9,
    DPadUp = 10,
    DPadDown = 11,
    DPadLeft = 12,
    DPadRight = 13,
    Count
};

class GamepadInput {
public:
    // シングルトンインスタンスを取得
    static GamepadInput *GetInstance();

    // 初期化
    bool Initialize(HINSTANCE hInstance, HWND hwnd);

    // 更新
    void Update();

    // 接続状態の確認
    bool IsConnected() const;

    // --- ボタン入力 ---
    // ボタンが押され続けているか
    bool IsButtonDown(GamepadButton button);
    // ボタンが押された瞬間か
    bool IsButtonPressed(GamepadButton button);
    // ボタンが離された瞬間か
    bool IsButtonReleased(GamepadButton button);

    // 互換用インデックス指定
    bool IsButtonDown(int buttonIndex);
    bool IsButtonPressed(int buttonIndex);
    bool IsButtonReleased(int buttonIndex);

    // --- D-Pad入力 ---
    bool IsDPadUp();
    bool IsDPadDown();
    bool IsDPadLeft();
    bool IsDPadRight();

    // --- アナログスティック入力 ---
    // -1.0f ~ 1.0f の範囲で正規化された値を取得（デッドゾーン処理済み）
    Vector2 GetLeftStick();
    Vector2 GetRightStick();

    // --- トリガー入力 ---
    // 0.0f ~ 1.0f の範囲で取得
    float GetLeftTrigger();
    float GetRightTrigger();

private:
    GamepadInput() = default;
    ~GamepadInput() = default;
    GamepadInput(const GamepadInput &) = delete;
    GamepadInput &operator=(const GamepadInput &) = delete;

    // XInput関連
    XINPUT_STATE xState_{};
    XINPUT_STATE preXState_{};
    bool isXInputConnected_ = false;

    // DirectInput関連
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> device_ = nullptr;
    DIJOYSTATE2 state_{};
    DIJOYSTATE2 preState_{};
    GUID gamepadGuid_{};
    bool isDeviceFound_ = false;

    // デバイスを検索するためのコールバック関数 (static)
    static BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext);

    WORD GetXInputMask(GamepadButton button) const;
    bool CheckXInputButton(const XINPUT_STATE& state, GamepadButton button) const;
};
