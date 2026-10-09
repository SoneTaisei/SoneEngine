#pragma once
#ifdef USE_IMGUI
#include <d3d12.h>
#include <imgui.h>
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Matrix4x4.h"
#include "Model3DEditorContext.h"

#include <functional>
class SceneManager;
class Camera;

class Model3DEditorViewport {
public:
    Model3DEditorViewport(Model3DEditorContext* context);
    ~Model3DEditorViewport() = default;

    void Initialize();

    void Draw(
        bool& showViewport,
        SceneManager* sceneManager,
        Camera** activeCamera,
        D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
        std::function<void()> onActive = nullptr
    );

    bool IsHovered() const { return isHovered_; }
    void SetHovered(bool hovered) { isHovered_ = hovered; }

private:
    void DrawViewportGrid(const Matrix4x4& viewProjectionMatrix, ImVec2 vpPos, ImVec2 vpSize);
    void DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void DrawTransformGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void HandleObjectPicking(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void HandleDragAndDrop(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize);
    void HandleKeyboardShortcuts();

    // Helper: Compute ray in world space from mouse screen position
    void GetMouseRay(Camera* activeCamera, ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSize, Vector3& outRayOrigin, Vector3& outRayDir);

private:
    Model3DEditorContext* context_ = nullptr;
    bool isHovered_ = false;

    // Gizmo dragging state
    bool isDraggingGizmo_ = false;
    int gizmoActiveAxis_ = -1; // 0: X, 1: Y, 2: Z, 3: Center/All
    ImVec2 gizmoDragStartMouse_ = { 0.0f, 0.0f };
    Vector3 gizmoStartTranslation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 gizmoStartRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 gizmoStartScale_ = { 1.0f, 1.0f, 1.0f };
    Model3DEditorContext::Model3DEditorSnapshot gizmoDragStartSnapshot_;

    // Camera snap lerp animation
    bool isCameraSnapLerping_ = false;
    float cameraSnapLerpTimer_ = 0.0f;
    float cameraSnapLerpDuration_ = 0.25f;
    Vector3 cameraSnapStartRot_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraSnapEndRot_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraSnapStartPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraSnapEndPos_ = { 0.0f, 0.0f, 0.0f };
};
#endif
