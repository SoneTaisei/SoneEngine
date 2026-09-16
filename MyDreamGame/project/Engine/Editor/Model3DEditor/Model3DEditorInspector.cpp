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

        DrawFileManagementSection();

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
    if (ImGui::Button("コピー (Ctrl+C)", ImVec2(110, 26))) {
        context_->CopySelectedObject();
    }
    ImGui::SameLine();
    bool canPaste = context_->HasClipboard();
    if (!canPaste) ImGui::BeginDisabled();
    if (ImGui::Button("貼り付け (Ctrl+V)", ImVec2(120, 26))) {
        context_->PasteObject();
    }
    if (!canPaste) ImGui::EndDisabled();

    ImGui::Spacing();

    if (ImGui::Button("複製 (Ctrl+D)", ImVec2(110, 26))) {
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("3D配置データ JSONファイル管理", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawFileManagementSection();
    }

    return true;
}

void Model3DEditorInspector::DrawFileManagementSection() {
    if (!context_) return;

    const auto& fileList = context_->GetAvailableLevelFiles();
    std::string curFileName = context_->GetCurrentFileName();

    static std::string lastSyncedFileName = "";
    if (lastSyncedFileName != curFileName) {
        lastSyncedFileName = curFileName;
        strcpy_s(saveFileNameBuf_, curFileName.c_str());
    }

    // 既存の3Dモデル配置ファイルを選択するコンボボックス
    if (!fileList.empty()) {
        selectedFileComboIdx_ = -1;
        for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
            if (fileList[i] == curFileName) {
                selectedFileComboIdx_ = i;
                break;
            }
        }

        std::string comboPreview = (selectedFileComboIdx_ != -1) ? fileList[selectedFileComboIdx_] : "3Dモデルファイルを選択...";
        if (ImGui::BeginCombo("3Dモデルファイルを選択", comboPreview.c_str())) {
            for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
                bool isSelected = (selectedFileComboIdx_ == i);
                if (ImGui::Selectable(fileList[i].c_str(), isSelected)) {
                    selectedFileComboIdx_ = i;
                    strcpy_s(saveFileNameBuf_, fileList[i].c_str());
                    context_->LoadFromFile(fileList[i]);
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // ファイル名入力 (Enterキーでロード)
    if (ImGui::InputText("ファイル名", saveFileNameBuf_, sizeof(saveFileNameBuf_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (strlen(saveFileNameBuf_) > 0) {
            context_->LoadFromFile(saveFileNameBuf_);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 操作ボタン (保存、データを削除)
    if (ImGui::Button("保存", ImVec2(100, 26))) {
        if (strlen(saveFileNameBuf_) > 0) {
            context_->SaveToFile(saveFileNameBuf_);
        } else {
            context_->SaveToFile();
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("データを削除", ImVec2(110, 26))) {
        ImGui::OpenPopup("Delete3DModelDataInspectorConfirmPopup");
    }
    ImGui::PopStyleColor(3);

    // 削除確認ポップアップ
    if (ImGui::BeginPopupModal("Delete3DModelDataInspectorConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string targetFile = context_->GetCurrentFileName();
        ImGui::Text("本当に3Dモデル配置ファイル '%s' を削除しますか？", targetFile.c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "※この操作は取り消せません。");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("削除", ImVec2(120, 0))) {
            context_->DeleteFile(targetFile);
            strcpy_s(saveFileNameBuf_, context_->GetCurrentFileName().c_str());
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ステータスメッセージ表示
    if (context_->GetStatusMessageTimer() > 0.0f && !context_->GetStatusMessage().empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), ">> %s", context_->GetStatusMessage().c_str());
    }
}
#endif
