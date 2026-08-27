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

class AnimationDopeSheet {
public:
    AnimationDopeSheet();
    ~AnimationDopeSheet() = default;

    void Initialize();
    void DrawDopeSheetUI(SceneManager* sceneManager, AnimationEditorContext* context);

    float& GetTimelineZoom() { return animTimelineZoom_; }
    float GetTimelineZoom() const { return animTimelineZoom_; }

    float& GetTimelineScrollX() { return animTimelineScrollX_; }
    float GetTimelineScrollX() const { return animTimelineScrollX_; }

private:
    bool isDraggingAnimKeyframe_ = false;
    float dragAnimKeyOriginalTime_ = 0.0f;
    bool isSummaryKeyDrag_ = false;
    float dragSummaryOriginalTime_ = 0.0f;
    bool isAnimRulerScrubbing_ = false;
    float animTimelineZoom_ = 200.0f; // 1秒あたりのピクセル幅
    float animTimelineScrollX_ = 0.0f;
};
#endif
