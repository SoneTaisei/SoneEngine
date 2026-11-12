#ifdef USE_IMGUI
#include "EditorCommandManager.h"
#include "EditorManager.h"
#include "Editor/Model3DEditor/Model3DEditor.h"
#include "Editor/Model3DEditor/Model3DEditorContext.h"
#include "Editor/MapEditor/MapEditor.h"
#include "Editor/MapEditor/MapEditorContext.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include <imgui.h>

void EditorCommandManager::Undo() {
    if (!undoStack_.empty()) {
        auto cmd = undoStack_.back();
        undoStack_.pop_back();
        cmd->Undo();
        redoStack_.push_back(cmd);
    }
}

void EditorCommandManager::Redo() {
    if (redoStack_.empty()) {
        return;
    }
    auto cmd = redoStack_.back();
    redoStack_.pop_back();
    cmd->Redo();
    undoStack_.push_back(cmd);
}

void EditorCommandManager::UpdateShortcuts(EditorManager *em, SceneManager *sceneManager) {
    if (!em) return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;

    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;

    // 3Dモデル配置エディター用 Undo / Redo
    if (em->activeMainTab_ == "3Dモデル配置" || em->showModelPlacementEditor_) {
        if (em->model3DEditor_ && em->model3DEditor_->GetContext()) {
            if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                em->model3DEditor_->GetContext()->Undo();
            }
            if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) {
                em->model3DEditor_->GetContext()->Redo();
            }
        }
    }

    // マップエディター用 Undo / Redo
    if (em->activeMainTab_ == "マップチップ" || em->showMapSettings_ || (em->mapEditor_ && em->mapEditor_->IsVisible())) {
        if (em->mapEditor_ && em->activeMainTab_ != "3Dモデル配置" && em->mapEditor_->GetContext()) {
            if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                em->mapEditor_->GetContext()->Undo();
                IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
                if (activeScene && activeScene->GetMapChip() && EditorManager::IsPlaying()) {
                    em->SyncPlayMapData(activeScene->GetMapChip());
                }
            }
            if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) {
                em->mapEditor_->GetContext()->Redo();
                IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
                if (activeScene && activeScene->GetMapChip() && EditorManager::IsPlaying()) {
                    em->SyncPlayMapData(activeScene->GetMapChip());
                }
            }
        }
    }
}
#endif
