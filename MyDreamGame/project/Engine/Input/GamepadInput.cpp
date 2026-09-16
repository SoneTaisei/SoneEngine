#define DIRECTINPUT_VERSION 0x0800
#include "GamepadInput.h"
#include <cmath>
#include <algorithm>

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
	if (SUCCEEDED(result)) {
		// ゲームパッドデバイスを列挙
		result = directInput_->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, this, DIEDFL_ATTACHEDONLY);
		if (SUCCEEDED(result) && isDeviceFound_) {
			// デバイスの生成
			result = directInput_->CreateDevice(gamepadGuid_, &device_, NULL);
			if (SUCCEEDED(result) && device_) {
				// 入力データ形式のセット
				device_->SetDataFormat(&c_dfDIJoystick2);
				// 協調レベルのセット
				device_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

				// 軸の範囲設定 (-1000 ~ 1000)
				DIPROPRANGE propRange;
				propRange.diph.dwSize = sizeof(DIPROPRANGE);
				propRange.diph.dwHeaderSize = sizeof(DIPROPHEADER);
				propRange.diph.dwHow = DIPH_BYOFFSET;
				propRange.diph.dwObj = DIJOFS_X;
				propRange.lMin = -1000;
				propRange.lMax = 1000;
				device_->SetProperty(DIPROP_RANGE, &propRange.diph);

				propRange.diph.dwObj = DIJOFS_Y;
				device_->SetProperty(DIPROP_RANGE, &propRange.diph);
			}
		}
	}

	// 起動時のXInput状態を取得しておく
	ZeroMemory(&xState_, sizeof(XINPUT_STATE));
	DWORD dwResult = XInputGetState(0, &xState_);
	isXInputConnected_ = (dwResult == ERROR_SUCCESS);
	preXState_ = xState_;

	return true;
}

void GamepadInput::Update() {
	// --- 1. XInput の更新 ---
	preXState_ = xState_;
	ZeroMemory(&xState_, sizeof(XINPUT_STATE));
	DWORD dwResult = XInputGetState(0, &xState_);
	isXInputConnected_ = (dwResult == ERROR_SUCCESS);

	// --- 2. DirectInput の更新 ---
	if (device_) {
		preState_ = state_;
		HRESULT hr = device_->Poll();
		if (FAILED(hr)) {
			hr = device_->Acquire();
		}
		if (SUCCEEDED(hr)) {
			device_->GetDeviceState(sizeof(DIJOYSTATE2), &state_);
		}
	}
}

bool GamepadInput::IsConnected() const {
	return isXInputConnected_ || (device_ != nullptr && isDeviceFound_);
}

WORD GamepadInput::GetXInputMask(GamepadButton button) const {
	switch (button) {
	case GamepadButton::A:         return XINPUT_GAMEPAD_A;
	case GamepadButton::B:         return XINPUT_GAMEPAD_B;
	case GamepadButton::X:         return XINPUT_GAMEPAD_X;
	case GamepadButton::Y:         return XINPUT_GAMEPAD_Y;
	case GamepadButton::LB:        return XINPUT_GAMEPAD_LEFT_SHOULDER;
	case GamepadButton::RB:        return XINPUT_GAMEPAD_RIGHT_SHOULDER;
	case GamepadButton::Back:      return XINPUT_GAMEPAD_BACK;
	case GamepadButton::Start:     return XINPUT_GAMEPAD_START;
	case GamepadButton::LStick:    return XINPUT_GAMEPAD_LEFT_THUMB;
	case GamepadButton::RStick:    return XINPUT_GAMEPAD_RIGHT_THUMB;
	case GamepadButton::DPadUp:    return XINPUT_GAMEPAD_DPAD_UP;
	case GamepadButton::DPadDown:  return XINPUT_GAMEPAD_DPAD_DOWN;
	case GamepadButton::DPadLeft:  return XINPUT_GAMEPAD_DPAD_LEFT;
	case GamepadButton::DPadRight: return XINPUT_GAMEPAD_DPAD_RIGHT;
	default:                       return 0;
	}
}

bool GamepadInput::CheckXInputButton(const XINPUT_STATE& state, GamepadButton button) const {
	WORD mask = GetXInputMask(button);
	if (mask == 0) return false;
	return (state.Gamepad.wButtons & mask) != 0;
}

// --- ボタン入力 ---
bool GamepadInput::IsButtonDown(GamepadButton button) {
	if (isXInputConnected_) {
		if (CheckXInputButton(xState_, button)) return true;
	}
	if (device_) {
		int idx = static_cast<int>(button);
		if (idx >= 0 && idx < 128) {
			if (state_.rgbButtons[idx] & 0x80) return true;
		}
	}
	return false;
}

bool GamepadInput::IsButtonPressed(GamepadButton button) {
	if (isXInputConnected_) {
		bool curr = CheckXInputButton(xState_, button);
		bool prev = CheckXInputButton(preXState_, button);
		if (curr && !prev) return true;
	}
	if (device_) {
		int idx = static_cast<int>(button);
		if (idx >= 0 && idx < 128) {
			bool curr = (state_.rgbButtons[idx] & 0x80) != 0;
			bool prev = (preState_.rgbButtons[idx] & 0x80) != 0;
			if (curr && !prev) return true;
		}
	}
	return false;
}

bool GamepadInput::IsButtonReleased(GamepadButton button) {
	if (isXInputConnected_) {
		bool curr = CheckXInputButton(xState_, button);
		bool prev = CheckXInputButton(preXState_, button);
		if (!curr && prev) return true;
	}
	if (device_) {
		int idx = static_cast<int>(button);
		if (idx >= 0 && idx < 128) {
			bool curr = (state_.rgbButtons[idx] & 0x80) != 0;
			bool prev = (preState_.rgbButtons[idx] & 0x80) != 0;
			if (!curr && prev) return true;
		}
	}
	return false;
}

bool GamepadInput::IsButtonDown(int buttonIndex) {
	if (buttonIndex >= 0 && buttonIndex < static_cast<int>(GamepadButton::Count)) {
		if (IsButtonDown(static_cast<GamepadButton>(buttonIndex))) return true;
	}
	if (device_ && buttonIndex >= 0 && buttonIndex < 128) {
		return (state_.rgbButtons[buttonIndex] & 0x80) != 0;
	}
	return false;
}

bool GamepadInput::IsButtonPressed(int buttonIndex) {
	if (buttonIndex >= 0 && buttonIndex < static_cast<int>(GamepadButton::Count)) {
		if (IsButtonPressed(static_cast<GamepadButton>(buttonIndex))) return true;
	}
	// DirectInput特有のボタンインデックス（例: 9番など）
	if (device_ && buttonIndex >= 0 && buttonIndex < 128) {
		return (state_.rgbButtons[buttonIndex] & 0x80) && !(preState_.rgbButtons[buttonIndex] & 0x80);
	}
	return false;
}

bool GamepadInput::IsButtonReleased(int buttonIndex) {
	if (buttonIndex >= 0 && buttonIndex < static_cast<int>(GamepadButton::Count)) {
		if (IsButtonReleased(static_cast<GamepadButton>(buttonIndex))) return true;
	}
	if (device_ && buttonIndex >= 0 && buttonIndex < 128) {
		return !(state_.rgbButtons[buttonIndex] & 0x80) && (preState_.rgbButtons[buttonIndex] & 0x80);
	}
	return false;
}

// --- D-Pad入力 ---
bool GamepadInput::IsDPadUp() {
	if (isXInputConnected_ && CheckXInputButton(xState_, GamepadButton::DPadUp)) {
		return true;
	}
	if (device_) {
		DWORD pov = state_.rgdwPOV[0];
		if (pov == 0) return true;
	}
	return false;
}

bool GamepadInput::IsDPadDown() {
	if (isXInputConnected_ && CheckXInputButton(xState_, GamepadButton::DPadDown)) {
		return true;
	}
	if (device_) {
		DWORD pov = state_.rgdwPOV[0];
		if (pov == 18000) return true;
	}
	return false;
}

bool GamepadInput::IsDPadLeft() {
	if (isXInputConnected_ && CheckXInputButton(xState_, GamepadButton::DPadLeft)) {
		return true;
	}
	if (device_) {
		DWORD pov = state_.rgdwPOV[0];
		if (pov == 27000) return true;
	}
	return false;
}

bool GamepadInput::IsDPadRight() {
	if (isXInputConnected_ && CheckXInputButton(xState_, GamepadButton::DPadRight)) {
		return true;
	}
	if (device_) {
		DWORD pov = state_.rgdwPOV[0];
		if (pov == 9000) return true;
	}
	return false;
}

// --- アナログスティック ---
Vector2 GamepadInput::GetLeftStick() {
	if (isXInputConnected_) {
		float rawX = static_cast<float>(xState_.Gamepad.sThumbLX);
		float rawY = static_cast<float>(xState_.Gamepad.sThumbLY);
		float mag = std::sqrt(rawX * rawX + rawY * rawY);

		if (mag > static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)) {
			if (mag > 32767.0f) mag = 32767.0f;
			float normMag = (mag - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			return {
				(rawX / mag) * normMag,
				(rawY / mag) * normMag
			};
		}
		return { 0.0f, 0.0f };
	}

	if (device_) {
		float x = static_cast<float>(state_.lX) / 1000.0f;
		float y = static_cast<float>(state_.lY) / -1000.0f;
		if (std::abs(x) < 0.2f) x = 0.0f;
		if (std::abs(y) < 0.2f) y = 0.0f;
		return { x, y };
	}

	return { 0.0f, 0.0f };
}

Vector2 GamepadInput::GetRightStick() {
	if (isXInputConnected_) {
		float rawX = static_cast<float>(xState_.Gamepad.sThumbRX);
		float rawY = static_cast<float>(xState_.Gamepad.sThumbRY);
		float mag = std::sqrt(rawX * rawX + rawY * rawY);

		if (mag > static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)) {
			if (mag > 32767.0f) mag = 32767.0f;
			float normMag = (mag - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / (32767.0f - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			return {
				(rawX / mag) * normMag,
				(rawY / mag) * normMag
			};
		}
		return { 0.0f, 0.0f };
	}

	if (device_) {
		float x = static_cast<float>(state_.lZ) / 1000.0f;
		float y = static_cast<float>(state_.lRz) / -1000.0f;
		if (std::abs(x) < 0.2f) x = 0.0f;
		if (std::abs(y) < 0.2f) y = 0.0f;
		return { x, y };
	}

	return { 0.0f, 0.0f };
}

// --- トリガー入力 ---
float GamepadInput::GetLeftTrigger() {
	if (isXInputConnected_) {
		BYTE raw = xState_.Gamepad.bLeftTrigger;
		if (raw > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
			return static_cast<float>(raw - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / (255.0f - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		}
	}
	return 0.0f;
}

float GamepadInput::GetRightTrigger() {
	if (isXInputConnected_) {
		BYTE raw = xState_.Gamepad.bRightTrigger;
		if (raw > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
			return static_cast<float>(raw - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / (255.0f - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		}
	}
	return 0.0f;
}
