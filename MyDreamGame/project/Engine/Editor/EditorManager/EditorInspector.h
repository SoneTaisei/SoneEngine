#pragma once
#ifdef USE_IMGUI
#include "Core/Utility/Structs.h"

class EditorManager;
class EditorHierarchy;
class EditorCommandManager;
class SceneManager;
class ModelCommon;
class GameCamera;
class DebugCamera;

class EditorInspector {
public:
    EditorInspector() = default;
    ~EditorInspector() = default;

    void Draw(bool* pOpen, EditorManager* editorManager, SceneManager* sceneManager,
              ModelCommon* modelCommon, GameCamera* gameCamera, DebugCamera* debugCamera);

private:
    void DrawReplayNodeProperties(EditorManager* em);
    void DrawReplayGlobalSettings(EditorManager* em, SceneManager* sceneManager);
    void DrawGlobalSettings(EditorManager* em, ModelCommon* modelCommon, GameCamera* gameCamera, DebugCamera* debugCamera);
    void DrawCameraSettings(EditorManager* em, GameCamera* gameCamera, DebugCamera* debugCamera);
};
#endif
