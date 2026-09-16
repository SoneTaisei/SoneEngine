#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <memory>
#include <string>
#include "Graphics/Camera.h"
#include "GPUParticleEditorContext.h"
#include "GPUParticleEditorViewport.h"
#include "GPUParticleInspector.h"
#include "GPUParticleTimeline.h"

class SceneManager;
class ParticleCommon;
class ModelManager;

class GPUParticleEditor {
public:
    GPUParticleEditor();
    ~GPUParticleEditor() = default;

    void Initialize(ID3D12Device* device);
    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager);

    // UI描画
    void DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle);
    void DrawInspectorUI(SceneManager* sceneManager);
    void DrawTimelineUI(SceneManager* sceneManager);

    // 状態管理
    bool IsHovered() const;
    void SetHovered(bool hovered);
    bool& GetShowEditor();
    void SetShowEditor(bool show);
    bool IsParticleScenePushed() const;
    void SetParticleScenePushed(bool pushed);

    // サブコンポーネントアクセサ
    GPUParticleEditorContext* GetContext() { return context_.get(); }
    GPUParticleEditorViewport* GetViewport() { return viewport_.get(); }
    GPUParticleInspector* GetInspector() { return inspector_.get(); }
    GPUParticleTimeline* GetTimeline() { return timeline_.get(); }

private:
    std::unique_ptr<GPUParticleEditorContext> context_;
    std::unique_ptr<GPUParticleEditorViewport> viewport_;
    std::unique_ptr<GPUParticleInspector> inspector_;
    std::unique_ptr<GPUParticleTimeline> timeline_;
};
#endif
