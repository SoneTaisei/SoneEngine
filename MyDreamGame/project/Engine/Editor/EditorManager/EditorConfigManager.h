#pragma once
#ifdef USE_IMGUI

class ModelCommon;
class IScene;
class SceneManager;
class GameCamera;
class DebugCamera;
class EditorManager;

class EditorConfigManager {
public:
    EditorConfigManager() = default;
    ~EditorConfigManager() = default;

    // EditorManager* を受け取ることで、各サブエディタやレイアウトに綺麗にアクセス
    void SaveSceneConfig(EditorManager *editor);
    void LoadSceneConfig(EditorManager *editor);

    void SaveLightingConfig(ModelCommon *modelCommon, EditorManager *editor);
    void LoadLightingConfig(ModelCommon *modelCommon, EditorManager *editor);

    void LoadPlacedModelsForScene(IScene *scene);

    void InitializeMapOnStart(EditorManager *editor, SceneManager *sceneManager, GameCamera *gameCamera, DebugCamera *debugCamera);
    void RestoreMapOnReset(EditorManager *editor, SceneManager *sceneManager, GameCamera *gameCamera, DebugCamera *debugCamera);
};
#endif