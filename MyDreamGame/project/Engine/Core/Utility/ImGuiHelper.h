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

    if (ImGui::TreeNode(name.c_str())) {
        // Object3Dが持っている座標情報を取得
        // ※Object3DにGetter/Setterがない場合は追加が必要です
        Vector3 pos = object->GetTranslation();
        Vector3 rot = object->GetRotation();
        Vector3 scale = object->GetScale();

        ImGui::DragFloat3("Translate", &pos.x, 0.01f);
        ImGui::DragFloat3("Rotate", &rot.x, 0.01f);
        ImGui::DragFloat3("Scale", &scale.x, 0.01f);

        // Object3Dに値を戻す
        object->SetTranslation(pos);
        object->SetRotation(rot);
        object->SetScale(scale);

        ImGui::TreePop();
    }
    ImGui::PopID();
#endif
}
