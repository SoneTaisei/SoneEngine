#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <imgui.h>

class SceneManager;
class GPUParticleEditorContext;

class GPUParticleTimeline {
public:
    GPUParticleTimeline() = default;
    ~GPUParticleTimeline() = default;

    void Initialize();
    void DrawTimelineUI(SceneManager* sceneManager, GPUParticleEditorContext* context);

private:
    void DrawFileOperations(GPUParticleEditorContext* context);
    void DrawEmitterList(GPUParticleEditorContext* context);
    void DrawTimelineTrack(GPUParticleEditorContext* context, float totalWidth);
};
#endif
