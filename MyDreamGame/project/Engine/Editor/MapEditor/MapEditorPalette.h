#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <string>
#include <imgui.h>

class SceneManager;
class MapEditorContext;

class MapEditorPalette {
public:
    MapEditorPalette(MapEditorContext* context);
    ~MapEditorPalette() = default;

    void Draw(SceneManager* sceneManager, const std::function<void()>& onSelectionCleared = nullptr);

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
};
#endif
