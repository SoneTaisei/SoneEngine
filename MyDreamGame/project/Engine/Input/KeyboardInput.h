#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>

// ライブラリのリンクを自動化
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class KeyboardInput {
public:
    // シングルトンインスタンスを取得するための関数
    static KeyboardInput *GetInstance();

    // 初期化処理
    bool Initialize(HINSTANCE hInstance, HWND hwnd);

    // 毎フレームの更新処理
    void Update();

    // キーが押され続けているかを取得
    bool IsKeyDown(BYTE keyCode);

    // キーが押された瞬間かを取得 (トリガー)
    bool IsKeyPressed(BYTE keyCode);

    // キーが離された瞬間かを取得
    bool IsKeyReleased(BYTE keyCode);

private:
    // シングルトンにするため、コンストラクタなどをprivateにする
    KeyboardInput() = default;
    ~KeyboardInput() = default;
    KeyboardInput(const KeyboardInput &) = delete;
    KeyboardInput &operator=(const KeyboardInput &) = delete;

    // DirectInputのCOMオブジェクト
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_ = nullptr;

    // 現在と前フレームのキー入力状態
    BYTE keys_[256] = {};
    BYTE preKeys_[256] = {};
};

