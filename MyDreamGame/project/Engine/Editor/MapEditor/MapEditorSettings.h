#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <imgui.h>

class SceneManager;
class MapEditorContext;
class MapEditorPalette;

class MapEditorSettings {
public:
    MapEditorSettings(MapEditorContext* context, MapEditorPalette* palette);
    ~MapEditorSettings() = default;

    void Draw(
        SceneManager* sceneManager,
        bool& showMapSettings,
        const std::function<void()>& onSaveSceneConfig = nullptr,
        const std::function<void()>& onSelectionCleared = nullptr
    );

private:
    MapEditorContext* context_ = nullptr;
    MapEditorPalette* palette_ = nullptr;
};
#endif
