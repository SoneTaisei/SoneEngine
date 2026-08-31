#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <imgui.h>

class SceneManager;
class MapEditorContext;

class MapEditorPalette {
public:
    MapEditorPalette(MapEditorContext* context);
    ~MapEditorPalette() = default;

    void Draw(SceneManager* sceneManager, const std::function<void()>& onSelectionCleared = nullptr);

private:
    MapEditorContext* context_ = nullptr;
    int toolToDelete_ = -1;
    bool openDeletePopup_ = false;
};
#endif
