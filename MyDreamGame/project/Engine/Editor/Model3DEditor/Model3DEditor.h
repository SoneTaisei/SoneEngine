#pragma once
#ifdef USE_IMGUI
#include <d3d12.h>
#include <memory>
#include <vector>
#include "Model3DEditorContext.h"
#include "Model3DEditorViewport.h"
#include "Model3DEditorPalette.h"
#include "Model3DEditorInspector.h"

class SceneManager;
class Camera;

class Model3DEditor {
public:
    Model3DEditor();
    ~Model3DEditor() = default;

    void Initialize(ID3D12Device* device);
    void Update();
    void Draw();

    // UI Drawing
    void DrawMainViewport(
        bool& showViewport,
        SceneManager* sceneManager,
        Camera** activeCamera,
        D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
        std::function<void()> onActive = nullptr
    );

    void DrawPalette(bool& showPalette, SceneManager* sceneManager);
    bool DrawInspectorUI(SceneManager* sceneManager);

    // State & Visibility
    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    bool& GetVisibleRef() { return isVisible_; }

    bool IsHovered() const { return viewport_ ? viewport_->IsHovered() : false; }

    // Sub-components
    Model3DEditorContext* GetContext() { return context_.get(); }
    Model3DEditorViewport* GetViewport() { return viewport_.get(); }
    Model3DEditorPalette* GetPalette() { return palette_.get(); }
    Model3DEditorInspector* GetInspector() { return inspector_.get(); }

    // Helpers for Scene integration (Hierarchy, etc.)
    const std::vector<std::unique_ptr<PlacedObject3D>>& GetPlacedObjects() const {
        return context_->GetObjects();
    }
    PlacedObject3D* GetSelectedObject() const {
        return context_ ? context_->GetSelectedObject() : nullptr;
    }
    void SetSelectedObject(PlacedObject3D* obj) {
        if (context_) context_->SetSelectedObject(obj);
    }

private:
    bool isVisible_ = true;

    std::unique_ptr<Model3DEditorContext> context_;
    std::unique_ptr<Model3DEditorViewport> viewport_;
    std::unique_ptr<Model3DEditorPalette> palette_;
    std::unique_ptr<Model3DEditorInspector> inspector_;
};
#endif
