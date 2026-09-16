#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <d3d12.h>
#include <imgui.h>

class Model3DEditorContext;
class SceneManager;
class Model;

struct PreviewTriangle {
    ImVec2 offset[3];
    ImVec2 uv[3];
    ImU32 col = 0;
    float avgZ = 0.0f;
};

struct ModelAssetItem {
    std::string displayName;
    std::string directoryPath;
    std::string fileName;
    std::string fullPath;
    std::string extension;
    Model* modelPtr = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE textureGpuHandle = {};
    bool hasTexture = false;
    bool isResourceLoaded = false;
    bool isPreviewGenerated = false;
    std::vector<PreviewTriangle> cachedTriangles;
};

class Model3DEditorPalette {
public:
    Model3DEditorPalette(Model3DEditorContext* context);
    ~Model3DEditorPalette() = default;

    void Initialize();
    void RefreshModelList();

    void Draw(bool& showModelPalette, SceneManager* sceneManager);

    const std::vector<ModelAssetItem>& GetModelList() const { return modelList_; }

private:
    void EnsureModelLoaded(ModelAssetItem& item);

private:
    Model3DEditorContext* context_ = nullptr;
    std::vector<ModelAssetItem> modelList_;
    char searchFilter_[128] = "";
    int selectedModelIdx_ = -1;

    // JSON Level Data File Management
    char saveFileNameBuf_[128] = "placed_models";
    int selectedFileComboIdx_ = -1;
};
#endif

