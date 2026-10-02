#pragma once
#ifdef USE_IMGUI

class SceneManager;
class AnimationEditorContext;

class AnimationInspector {
public:
    AnimationInspector();
    ~AnimationInspector() = default;

    void Initialize();
    void DrawInspectorUI(SceneManager* sceneManager, AnimationEditorContext* context);
};
#endif
