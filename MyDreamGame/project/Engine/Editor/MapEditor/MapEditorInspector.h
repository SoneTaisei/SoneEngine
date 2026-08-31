#pragma once
#ifdef USE_IMGUI
#include <imgui.h>

class SceneManager;
class MapEditorContext;

class MapEditorInspector {
public:
    MapEditorInspector(MapEditorContext* context);
    ~MapEditorInspector() = default;

    bool Draw(SceneManager* sceneManager);

private:
    MapEditorContext* context_ = nullptr;
};
#endif
