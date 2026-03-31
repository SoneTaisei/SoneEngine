#define DIRECTINPUT_VERSION 0x0800
#include "GamepadInput.h"

GamepadInput *GamepadInput::GetInstance() {
	static GamepadInput instance;
	return &instance;
}

// デバイスを列挙するためのコールバック関数
BOOL CALLBACK GamepadInput::EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext) {
	GamepadInput *self = static_cast<GamepadInput *>(pContext);

	// 見つかったデバイスのGUIDを保存
	self->gamepadGuid_ = pdidInstance->guidInstance;
	self->isDeviceFound_ = true;

	// 最初の1つが見つかったら列挙を停止
	return DIENUM_STOP;
}

bool GamepadInput::Initialize(HINSTANCE hInstance, HWND hwnd) {
	HRESULT result;

	// DirectInputの生成
	result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void **)&directInput_, nullptr);
	if(FAILED(result)) {
		return false;
	}

	// ゲームパッドデバイスを列挙
	result = directInput_->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, this, DIEDFL_ATTACHEDONLY);
	if(FAILED(result) || !isDeviceFound_) {
		return false; // それでも見つからなければ失敗
	}

// デバイスの生成
	result = directInput_->CreateDevice(gamepadGuid_, &device_, NULL);
	if(FAILED(result)) {
		return false;
	}

	// 入力データ形式のセット (c_dfDIJoystick2 を使用)
	result = device_->SetDataFormat(&c_dfDIJoystick2);
	if(FAILED(result)) {
		return false;
	}

	// 協調レベルのセット
	result = device_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if(FAILED(result)) {
		return false;
	}

	// 軸の範囲設定 (-1000 ~ 1000 にする)
	DIPROPRANGE propRange;
	propRange.diph.dwSize = sizeof(DIPROPRANGE);
	propRange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	propRange.diph.dwHow = DIPH_BYOFFSET;
	propRange.diph.dwObj = DIJOFS_X; // X軸
	propRange.lMin = -1000;
	propRange.lMax = 1000;
	device_->SetProperty(DIPROP_RANGE, &propRange.diph);
	// Y, Z, RZ軸なども同様に設定... (ここではX軸のみ例示)
	propRange.diph.dwObj = DIJOFS_Y; // Y軸
	device_->SetProperty(DIPROP_RANGE, &propRange.diph);


	return true;
}

void GamepadInput::Update() {
	if(!device_) {
		return;
	}

	// 前フレームの状態を保存
	preState_ = state_;

	// デバイスをポーリング (キーボードと違う重要な処理)
	HRESULT hr = device_->Poll();
	// デバイスがロストしていたら再取得を試みる
	if(FAILED(hr)) {
		hr = device_->Acquire();
		if(FAILED(hr)) {
			return; // 取得できなければ何もしない
		}
	}

	// デバイスの状態を取得
	device_->GetDeviceState(sizeof(DIJOYSTATE2), &state_);
}

// --- ボタン入力 ---
bool GamepadInput::IsButtonDown(int buttonIndex) {
	if(buttonIndex < 0 || buttonIndex >= 128) return false;
	return state_.rgbButtons[buttonIndex] & 0x80;
}

bool GamepadInput::IsButtonPressed(int buttonIndex) {
	if(buttonIndex < 0 || buttonIndex >= 128) return false;
	return (state_.rgbButtons[buttonIndex] & 0x80) && !(preState_.rgbButtons[buttonIndex] & 0x80);
}

bool GamepadInput::IsButtonReleased(int buttonIndex) {
	if(buttonIndex < 0 || buttonIndex >= 128) return false;
	return !(state_.rgbButtons[buttonIndex] & 0x80) && (preState_.rgbButtons[buttonIndex] & 0x80);
}

// --- D-Pad入力 ---
bool GamepadInput::IsDPadUp() {
	DWORD pov = state_.rgdwPOV[0];
	return pov == 0;
}
bool GamepadInput::IsDPadDown() {
	DWORD pov = state_.rgdwPOV[0];
	return pov == 18000;
}
bool GamepadInput::IsDPadLeft() {
	DWORD pov = state_.rgdwPOV[0];
	return pov == 27000;
}
bool GamepadInput::IsDPadRight() {
	DWORD pov = state_.rgdwPOV[0];
	return pov == 9000;
}

// --- アナログスティック ---
Vector2 GamepadInput::GetLeftStick() {
	return {
		static_cast<float>(state_.lX) / 1000.0f,
		static_cast<float>(state_.lY) / -1000.0f // Y軸は上下反転
	};
}

Vector2 GamepadInput::GetRightStick() {
	// DIJOYSTATE2では右スティックは通常 lZ, lRz
	return {
		static_cast<float>(state_.lZ) / 1000.0f,
		static_cast<float>(state_.lRz) / -1000.0f // Y軸は上下反転
	};
}
