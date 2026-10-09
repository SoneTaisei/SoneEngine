#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <imgui.h>

class SceneManager;
class MapEditorContext;
class Model;

struct MapChipPreviewTriangle {
    ImVec2 offset[3];
    ImVec2 uv[3];
    ImU32 col = 0;
    float avgZ = 0.0f;
};

struct MapChipPreviewCacheKey {
    Model* modelPtr = nullptr;
    uint32_t colorHex = 0;
    int scaleInt = 100;

    bool operator==(const MapChipPreviewCacheKey& other) const {
        return modelPtr == other.modelPtr &&
               colorHex == other.colorHex &&
               scaleInt == other.scaleInt;
    }
};

struct MapChipPreviewCacheKeyHash {
    size_t operator()(const MapChipPreviewCacheKey& k) const {
        size_t h1 = std::hash<void*>()(k.modelPtr);
        size_t h2 = std::hash<uint32_t>()(k.colorHex);
        size_t h3 = std::hash<int>()(k.scaleInt);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct MapChipResourceCacheItem {
    Model* model = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE texGpuHandle = {};
    bool hasTexture = false;
};

class MapEditorPalette {
public:
    MapEditorPalette(MapEditorContext* context);
    ~MapEditorPalette() = default;

    void Draw(SceneManager* sceneManager, const std::function<void()>& onSelectionCleared = nullptr);

    // キャッシュをクリアする
    void ClearPreviewCache() {
        previewCache_.clear();
        resourceCache_.clear();
    }

private:
    MapEditorContext* context_ = nullptr;
    int toolToDelete_ = -1;
    bool openDeletePopup_ = false;
    int templateToDelete_ = -1;
    bool openDeleteTemplatePopup_ = false;
    bool deleteClassSourceFiles_ = true;

    // 新規ブロッククラス作成モーダル用
    bool openCreateBlockPopup_ = false;
    char newClassNameBuf_[128] = "JumpBlock";
    char newDisplayNameBuf_[128] = "Jump";
    int newBlockBehaviorType_ = 0; // 0: 通常(Solid), 1: すり抜け(OneWay), 2: ダメージ(Death), 3: ゴール(Goal), 4: トリガー(Non-solid)
    float newBlockColor_[4] = { 0.4f, 0.7f, 0.9f, 1.0f };
    std::string createResultStatus_ = "";
    bool showCreateResultPopup_ = false;

    // プレビュー描画キャッシュ
    std::unordered_map<MapChipPreviewCacheKey, std::vector<MapChipPreviewTriangle>, MapChipPreviewCacheKeyHash> previewCache_;
    std::unordered_map<std::string, MapChipResourceCacheItem> resourceCache_;
};
#endif

