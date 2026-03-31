#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <joystickapi.h> // DIJOYSTATE2 のために追加
#include"Core/Utility/Structs.h"

// ライブラリのリンク
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class GamepadInput {
public:
    // シングルトンインスタンスを取得
    static GamepadInput *GetInstance();

    // 初期化
    bool Initialize(HINSTANCE hInstance, HWND hwnd);

    // 更新
    void Update();

    // --- ボタン入力 ---
    // ボタンが押され続けているか
    bool IsButtonDown(int buttonIndex);
    // ボタンが押された瞬間か
    bool IsButtonPressed(int buttonIndex);
    // ボタンが離された瞬間か
    bool IsButtonReleased(int buttonIndex);

    // --- D-Pad入力 ---
    bool IsDPadUp();
    bool IsDPadDown();
    bool IsDPadLeft();
    bool IsDPadRight();

    // --- アナログスティック入力 ---
    // -1.0f ~ 1.0f の範囲で正規化された値を取得
    Vector2 GetLeftStick();
    Vector2 GetRightStick();

private:
    GamepadInput() = default;
    ~GamepadInput() = default;
    GamepadInput(const GamepadInput &) = delete;
    GamepadInput &operator=(const GamepadInput &) = delete;

    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> device_ = nullptr;

    // コントローラーの状態を保持する構造体
    DIJOYSTATE2 state_{};
    DIJOYSTATE2 preState_{};

    // 見つかったコントローラーのGUID
    GUID gamepadGuid_{};
    bool isDeviceFound_ = false;

    // デバイスを検索するためのコールバック関数 (static)
    static BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext);
};
