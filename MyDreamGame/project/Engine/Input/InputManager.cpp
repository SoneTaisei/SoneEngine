#include "InputManager.h"

void InputManager::UpdateFromImGui() {
#ifdef USE_IMGUI


    ImGuiIO &io = ImGui::GetIO();

    mouseDelta_ = { io.MouseDelta.x, io.MouseDelta.y };
    wheelDelta_ = io.MouseWheel;

    for(int i = 0; i < 5; ++i) {
        mouseDown_[i] = io.MouseDown[i];
    }

    ctrlDown_ = io.KeyCtrl;
#endif // USE_IMGUI
}

Vector3 InputManager::GetMouseDelta() {
    return mouseDelta_;
}

bool InputManager::IsMouseDown(int button) {
    if(button < 0 || button >= 5) return false;
    return mouseDown_[button];
}

float InputManager::GetWheelDelta() {
    return wheelDelta_;
}

bool InputManager::IsCtrlPressed() {
    return ctrlDown_;
}
