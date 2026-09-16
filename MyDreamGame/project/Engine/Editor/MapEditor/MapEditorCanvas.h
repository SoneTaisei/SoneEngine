#pragma once
#ifdef USE_IMGUI
#include <d3d12.h>
#include <functional>
#include <imgui.h>

class SceneManager;
class Camera;
class MapEditorContext;

class MapEditorCanvas {
public:
    MapEditorCanvas(MapEditorContext* context);
    ~MapEditorCanvas() = default;

    void Draw(
        SceneManager* sceneManager,
        Camera** activeCamera,
        D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
        bool& isMapEditorVisible,
        bool& isMapEditorHovered,
        const std::function<void()>& onTabActive = nullptr
    );

private:
    MapEditorContext* context_ = nullptr;
};
#endif
