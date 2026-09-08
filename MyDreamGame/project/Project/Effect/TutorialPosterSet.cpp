#include "TutorialPosterSet.h"
#include "Core/Utility/UtilityFunctions.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scenes/BlockDesignPanel.h"
#endif

namespace {
    constexpr const char* kSheetDir = "resources/Sprite/anim";
    constexpr const char* kConfigDir = "resources/json/shared/Tutorial";
}

std::string TutorialPosterSet::ConfigPathFor(const std::string& mapFilePath) {
    std::string stem = std::filesystem::path(mapFilePath).stem().string();
    if (stem.empty()) stem = "default";
    return std::string(kConfigDir) + "/" + stem + "_posters.json";
}

void TutorialPosterSet::Initialize(ID3D12Device* device, const std::string& mapFilePath) {
    device_ = device;
    mapFilePath_ = mapFilePath;
    configPath_ = ConfigPathFor(mapFilePath);
    entries_.clear();
    selected_ = -1;
    dirty_ = false;
    hasFile_ = Load();
}

void TutorialPosterSet::RebindMap(const std::string& mapFilePath) {
    if (mapFilePath.empty() || mapFilePath == mapFilePath_) return;
    mapFilePath_ = mapFilePath;
    configPath_ = ConfigPathFor(mapFilePath);
    // そのマップの保存があれば読み直す。無ければ今の配置（初期配置）のまま、保存先だけ付け替える
    if (Load()) {
        hasFile_ = true;
    } else {
        hasFile_ = false;
        Log("TutorialPosterSet: rebound to " + configPath_ + " (no file yet)\n");
    }
}

bool TutorialPosterSet::Load() {
    std::ifstream ifs(configPath_);
    if (!ifs) return false;
    nlohmann::json j;
    try {
        ifs >> j;
    } catch (...) {
        Log("TutorialPosterSet: parse error: " + configPath_ + "\n");
        return false;
    }
    entries_.clear();
    selected_ = -1;
    if (j.contains("posters") && j["posters"].is_array()) {
        for (const auto& p : j["posters"]) {
            Entry e;
            e.name = p.value("name", std::string("説明"));
            e.sheet = p.value("sheet", std::string());
            e.x = p.value("x", 0.0f);
            e.y = p.value("y", 0.0f);
            e.width = p.value("width", 14.0f);
            e.triggerX = p.value("triggerX", e.x);
            e.triggerY = p.value("triggerY", e.y);
            e.triggerW = p.value("triggerW", 6.0f);
            e.triggerH = p.value("triggerH", 4.0f);
            e.showDist = p.value("showDist", 3.0f);
            e.hideDist = p.value("hideDist", 5.5f);
            e.alwaysShow = p.value("alwaysShow", false);
            Add(e);
        }
    }
    dirty_ = false;
    Log("TutorialPosterSet: loaded " + std::to_string(entries_.size()) + " posters from " + configPath_ + "\n");
    return true;
}

bool TutorialPosterSet::Save() {
    nlohmann::json j;
    j["map"] = mapFilePath_;
    j["posters"] = nlohmann::json::array();
    for (const auto& e : entries_) {
        nlohmann::json p;
        p["name"] = e->name;
        p["sheet"] = e->sheet;
        p["x"] = e->x;
        p["y"] = e->y;
        p["width"] = e->width;
        p["triggerX"] = e->triggerX;
        p["triggerY"] = e->triggerY;
        p["triggerW"] = e->triggerW;
        p["triggerH"] = e->triggerH;
        p["showDist"] = e->showDist;
        p["hideDist"] = e->hideDist;
        p["alwaysShow"] = e->alwaysShow;
        j["posters"].push_back(p);
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(configPath_).parent_path(), ec);
    std::ofstream ofs(configPath_);
    if (!ofs) {
        Log("TutorialPosterSet: save failed: " + configPath_ + "\n");
        return false;
    }
    ofs << j.dump(2);
    dirty_ = false;
    Log("TutorialPosterSet: saved " + std::to_string(entries_.size()) + " posters to " + configPath_ + "\n");
    return true;
}

TutorialPosterSet::Entry& TutorialPosterSet::Add(const Entry& src) {
    auto e = std::make_unique<Entry>();
    e->name = src.name;
    e->sheet = src.sheet;
    e->x = src.x; e->y = src.y; e->width = src.width;
    e->triggerX = src.triggerX; e->triggerY = src.triggerY; e->triggerW = src.triggerW; e->triggerH = src.triggerH;
    e->showDist = src.showDist; e->hideDist = src.hideDist;
    e->alwaysShow = src.alwaysShow;
    entries_.push_back(std::move(e));
    Rebuild(*entries_.back());
    return *entries_.back();
}

void TutorialPosterSet::Remove(size_t index) {
    if (index >= entries_.size()) return;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
    if (selected_ >= static_cast<int>(entries_.size())) selected_ = static_cast<int>(entries_.size()) - 1;
}

void TutorialPosterSet::Rebuild(Entry& e) {
    e.poster.reset();
    if (!device_ || e.sheet.empty()) return;
    auto p = std::make_unique<TutorialPoster>();
    p->Initialize(device_, e.sheet);
    if (!p->IsValid()) {
        Log("TutorialPosterSet: sheet not usable: " + e.sheet + "\n");
        return;
    }
    e.poster = std::move(p);
    Apply(e);
}

void TutorialPosterSet::Apply(Entry& e) {
    if (!e.poster) return;
    const float h = e.width * e.poster->GetAspect();
    e.poster->SetPlacement({ e.x, e.y, 0.0f }, e.width, h);
    e.poster->SetTarget(e.triggerX - e.triggerW * 0.5f, e.triggerX + e.triggerW * 0.5f,
                        e.triggerY - e.triggerH * 0.5f, e.triggerY + e.triggerH * 0.5f);
    e.poster->SetDistances(e.showDist, (std::max)(e.hideDist, e.showDist + 0.5f));
    e.poster->SetAlwaysShow(e.alwaysShow);
    e.poster->SetPreview(preview_);
}

void TutorialPosterSet::Update(float dt, const Vector3& playerPos, bool active) {
    for (auto& e : entries_) {
        if (e->poster) e->poster->Update(dt, playerPos, active);
    }
}

void TutorialPosterSet::Draw() {
    for (auto& e : entries_) {
        if (e->poster) e->poster->Draw();
    }
}

std::vector<std::string> TutorialPosterSet::ListSheets() {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& it : std::filesystem::directory_iterator(kSheetDir, ec)) {
        if (!it.is_regular_file()) continue;
        if (it.path().extension() != ".json") continue;
        out.push_back(it.path().generic_string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string TutorialPosterSet::SheetTitle(const std::string& sheetPath) {
    std::ifstream ifs(sheetPath);
    if (ifs) {
        try {
            nlohmann::json j;
            ifs >> j;
            std::string t = j.value("title", std::string());
            if (!t.empty()) return t;
        } catch (...) {
        }
    }
    return std::filesystem::path(sheetPath).stem().string();
}

void TutorialPosterSet::DrawImGui(Camera* camera, const Vector3& playerPos) {
#ifdef USE_IMGUI
    ImGui::TextDisabled("保存先: %s", configPath_.c_str());
    ImGui::TextWrapped("映像（resources/Sprite/anim の JSON）を選び、中心・幅・出す範囲を決めて保存します。位置の単位はマス（ブロック 1 個 = 1）。");
    if (ImGui::Checkbox("編集中は全部見せる (近づかなくても表示)", &preview_)) {
        for (auto& e : entries_) if (e->poster) e->poster->SetPreview(preview_);
    }

    static std::vector<std::string> sheets;
    static std::vector<std::string> titles;
    static float refreshTimer = 0.0f;
    refreshTimer -= ImGui::GetIO().DeltaTime;
    if (sheets.empty() || refreshTimer <= 0.0f || ImGui::Button("映像の一覧を更新")) {
        sheets = ListSheets();
        titles.clear();
        for (const auto& s : sheets) titles.push_back(SheetTitle(s) + "  (" + std::filesystem::path(s).filename().string() + ")");
        refreshTimer = 5.0f;
    }

    if (ImGui::Button("追加 (プレイヤーの近くに置く)")) {
        Entry e;
        e.name = "説明 " + std::to_string(entries_.size() + 1);
        e.sheet = sheets.empty() ? std::string() : sheets.front();
        e.width = 14.0f;
        e.x = playerPos.x; e.y = playerPos.y + 5.0f;
        e.triggerX = playerPos.x; e.triggerY = playerPos.y;
        Add(e);
        selected_ = static_cast<int>(entries_.size()) - 1;
        dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("保存 (Save)")) Save();
    ImGui::SameLine();
    if (ImGui::Button("読み直し (Reload)")) { if (!Load()) { entries_.clear(); } }
    if (dirty_) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "※ 未保存の変更があります（保存を押すと %s に書きます）", configPath_.c_str());
    ImGui::Separator();

    int removeIndex = -1;
    int duplicateIndex = -1;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        Entry& e = *entries_[i];
        ImGui::PushID(i);
        const bool isSel = (selected_ == i);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | (isSel ? ImGuiTreeNodeFlags_Selected : 0);
        std::string label = std::to_string(i + 1) + ": " + e.name + (e.poster ? "" : "  [映像なし]") + "###poster";
        bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (ImGui::IsItemClicked()) selected_ = i;
        if (open) {
            char nameBuf[128];
            snprintf(nameBuf, sizeof(nameBuf), "%s", e.name.c_str());
            if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf))) { e.name = nameBuf; dirty_ = true; }

            // 映像の選択
            int cur = -1;
            for (int s = 0; s < static_cast<int>(sheets.size()); ++s) if (sheets[s] == e.sheet) cur = s;
            const char* curLabel = (cur >= 0) ? titles[cur].c_str() : (e.sheet.empty() ? "（未選択）" : e.sheet.c_str());
            if (ImGui::BeginCombo("映像 (シート)", curLabel)) {
                for (int s = 0; s < static_cast<int>(sheets.size()); ++s) {
                    bool sel = (s == cur);
                    if (ImGui::Selectable(titles[s].c_str(), sel)) {
                        e.sheet = sheets[s];
                        Rebuild(e);
                        dirty_ = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            bool changed = false;
            float pos[2] = { e.x, e.y };
            if (ImGui::DragFloat2("中心 X / Y", pos, 0.25f)) { e.x = pos[0]; e.y = pos[1]; changed = true; }
            if (ImGui::DragFloat("幅 (マス)", &e.width, 0.25f, 2.0f, 80.0f)) changed = true;
            if (e.poster) ImGui::TextDisabled("高さ: %.1f マス（コマの縦横比から）", e.width * e.poster->GetAspect());
            if (ImGui::Button("ポスターをプレイヤーの上へ")) {
                e.x = playerPos.x;
                e.y = playerPos.y + 1.0f + (e.poster ? e.width * e.poster->GetAspect() * 0.5f : 3.5f);
                changed = true;
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "【出す範囲】プレイヤーがこの範囲に近づくと現れる");
            if (ImGui::Checkbox("常に表示 (近づかなくても出す)", &e.alwaysShow)) changed = true;
            float tc[2] = { e.triggerX, e.triggerY };
            if (ImGui::DragFloat2("範囲の中心 X / Y", tc, 0.25f)) { e.triggerX = tc[0]; e.triggerY = tc[1]; changed = true; }
            float ts[2] = { e.triggerW, e.triggerH };
            if (ImGui::DragFloat2("範囲の大きさ 幅 / 高さ", ts, 0.25f, 0.5f, 200.0f)) { e.triggerW = ts[0]; e.triggerH = ts[1]; changed = true; }
            float dist[2] = { e.showDist, e.hideDist };
            if (ImGui::DragFloat2("出す距離 / 消す距離", dist, 0.1f, 0.0f, 100.0f)) { e.showDist = dist[0]; e.hideDist = dist[1]; changed = true; }
            if (ImGui::Button("範囲をプレイヤーの位置へ")) { e.triggerX = playerPos.x; e.triggerY = playerPos.y; changed = true; }
            ImGui::SameLine();
            if (ImGui::Button("範囲をポスターの真下へ")) { e.triggerX = e.x; e.triggerY = e.y - (e.poster ? e.width * e.poster->GetAspect() * 0.5f : 3.5f) - 1.5f; changed = true; }

            if (changed) { Apply(e); dirty_ = true; }
            if (e.poster) ImGui::TextDisabled("今: %s", e.poster->IsVisible() ? "見えている" : "消えている");

            ImGui::Spacing();
            if (ImGui::Button("複製")) duplicateIndex = i;
            ImGui::SameLine();
            if (ImGui::Button("削除")) removeIndex = i;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (duplicateIndex >= 0) {
        Entry copy;
        const Entry& src = *entries_[duplicateIndex];
        copy.name = src.name + " のコピー"; copy.sheet = src.sheet;
        copy.x = src.x + 2.0f; copy.y = src.y; copy.width = src.width;
        copy.triggerX = src.triggerX + 2.0f; copy.triggerY = src.triggerY; copy.triggerW = src.triggerW; copy.triggerH = src.triggerH;
        copy.showDist = src.showDist; copy.hideDist = src.hideDist; copy.alwaysShow = src.alwaysShow;
        Add(copy);
        selected_ = static_cast<int>(entries_.size()) - 1;
        dirty_ = true;
    }
    if (removeIndex >= 0) { Remove(static_cast<size_t>(removeIndex)); dirty_ = true; }

    // ---- ゲームビューへの枠の重ね描き（水色 = ポスター、黄色 = 出す範囲） ----
    if (camera) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            const Entry& e = *entries_[i];
            const float h = e.width * (e.poster ? e.poster->GetAspect() : 0.5f);
            const bool sel = (selected_ == i);
            float x0, y0, x1, y1;
            if (BlockDesignPanel::WorldToScreen(camera, { e.x - e.width * 0.5f, e.y + h * 0.5f, 0.0f }, x0, y0) &&
                BlockDesignPanel::WorldToScreen(camera, { e.x + e.width * 0.5f, e.y - h * 0.5f, 0.0f }, x1, y1)) {
                ImVec2 a((std::min)(x0, x1), (std::min)(y0, y1)), b((std::max)(x0, x1), (std::max)(y0, y1));
                dl->AddRect(a, b, sel ? IM_COL32(80, 220, 255, 255) : IM_COL32(80, 220, 255, 140), 0.0f, 0, sel ? 3.0f : 1.5f);
                std::string tag = std::to_string(i + 1) + ": " + e.name;
                dl->AddText(ImVec2(a.x + 4.0f, a.y + 2.0f), IM_COL32(80, 220, 255, 255), tag.c_str());
            }
            if (BlockDesignPanel::WorldToScreen(camera, { e.triggerX - e.triggerW * 0.5f, e.triggerY + e.triggerH * 0.5f, 0.0f }, x0, y0) &&
                BlockDesignPanel::WorldToScreen(camera, { e.triggerX + e.triggerW * 0.5f, e.triggerY - e.triggerH * 0.5f, 0.0f }, x1, y1)) {
                ImVec2 a((std::min)(x0, x1), (std::min)(y0, y1)), b((std::max)(x0, x1), (std::max)(y0, y1));
                dl->AddRect(a, b, sel ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 220, 60, 140), 0.0f, 0, sel ? 3.0f : 1.5f);
            }
        }
    }
#else
    (void)camera; (void)playerPos;
#endif
}
