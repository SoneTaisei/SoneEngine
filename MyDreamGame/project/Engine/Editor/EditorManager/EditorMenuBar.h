#pragma once
#ifdef USE_IMGUI

class EditorManager;
class SceneManager;
class ModelCommon;
class GameCamera;
class DebugCamera;

class EditorMenuBar {
public:
    EditorMenuBar() = default;
    ~EditorMenuBar() = default;

    void Draw(EditorManager* editorManager, SceneManager* sceneManager, ModelCommon* modelCommon,
              GameCamera* gameCamera, DebugCamera* debugCamera);
    void SetupDockspace(EditorManager* editorManager);
};
#endif
