#pragma once
#ifdef USE_IMGUI
#include <d3d12.h>
#include <memory>
#include <functional>
#include "MapEditorContext.h"
#include "MapEditorCanvas.h"
#include "MapEditorPalette.h"
#include "MapEditorSettings.h"
#include "MapEditorInspector.h"

class SceneManager;
class Camera;

class MapEditor {
public:
    MapEditor();
    ~MapEditor() = default;

    void Initialize();

    // UI描画
    void DrawCanvas(
        SceneManager* sceneManager,
        Camera** activeCamera,
        D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
        const std::function<void()>& onTabActive = nullptr
    );

    void DrawSettingsUI(
        SceneManager* sceneManager,
        bool& showMapSettings,
        const std::function<void()>& onSaveSceneConfig = nullptr,
        const std::function<void()>& onSelectionCleared = nullptr
    );

    bool DrawInspectorUI(SceneManager* sceneManager);

    // 状態アクセサ
    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    bool& GetVisibleRef() { return isVisible_; }

    bool IsHovered() const { return isHovered_; }
    bool IsRoomDragging() const { return context_ ? context_->GetDraggingRoomIndex() != -1 : false; }

    const char* GetStageFilename() const { return context_ ? context_->GetStageFilename() : ""; }
    void SetStageFilename(const std::string& filename) { if (context_) context_->SetStageFilename(filename); }

    void UpdateAStarPositionsFromMap(MapChip2D* mapChip, SceneManager* sceneManager = nullptr) {
        if (context_) context_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
    }
    float GetAStarStartX() const { return context_ ? context_->GetAStarStartX() : 0.0f; }
    float GetAStarStartY() const { return context_ ? context_->GetAStarStartY() : 0.0f; }
    float GetAStarGoalX() const { return context_ ? context_->GetAStarGoalX() : 30.0f; }
    float GetAStarGoalY() const { return context_ ? context_->GetAStarGoalY() : 0.0f; }

    void ClearHistory() { if (context_) context_->ClearHistory(); }

    void BeginMapHistoryCapture(MapChip2D* mapChip) { if (context_) context_->BeginMapHistoryCapture(mapChip); }
    void EndMapHistoryCapture(MapChip2D* mapChip) { if (context_) context_->EndMapHistoryCapture(mapChip); }
    void BeginRoomHistoryCapture(MapChip2D* mapChip) { if (context_) context_->BeginRoomHistoryCapture(mapChip); }
    void EndRoomHistoryCapture(MapChip2D* mapChip) { if (context_) context_->EndRoomHistoryCapture(mapChip); }

    // サブコンポーネントアクセサ
    MapEditorContext* GetContext() { return context_.get(); }
    MapEditorCanvas* GetCanvas() { return canvas_.get(); }
    MapEditorPalette* GetPalette() { return palette_.get(); }
    MapEditorSettings* GetSettings() { return settings_.get(); }
    MapEditorInspector* GetInspector() { return inspector_.get(); }

private:
    bool isVisible_ = false;
    bool isHovered_ = false;

    std::unique_ptr<MapEditorContext> context_;
    std::unique_ptr<MapEditorCanvas> canvas_;
    std::unique_ptr<MapEditorPalette> palette_;
    std::unique_ptr<MapEditorSettings> settings_;
    std::unique_ptr<MapEditorInspector> inspector_;
};
#endif
