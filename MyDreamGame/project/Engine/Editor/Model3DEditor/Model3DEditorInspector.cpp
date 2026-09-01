#ifdef USE_IMGUI
#include "Model3DEditorInspector.h"
#include "Model3DEditorContext.h"
#include "Scene/SceneManager.h"
#include <imgui.h>
#include <cstring>

Model3DEditorInspector::Model3DEditorInspector(Model3DEditorContext* context)
    : context_(context) {
}

bool Model3DEditorInspector::Draw(SceneManager* sceneManager) {
    if (!context_) return false;

    PlacedObject3D* sel = context_->GetSelectedObject();

    if (!sel) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "3Dモデル配置エディター (全体設定)");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("配置オブジェクト総数: %zu", context_->GetObjects().size());
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Spacing();

        // Gizmo Mode Toggle in Inspector
        ImGui::Text("操作モード (S/R/T キーで切替可能):");
        int mode = static_cast<int>(context_->GetGizmoMode());
        ImGui::RadioButton("移動 (T: Translate)", &mode, 0); ImGui::SameLine();
        ImGui::RadioButton("回転 (R: Rotate)", &mode, 1); ImGui::SameLine();
        ImGui::RadioButton("拡大縮小 (S: Scale)", &mode, 2);
        context_->SetGizmoMode(static_cast<Model3DEditorContext::GizmoMode>(mode));

        ImGui::Spacing();
        int space = static_cast<int>(context_->GetGizmoSpace());
        ImGui::RadioButton("World Space", &space, 0); ImGui::SameLine();
        ImGui::RadioButton("Local Space", &space, 1);
        context_->SetGizmoSpace(static_cast<Model3DEditorContext::GizmoSpace>(space));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("配置データの保存・読込み:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SaveFilePath", saveFilePathBuf_, sizeof(saveFilePathBuf_));
        ImGui::Spacing();

        if (ImGui::Button("ファイルに保存 (Save)", ImVec2(140, 26))) {
            context_->SaveToFile(saveFilePathBuf_);
        }
        ImGui::SameLine();
        if (ImGui::Button("ファイルから読込 (Load)", ImVec2(140, 26))) {
            context_->LoadFromFile(saveFilePathBuf_);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("全オブジェクトをクリア (Clear All)", ImVec2(-1, 26))) {
            context_->ClearObjects();
        }
        ImGui::PopStyleColor();

        return false;
    }

    // Selected Object Inspector
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "3Dモデル プロパティ");
    ImGui::Separator();
    ImGui::Spacing();

    // Name
    strcpy_s(nameBuf_, sel->GetName().c_str());
    if (ImGui::InputText("名前 (Name)", nameBuf_, sizeof(nameBuf_))) {
        sel->SetName(nameBuf_);
    }
    if (ImGui::IsItemActivated()) {
        preEditSnapshot_ = context_->CreateSnapshot();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context_->PushSnapshotToUndo(preEditSnapshot_);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("モデル: %s / %s", sel->GetModelDirectory().c_str(), sel->GetModelFileName().c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Transform
    Vector3 pos = sel->GetTranslation();
    if (ImGui::DragFloat3("位置 (Position)", &pos.x, 0.05f)) {
        sel->SetTranslation(pos);
        sel->Update();
    }
    if (ImGui::IsItemActivated()) {
        preEditSnapshot_ = context_->CreateSnapshot();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context_->PushSnapshotToUndo(preEditSnapshot_);
    }

    Vector3 rotDeg = sel->GetRotationDegrees();
    if (ImGui::DragFloat3("回転 (Rotation deg)", &rotDeg.x, 1.0f)) {
        sel->SetRotationDegrees(rotDeg);
        sel->Update();
    }
    if (ImGui::IsItemActivated()) {
        preEditSnapshot_ = context_->CreateSnapshot();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context_->PushSnapshotToUndo(preEditSnapshot_);
    }

    Vector3 scl = sel->GetScale();
    if (ImGui::DragFloat3("スケール (Scale)", &scl.x, 0.02f, 0.001f, 100.0f)) {
        sel->SetScale(scl);
        sel->Update();
    }
    if (ImGui::IsItemActivated()) {
        preEditSnapshot_ = context_->CreateSnapshot();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context_->PushSnapshotToUndo(preEditSnapshot_);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Material & Color
    Vector4 col = sel->GetColor();
    if (ImGui::ColorEdit4("カラー (Color)", &col.x)) {
        sel->SetColor(col);
        sel->Update();
    }
    if (ImGui::IsItemActivated()) {
        preEditSnapshot_ = context_->CreateSnapshot();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context_->PushSnapshotToUndo(preEditSnapshot_);
    }

    bool doubleSided = sel->IsDoubleSided();
    if (ImGui::Checkbox("両面描画 (Double Sided)", &doubleSided)) {
        context_->PushUndoState();
        sel->SetDoubleSided(doubleSided);
        sel->Update();
    }

    // Texture
    strcpy_s(texBuf_, sel->GetTexturePath().c_str());
    if (ImGui::InputText("テクスチャパス (Texture)", texBuf_, sizeof(texBuf_))) {
        sel->SetTexture(texBuf_);
        sel->Update();
    }
    if (ImGui::IsItemActivated()) {
        preEditSnapshot_ = context_->CreateSnapshot();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context_->PushSnapshotToUndo(preEditSnapshot_);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Gizmo Mode Toggle in Object Inspector
    ImGui::Text("操作モード (S/R/T キーで切替可能):");
    int mode = static_cast<int>(context_->GetGizmoMode());
    if (ImGui::RadioButton("移動 (T)", &mode, 0)) context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Translation);
    ImGui::SameLine();
    if (ImGui::RadioButton("回転 (R)", &mode, 1)) context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Rotation);
    ImGui::SameLine();
    if (ImGui::RadioButton("スケール (S)", &mode, 2)) context_->SetGizmoMode(Model3DEditorContext::GizmoMode::Scale);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Actions
    if (ImGui::Button("複製 (Duplicate)", ImVec2(120, 26))) {
        context_->DuplicateObject(sel);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("削除 (Delete)", ImVec2(120, 26))) {
        context_->RemoveObject(sel);
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("選択解除", ImVec2(-1, 24))) {
        context_->ClearSelection();
    }

    return true;
}
#endif
