#pragma once
#include"Core/Utility/Structs.h" // 必要なら自作Vector2型を定義

class InputManager {
public:
    static void UpdateFromImGui(); // ImGui::GetIO() を読み込む

    static Vector3 GetMouseDelta();
    static bool IsMouseDown(int button); // 0:左 1:右 2:中
    static float GetWheelDelta();
    static bool IsCtrlPressed();

private:
    static inline Vector3 mouseDelta_ = { 0, 0 };
    static inline bool mouseDown_[5] = {};
    static inline float wheelDelta_ = 0.0f;
    static inline bool ctrlDown_ = false;
};

