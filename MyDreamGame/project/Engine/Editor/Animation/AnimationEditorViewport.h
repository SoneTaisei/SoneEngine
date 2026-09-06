#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <imgui.h>
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Matrix4x4.h"
#include "Core/Utility/Quaternion.h"
#include "Graphics/Camera.h"

class SceneManager;
class AnimationEditorContext;

class AnimationEditorViewport {
public:
    AnimationEditorViewport();
    ~AnimationEditorViewport() = default;

    void Initialize();
    void DrawMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, AnimationEditorContext* context);

private:
    void DrawViewportGrid(const Matrix4x4& viewProjectionMatrix, ImVec2 vpPos, ImVec2 vpSize);
    void DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void DrawSkeletonJointsOverlay(SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize, AnimationEditorContext* context);
    void DrawBoneTransformGizmo(SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize, AnimationEditorContext* context);

    // カメラ軸スナップの線形補間用変数
    bool isCameraSnapLerping_ = false;
    float cameraSnapLerpTimer_ = 0.0f;
    float cameraSnapLerpDuration_ = 0.25f;
    Vector3 cameraSnapStartRot_ = {};
    Vector3 cameraSnapEndRot_ = {};
    Vector3 cameraSnapStartPos_ = {};
    Vector3 cameraSnapEndPos_ = {};

    // ギズモドラッグ状態
    int animGizmoActiveAxis_ = -1; // -1: None, 0: X, 1: Y, 2: Z, 3: Center/XYZ
    bool isHoveringAnimGizmo_ = false;
    bool isDraggingAnimGizmo_ = false;
    ImVec2 animGizmoDragStartMouse_ = {};
    Vector3 animGizmoStartTranslate_ = {};
    Quaternion animGizmoStartRotate_ = {};
    Vector3 animGizmoStartScale_ = { 1.0f, 1.0f, 1.0f };
};
#endif
