#ifdef USE_IMGUI
#include "MapEditor.h"

MapEditor::MapEditor() {
    context_ = std::make_unique<MapEditorContext>();
    canvas_ = std::make_unique<MapEditorCanvas>(context_.get());
    palette_ = std::make_unique<MapEditorPalette>(context_.get());
    settings_ = std::make_unique<MapEditorSettings>(context_.get(), palette_.get());
    inspector_ = std::make_unique<MapEditorInspector>(context_.get());
}

void MapEditor::Initialize() {
    if (context_) {
        context_->Initialize();
    }
}

void MapEditor::DrawCanvas(
    SceneManager* sceneManager,
    Camera** activeCamera,
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
    const std::function<void()>& onTabActive
) {
    if (canvas_) {
        canvas_->Draw(sceneManager, activeCamera, renderTextureSrvHandle, isVisible_, isHovered_, onTabActive);
    }
}

void MapEditor::DrawSettingsUI(
    SceneManager* sceneManager,
    bool& showMapSettings,
    const std::function<void()>& onSaveSceneConfig,
    const std::function<void()>& onSelectionCleared
) {
    if (settings_) {
        settings_->Draw(sceneManager, showMapSettings, onSaveSceneConfig, onSelectionCleared);
    }
}

bool MapEditor::DrawInspectorUI(SceneManager* sceneManager) {
    if (inspector_) {
        return inspector_->Draw(sceneManager);
    }
    return false;
}
#endif
