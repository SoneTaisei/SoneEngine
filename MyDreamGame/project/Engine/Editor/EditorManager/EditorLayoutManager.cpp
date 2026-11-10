#ifdef USE_IMGUI
#include "EditorLayoutManager.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

void EditorLayoutManager::ApplyDefaultLayout(ImGuiID explicitDockspaceId) {

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (!viewport)
        return;

    ImGuiID dockspace_id = explicitDockspaceId;
    if (dockspace_id == 0) {
        dockspace_id = ImGui::DockSpaceOverViewport(0, viewport);
    }

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID dock_id_main = dockspace_id;
    // 左側に「ヒエラルキー」
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, NULL, &dock_id_main);
    // 右側に「インスペクター」
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, NULL, &dock_id_main);
    // メインの下側に「マップチップ画面」「リプレイマネージャー」など
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.35f, NULL, &dock_id_main);

    // 各ウィンドウを各ノードに割り当てる
    ImGui::DockBuilderDockWindow("ゲームビュー", dock_id_main);
    ImGui::DockBuilderDockWindow("リプレイエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("アニメーションエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("GPUパーティクルエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("ライトエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_main);
    ImGui::DockBuilderDockWindow("3Dモデル配置", dock_id_main);

    // 左側
    ImGui::DockBuilderDockWindow("ヒエラルキー", dock_id_left);
    ImGui::DockBuilderDockWindow("マイメディア (リプレイ履歴)", dock_id_left);

    // 右側
    ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
    ImGui::DockBuilderDockWindow("ポストエフェクト", dock_id_right);
    ImGui::DockBuilderDockWindow("スプライトアニメーション", dock_id_right);

    // 下側
    ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);
    ImGui::DockBuilderDockWindow("マップ設定", dock_id_bottom);
    ImGui::DockBuilderDockWindow("3Dモデルパレット", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ステージセレクトエディター", dock_id_bottom);
    ImGui::DockBuilderDockWindow("タイムライン", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ドープシート (タイムライン)", dock_id_bottom);
    ImGui::DockBuilderDockWindow("GPUパーティクル タイムライン & エミッター階層", dock_id_bottom);
    ImGui::DockBuilderDockWindow("スポットライト", dock_id_bottom);

    ImGui::DockBuilderFinish(dockspace_id);

    presetStatusMessage_ = "標準レイアウトにリセットしました";
    presetStatusMessageTimer_ = 3.0f;
}

void EditorLayoutManager::ScanLayoutPresets() {
    layoutPresets_.clear();
    std::filesystem::path dirPath("resources/json/local/layout_presets");
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
        return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::ifstream ifs(entry.path());
            if (!ifs.is_open())
                continue;
            try {
                nlohmann::json j;
                ifs >> j;
                WindowLayoutPreset preset;
                preset.name = j.value("name", entry.path().stem().string());
                preset.iniData = j.value("iniData", "");
                preset.showInspector = j.value("showInspector", true);
                preset.showHierarchy = j.value("showHierarchy", true);
                preset.showGameView = j.value("showGameView", true);
                preset.showPostEffect = j.value("showPostEffect", true);
                preset.showMapEditor = j.value("showMapEditor", true);
                preset.showMapSettings = j.value("showMapSettings", true);
                preset.showReplayEditor = j.value("showReplayEditor", true);
                preset.showAnimEditor = j.value("showAnimEditor", true);
                preset.showLightEditor = j.value("showLightEditor", true);
                preset.showSpotLightPanel = j.value("showSpotLightPanel", true);
                preset.showModelPlacement = j.value("showModelPlacement", true);
                preset.showModelPalette = j.value("showModelPalette", true);

                layoutPresets_.push_back(preset);
            } catch (...) {
                // パース失敗時はスキップ
            }
        }
    }
}

void EditorLayoutManager::SaveLayoutPreset(const std::string &name) {
    if (name.empty())
        return;

    std::filesystem::path dirPath("resources/json/local/layout_presets");
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    size_t iniSize = 0;
    const char *iniStr = ImGui::SaveIniSettingsToMemory(&iniSize);

    WindowLayoutPreset preset;
    preset.name = name;
    preset.iniData = (iniStr && iniSize > 0) ? std::string(iniStr, iniSize) : "";
    preset.showInspector = visibility_.showInspector;
    preset.showHierarchy = visibility_.showHierarchy;
    preset.showGameView = visibility_.showGameView;
    preset.showPostEffect = visibility_.showPostEffect;
    preset.showMapEditor = visibility_.showMapEditor;
    preset.showMapSettings = visibility_.showMapSettings;
    preset.showReplayEditor = visibility_.showReplayEditor;
    preset.showAnimEditor = visibility_.showAnimEditor;
    preset.showLightEditor = visibility_.showLightEditor;
    preset.showSpotLightPanel = visibility_.showSpotLightPanel;
    preset.showModelPlacement = visibility_.showModelPlacement;
    preset.showModelPalette = visibility_.showModelPalette;
    preset.showSpriteAnimation = visibility_.showSpriteAnimation;

    nlohmann::json j;
    j["name"] = preset.name;
    j["iniData"] = preset.iniData;
    j["showInspector"] = preset.showInspector;
    j["showHierarchy"] = preset.showHierarchy;
    j["showGameView"] = preset.showGameView;
    j["showPostEffect"] = preset.showPostEffect;
    j["showSpriteAnimation"] = preset.showSpriteAnimation;
    j["showMapEditor"] = preset.showMapEditor;
    j["showMapSettings"] = preset.showMapSettings;
    j["showReplayEditor"] = preset.showReplayEditor;
    j["showAnimEditor"] = preset.showAnimEditor;
    j["showLightEditor"] = preset.showLightEditor;
    j["showSpotLightPanel"] = preset.showSpotLightPanel;
    j["showModelPlacement"] = preset.showModelPlacement;
    j["showModelPalette"] = preset.showModelPalette;

    std::filesystem::path filePath = dirPath / (name + ".json");
    std::ofstream ofs(filePath);
    if (ofs.is_open()) {
        ofs << j.dump(4);
    }

    bool found = false;
    for (auto &p : layoutPresets_) {
        if (p.name == name) {
            p = preset;
            found = true;
            break;
        }
    }
    if (!found) {
        layoutPresets_.push_back(preset);
    }

    presetStatusMessage_ = "プリセット「" + name + "」を保存しました";
    presetStatusMessageTimer_ = 3.0f;
}

bool EditorLayoutManager::ApplyLayoutPreset(const std::string &name) {
    for (const auto &preset : layoutPresets_) {
        if (preset.name == name) {
            visibility_.showInspector = preset.showInspector;
            visibility_.showHierarchy = preset.showHierarchy;
            visibility_.showGameView = preset.showGameView;
            visibility_.showPostEffect = preset.showPostEffect;
            visibility_.showSpriteAnimation = preset.showSpriteAnimation;
            visibility_.showMapEditor = preset.showMapEditor;
            visibility_.showMapSettings = preset.showMapSettings;
            visibility_.showReplayEditor = preset.showReplayEditor;
            visibility_.showAnimEditor = preset.showAnimEditor;
            visibility_.showLightEditor = preset.showLightEditor;
            visibility_.showSpotLightPanel = preset.showSpotLightPanel;
            visibility_.showModelPlacement = preset.showModelPlacement;
            visibility_.showModelPalette = preset.showModelPalette;

            if (!preset.iniData.empty()) {
                ImGui::LoadIniSettingsFromMemory(preset.iniData.c_str(), preset.iniData.size());
            }
            presetStatusMessage_ = "プリセット「" + name + "」を適用しました";
            presetStatusMessageTimer_ = 3.0f;
            return true;
        }
    }
    return false;
}

bool EditorLayoutManager::DeleteLayoutPreset(const std::string &name) {
    std::filesystem::path filePath = std::filesystem::path("resources/json/local/layout_presets") / (name + ".json");
    if (std::filesystem::exists(filePath)) {
        std::filesystem::remove(filePath);
    }

    auto it = std::remove_if(layoutPresets_.begin(), layoutPresets_.end(), [&](const WindowLayoutPreset &p) {
        return p.name == name;
    });
    if (it != layoutPresets_.end()) {
        layoutPresets_.erase(it, layoutPresets_.end());
        presetStatusMessage_ = "プリセット「" + name + "」を削除しました";
        presetStatusMessageTimer_ = 3.0f;
        return true;
    }
    return false;
}

bool EditorLayoutManager::ExportLayoutPresetToFile(const std::string &name, const std::string &filePath) {
    for (const auto &preset : layoutPresets_) {
        if (preset.name == name) {
            nlohmann::json j;
            j["name"] = preset.name;
            j["iniData"] = preset.iniData;
            j["showInspector"] = preset.showInspector;
            j["showHierarchy"] = preset.showHierarchy;
            j["showGameView"] = preset.showGameView;
            j["showPostEffect"] = preset.showPostEffect;
            j["showSpriteAnimation"] = preset.showSpriteAnimation;
            j["showMapEditor"] = preset.showMapEditor;
            j["showMapSettings"] = preset.showMapSettings;
            j["showReplayEditor"] = preset.showReplayEditor;
            j["showAnimEditor"] = preset.showAnimEditor;
            j["showLightEditor"] = preset.showLightEditor;
            j["showSpotLightPanel"] = preset.showSpotLightPanel;
            j["showModelPlacement"] = preset.showModelPlacement;
            j["showModelPalette"] = preset.showModelPalette;

            std::filesystem::path outPath(filePath);
            if (outPath.has_parent_path()) {
                std::filesystem::create_directories(outPath.parent_path());
            }
            std::ofstream ofs(outPath);
            if (ofs.is_open()) {
                ofs << j.dump(4);
                presetStatusMessage_ = "ファイル「" + outPath.string() + "」へ出力しました";
                presetStatusMessageTimer_ = 3.0f;
                return true;
            }
            break;
        }
    }
    presetStatusMessage_ = "出力に失敗しました";
    presetStatusMessageTimer_ = 3.0f;
    return false;
}

bool EditorLayoutManager::ImportLayoutPresetFromFile(const std::string &filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        presetStatusMessage_ = "ファイルが開けませんでした: " + filePath;
        presetStatusMessageTimer_ = 3.0f;
        return false;
    }
    try {
        nlohmann::json j;
        ifs >> j;
        WindowLayoutPreset preset;
        preset.name = j.value("name", std::filesystem::path(filePath).stem().string());
        preset.iniData = j.value("iniData", "");
        preset.showInspector = j.value("showInspector", true);
        preset.showHierarchy = j.value("showHierarchy", true);
        preset.showGameView = j.value("showGameView", true);
        preset.showPostEffect = j.value("showPostEffect", true);
        preset.showSpriteAnimation = j.value("showSpriteAnimation", true);
        preset.showMapEditor = j.value("showMapEditor", true);
        preset.showMapSettings = j.value("showMapSettings", true);
        preset.showReplayEditor = j.value("showReplayEditor", true);
        preset.showAnimEditor = j.value("showAnimEditor", true);
        preset.showLightEditor = j.value("showLightEditor", true);
        preset.showSpotLightPanel = j.value("showSpotLightPanel", true);

        std::filesystem::path dirPath("resources/json/local/layout_presets");
        std::filesystem::create_directories(dirPath);
        std::ofstream ofs(dirPath / (preset.name + ".json"));
        if (ofs.is_open()) {
            ofs << j.dump(4);
        }

        bool found = false;
        for (auto &p : layoutPresets_) {
            if (p.name == preset.name) {
                p = preset;
                found = true;
                break;
            }
        }
        if (!found) {
            layoutPresets_.push_back(preset);
        }
        presetStatusMessage_ = "プリセット「" + preset.name + "」をインポートしました";
        presetStatusMessageTimer_ = 3.0f;
        return true;
    } catch (...) {
        presetStatusMessage_ = "JSONの解析に失敗しました";
        presetStatusMessageTimer_ = 3.0f;
        return false;
    }
}

void EditorLayoutManager::DrawSavePresetModal(bool *pOpen) {
    if (!pOpen || !*pOpen) return;
    if (ImGui::Begin("レイアウトプリセットの保存", pOpen, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("現在の配置をプリセットとして保存します。");
        ImGui::Spacing();
        ImGui::Text("プリセット名:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        bool enterPressed = ImGui::InputText("##PresetNameDialogInput", newPresetNameBuf_, sizeof(newPresetNameBuf_), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("保存", ImVec2(100, 0)) || (enterPressed && strlen(newPresetNameBuf_) > 0)) {
            if (strlen(newPresetNameBuf_) > 0) {
                SaveLayoutPreset(newPresetNameBuf_);
                newPresetNameBuf_[0] = '\0';
                *pOpen = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(100, 0))) {
            *pOpen = false;
        }
    }
    ImGui::End();
}

void EditorLayoutManager::ApplyStyle() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiStyle* style_ptr = &ImGui::GetStyle();
    ImVec4* colors = style_ptr->Colors;

    auto ImVec4FromHex = [](uint32_t hex, float alpha = 1.0f) {
        return ImVec4(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            ((hex >> 0) & 0xFF) / 255.0f,
            alpha
        );
    };

    ImVec4 base = ImVec4FromHex(0x14141E);
    ImVec4 mantle = ImVec4FromHex(0x0F0F16);
    ImVec4 crust = ImVec4FromHex(0x08080C);
    ImVec4 surface0 = ImVec4FromHex(0x232333);
    ImVec4 surface1 = ImVec4FromHex(0x333547);
    ImVec4 surface2 = ImVec4FromHex(0x45475A);
    ImVec4 text = ImVec4FromHex(0xCDD6F4);
    ImVec4 subtext0 = ImVec4FromHex(0xA6ADC8);
    ImVec4 mauve = ImVec4FromHex(0xCBA6F7);
    ImVec4 pink = ImVec4FromHex(0xF5C2E7);

    colors[ImGuiCol_Text]                   = text;
    colors[ImGuiCol_TextDisabled]           = subtext0;
    colors[ImGuiCol_WindowBg]               = base;
    colors[ImGuiCol_ChildBg]                = mantle;
    colors[ImGuiCol_PopupBg]                = mantle;
    colors[ImGuiCol_Border]                 = surface1;
    colors[ImGuiCol_BorderShadow]           = crust;
    colors[ImGuiCol_FrameBg]                = surface0;
    colors[ImGuiCol_FrameBgHovered]         = surface1;
    colors[ImGuiCol_FrameBgActive]          = surface2;
    colors[ImGuiCol_TitleBg]                = mantle;
    colors[ImGuiCol_TitleBgActive]          = base;
    colors[ImGuiCol_TitleBgCollapsed]       = crust;
    colors[ImGuiCol_MenuBarBg]              = mantle;
    colors[ImGuiCol_ScrollbarBg]            = mantle;
    colors[ImGuiCol_ScrollbarGrab]          = surface0;
    colors[ImGuiCol_ScrollbarGrabHovered]   = surface1;
    colors[ImGuiCol_ScrollbarGrabActive]    = surface2;
    colors[ImGuiCol_CheckMark]              = mauve;
    colors[ImGuiCol_SliderGrab]             = mauve;
    colors[ImGuiCol_SliderGrabActive]       = pink;
    colors[ImGuiCol_Button]                 = surface0;
    colors[ImGuiCol_ButtonHovered]          = surface1;
    colors[ImGuiCol_ButtonActive]           = surface2;
    colors[ImGuiCol_Header]                 = surface1;
    colors[ImGuiCol_HeaderHovered]          = ImVec4(mauve.x, mauve.y, mauve.z, 0.6f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(mauve.x, mauve.y, mauve.z, 0.8f);
    colors[ImGuiCol_Separator]              = surface1;
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(mauve.x, mauve.y, mauve.z, 0.6f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(mauve.x, mauve.y, mauve.z, 0.8f);
    colors[ImGuiCol_ResizeGrip]             = surface0;
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(mauve.x, mauve.y, mauve.z, 0.6f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(mauve.x, mauve.y, mauve.z, 0.8f);
    colors[ImGuiCol_Tab]                    = mantle;
    colors[ImGuiCol_TabHovered]             = surface1;
    colors[ImGuiCol_TabActive]              = surface0;
    colors[ImGuiCol_TabUnfocused]           = mantle;
    colors[ImGuiCol_TabUnfocusedActive]     = base;
    colors[ImGuiCol_DockingPreview]         = ImVec4(mauve.x, mauve.y, mauve.z, 0.3f);
    colors[ImGuiCol_DockingEmptyBg]         = crust;

    style_ptr->WindowRounding    = 8.0f;
    style_ptr->ChildRounding     = 6.0f;
    style_ptr->FrameRounding     = 5.0f;
    style_ptr->PopupRounding     = 6.0f;
    style_ptr->ScrollbarRounding = 9.0f;
    style_ptr->GrabRounding      = 5.0f;
    style_ptr->TabRounding       = 6.0f;
    style_ptr->WindowBorderSize  = 1.0f;
    style_ptr->FrameBorderSize   = 0.0f;
    style_ptr->PopupBorderSize   = 1.0f;
    style_ptr->WindowPadding     = ImVec2(10.0f, 10.0f);
    style_ptr->FramePadding      = ImVec2(8.0f,  4.0f);
    style_ptr->ItemSpacing       = ImVec2(8.0f,  6.0f);
    style_ptr->ItemInnerSpacing  = ImVec2(6.0f,  4.0f);
    style_ptr->IndentSpacing     = 18.0f;
    style_ptr->ScrollbarSize     = 12.0f;
    style_ptr->GrabMinSize       = 10.0f;
}

#endif // USE_IMGUI