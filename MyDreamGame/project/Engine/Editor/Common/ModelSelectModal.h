#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <functional>
#include <d3d12.h>
#include <imgui.h>

class Model;

struct ModelPreviewTriangle {
    ImVec2 offset[3];
    ImVec2 uv[3];
    ImU32 col = 0;
    float avgZ = 0.0f;
};

struct ModelSelectItem {
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
    std::vector<ModelPreviewTriangle> cachedTriangles;
};

class ModelSelectModal {
public:
    ModelSelectModal();
    ~ModelSelectModal() = default;

    void Initialize();
    void RefreshModelList();

    // モーダルダイアログを描画。モデルが決定されたら onSelect を呼び出して isOpen を false にする
    void DrawModal(const char* popupId, bool& isOpen, const std::function<void(const ModelSelectItem&)>& onSelect);

    const std::vector<ModelSelectItem>& GetModelList() const { return modelList_; }

private:
    void EnsureModelLoaded(ModelSelectItem& item);

private:
    std::vector<ModelSelectItem> modelList_;
    char searchFilter_[128] = "";
    int selectedModelIdx_ = -1;
};
#endif
