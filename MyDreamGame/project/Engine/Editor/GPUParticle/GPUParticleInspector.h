#pragma once
#ifdef USE_IMGUI

class SceneManager;
class GPUParticleEditorContext;

class GPUParticleInspector {
public:
    GPUParticleInspector() = default;
    ~GPUParticleInspector() = default;

    void Initialize();
    void DrawInspectorUI(SceneManager* sceneManager, GPUParticleEditorContext* context);

private:
    void DrawRendererSection(GPUParticleEditorContext* context, SceneManager* sceneManager);
    void DrawSpawnSection(GPUParticleEditorContext* context);
    void DrawShapeSection(GPUParticleEditorContext* context);
    void DrawPhysicsSection(GPUParticleEditorContext* context);
    void DrawTransformSection(GPUParticleEditorContext* context);
    void DrawColorSection(GPUParticleEditorContext* context);
};
#endif
