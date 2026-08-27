#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <imgui.h>

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
