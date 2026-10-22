#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <functional>
#include <d3d12.h>
#include <imgui.h>

struct SpriteSelectItem {
    std::string displayName;
    std::string directoryPath;
    std::string fileName;
    std::string fullPath;
    std::string extension;
    std::string subCategory;
    D3D12_GPU_DESCRIPTOR_HANDLE textureGpuHandle = {};
    bool hasTexture = false;
    bool isResourceLoaded = false;
};

class SpriteSelectModal {
public:
    SpriteSelectModal();
    ~SpriteSelectModal() = default;

    void Initialize();
    void RefreshSpriteList();

    // モーダルダイアログを描画。スプライトが決定されたら onSelect を呼び出して isOpen を false にする
    // targetSpritePath: 選択を解除してモデル本来のテクスチャにする場合は空文字列 "" が渡される
    void DrawModal(const char* popupId, bool& isOpen, const std::function<void(const std::string&)>& onSelect);

    const std::vector<SpriteSelectItem>& GetSpriteList() const { return spriteList_; }
    const std::vector<std::string>& GetCategories() const { return categories_; }

private:
    void EnsureTextureLoaded(SpriteSelectItem& item);

private:
    std::vector<SpriteSelectItem> spriteList_;
    std::vector<std::string> categories_;
    char searchFilter_[128] = "";
    int selectedCategoryIdx_ = 0; // 0: "すべて (All)"
    int selectedSpriteIdx_ = -1;
};
#endif
