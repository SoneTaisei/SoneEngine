#ifdef USE_IMGUI
#include "Model3DEditor.h"

Model3DEditor::Model3DEditor() {
    context_ = std::make_unique<Model3DEditorContext>();
    viewport_ = std::make_unique<Model3DEditorViewport>(context_.get());
    palette_ = std::make_unique<Model3DEditorPalette>(context_.get());
    inspector_ = std::make_unique<Model3DEditorInspector>(context_.get());
}

void Model3DEditor::Initialize(ID3D12Device* device) {
    if (context_) {
        context_->Initialize(device);
    }
    if (viewport_) {
        viewport_->Initialize();
    }
    if (palette_) {
        palette_->Initialize();
    }
}

void Model3DEditor::Update() {
    if (context_) {
        context_->Update();
    }
}

void Model3DEditor::Draw() {
    if (context_) {
        context_->Draw();
    }
}

void Model3DEditor::DrawMainViewport(
    bool& showViewport,
    SceneManager* sceneManager,
    Camera** activeCamera,
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
    std::function<void()> onActive
) {
    if (viewport_) {
        viewport_->Draw(showViewport, sceneManager, activeCamera, renderTextureSrvHandle, onActive);
    }
}

void Model3DEditor::DrawPalette(bool& showPalette, SceneManager* sceneManager) {
    if (palette_) {
        palette_->Draw(showPalette, sceneManager);
    }
}

bool Model3DEditor::DrawInspectorUI(SceneManager* sceneManager) {
    if (inspector_) {
        return inspector_->Draw(sceneManager);
    }
    return false;
}
#endif
