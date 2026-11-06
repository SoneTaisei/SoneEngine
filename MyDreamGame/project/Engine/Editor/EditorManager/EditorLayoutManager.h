#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>

class EditorLayoutManager {
public:
    struct WindowLayoutPreset {
        std::string name;
        std::string iniData;
        bool showInspector = true;
        bool showHierarchy = true;
        bool showGameView = true;
        bool showPostEffect = true;
        bool showMapEditor = true;
        bool showMapSettings = true;
        bool showReplayEditor = true;
        bool showAnimEditor = true;
        bool showLightEditor = true;
        bool showSpotLightPanel = true;
        bool showModelPlacement = true;
        bool showModelPalette = true;
        bool showSpriteAnimation = true;
    };

    EditorLayoutManager() = default;
    ~EditorLayoutManager() = default;

    void ScanLayoutPresets();
    void SaveLayoutPreset(const std::string &name);
    bool ApplyLayoutPreset(const std::string &name);
    bool DeleteLayoutPreset(const std::string &name);
    bool ExportLayoutPresetToFile(const std::string &name, const std::string &filePath);
    bool ImportLayoutPresetFromFile(const std::string &filePath);
    void ApplyDefaultLayout();
    void ApplyStyle();

    void DrawSavePresetModal(bool *pOpen);

    WindowLayoutPreset& GetVisibility() { return visibility_; }
    const WindowLayoutPreset& GetVisibility() const { return visibility_; }




    const std::vector<WindowLayoutPreset> &GetLayoutPresets() const { return layoutPresets_; }
    bool IsShowSavePresetWindow() const { return showSavePresetWindow_; }
    void SetShowSavePresetWindow(bool show) { showSavePresetWindow_ = show; }

    const std::string &GetPresetStatusMessage() const { return presetStatusMessage_; }
    void SetPresetStatusMessage(const std::string &msg, float timer = 3.0f) {
        presetStatusMessage_ = msg;
        presetStatusMessageTimer_ = timer;
    }

private:
    std::vector<WindowLayoutPreset> layoutPresets_;
    bool showSavePresetWindow_ = false;
    char newPresetNameBuf_[128] = "";
    std::string presetStatusMessage_ = "";
    float presetStatusMessageTimer_ = 0.0f;
    WindowLayoutPreset visibility_;
};
#endif