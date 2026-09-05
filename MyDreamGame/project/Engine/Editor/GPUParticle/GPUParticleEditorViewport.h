#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <imgui.h>
#include "Graphics/Camera.h"
#include "Core/Utility/UtilityFunctions.h"

class SceneManager;
class GPUParticleEditorContext;

class GPUParticleEditorViewport {
public:
    GPUParticleEditorViewport() = default;
    ~GPUParticleEditorViewport() = default;

    void Initialize();
    void DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, GPUParticleEditorContext* context);

private:
    void DrawHUD(GPUParticleEditorContext* context, ImVec2 vpPos, ImVec2 vpSize);
    void DrawStatsHUD(GPUParticleEditorContext* context, ImVec2 vpPos, ImVec2 vpSize);
    void DrawShapeGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize, GPUParticleEditorContext* context);
    void DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);

private:
    bool isCameraSnapLerping_ = false;
    float cameraSnapLerpTimer_ = 0.0f;
    float cameraSnapLerpDuration_ = 0.25f;
    Vector3 cameraSnapStartRot_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraSnapEndRot_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraSnapStartPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraSnapEndPos_ = { 0.0f, 0.0f, 0.0f };
};
#endif
