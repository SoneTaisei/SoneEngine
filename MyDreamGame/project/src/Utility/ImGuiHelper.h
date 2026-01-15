#pragma once
#include "../externals/imgui/imgui.h" // ImGuiを使うため
#include "Model/Model.h"
#include <string>

// これが魔法の関数！モデルを渡すだけでGUIが出る
inline void ShowModelGui(const std::string &name, Model *model) {
    if (model == nullptr)
        return;

    // ID被り防止（同じ名前のモデルがいても大丈夫なように、ポインタをIDにする）
    ImGui::PushID(model);

    // ツリー形式で表示
    if (ImGui::TreeNode(name.c_str())) {

        // 1. Modelから「今の値」を取ってくる (Get)
        Vector3 pos = model->GetTranslation();
        Vector3 rot = model->GetRotation();
        Vector3 scale = model->GetScale();

        // 2. ImGuiで値をいじる
        ImGui::DragFloat3("Translate", &pos.x, 0.01f);
        ImGui::DragFloat3("Rotate", &rot.x, 0.01f);
        ImGui::DragFloat3("Scale", &scale.x, 0.01f);

        // 3. いじった値をModelに戻す (Set)
        model->SetTranslation(pos);
        model->SetRotation(rot);
        model->SetScale(scale);

        ImGui::TreePop();
    }

    ImGui::PopID();
}
