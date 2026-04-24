#pragma once
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h" // ImGuiを使うため
#endif
#include "GameObject/Object3D.h"
#include "Core/Utility/Structs.h"
#include <string>

// これが魔法の関数！モデルを渡すだけでGUIが出る
inline void ShowObject3DGui(const std::string &name, Object3D *object) {
    if (object == nullptr)
        return;

#ifdef USE_IMGUI
    ImGui::PushID(object);
    object->DisplayImGui(name);
    ImGui::PopID();
#endif
}
