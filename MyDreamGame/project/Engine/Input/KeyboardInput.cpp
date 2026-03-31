#include "KeyboardInput.h"
#include <cassert> // assertを使うためにインクルード

// GetInstance関数の実体
KeyboardInput *KeyboardInput::GetInstance() {
    static KeyboardInput instance;
    return &instance;
}

// 初期化処理
bool KeyboardInput::Initialize(HINSTANCE hInstance, HWND hwnd) {
    HRESULT result;

    // DirectInputの生成
    result = DirectInput8Create(
        hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
        (void **)&directInput_, nullptr);
    if(FAILED(result)) {
        return false;
    }

    // キーボードデバイスの生成
    result = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
    if(FAILED(result)) {
        return false;
    }

    // 入力データ形式のセット
    result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    if(FAILED(result)) {
        return false;
    }

    // 排他制御レベルのセット
    result = keyboard_->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    if(FAILED(result)) {
        return false;
    }

    return true; // すべて成功したらtrueを返す
}

// 更新処理
void KeyboardInput::Update() {
    // 前フレームのキー状態を現在の状態としてコピー
    memcpy(preKeys_, keys_, sizeof(keys_));

    // キーボードデバイスから現在の状態を取得
    keyboard_->Acquire();
    keyboard_->GetDeviceState(sizeof(keys_), keys_);
}

// キーが押されているか
bool KeyboardInput::IsKeyDown(BYTE keyCode) {
    // 0x80が立っていれば押されている
    return keys_[keyCode] & 0x80;
}

// キーが押された瞬間か
bool KeyboardInput::IsKeyPressed(BYTE keyCode) {
    // 現在は押されていて、前フレームでは押されていない
    return (keys_[keyCode] & 0x80) && !(preKeys_[keyCode] & 0x80);
}

// キーが離された瞬間か
bool KeyboardInput::IsKeyReleased(BYTE keyCode) {
    // 現在は押されておらず、前フレームでは押されている
    return !(keys_[keyCode] & 0x80) && (preKeys_[keyCode] & 0x80);
}