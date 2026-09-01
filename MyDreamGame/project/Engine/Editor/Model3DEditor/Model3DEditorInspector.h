#pragma once
#ifdef USE_IMGUI
#include "Model3DEditorContext.h"
#include <string>

class SceneManager;

class Model3DEditorInspector {
public:
    Model3DEditorInspector(Model3DEditorContext* context);
    ~Model3DEditorInspector() = default;

    bool Draw(SceneManager* sceneManager);

private:
    Model3DEditorContext* context_ = nullptr;
    Model3DEditorContext::Model3DEditorSnapshot preEditSnapshot_;
    char nameBuf_[128] = "";
    char texBuf_[256] = "";
    char saveFilePathBuf_[256] = "resources/json/shared/LevelData/placed_models.json";
};
#endif
