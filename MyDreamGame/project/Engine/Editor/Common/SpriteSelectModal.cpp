#ifdef USE_IMGUI
#include "SpriteSelectModal.h"
#include "Graphics/TextureManager.h"
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace {
    std::string GetEllipsisText(const std::string& text, float maxWidth) {
        if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
            return text;
        }
        std::string result = text;
        while (!result.empty() && ImGui::CalcTextSize((result + "...").c_str()).x > maxWidth) {
            result.pop_back();
        }
        return result + "...";
    }
}

SpriteSelectModal::SpriteSelectModal() {
}

void SpriteSelectModal::Initialize() {
    RefreshSpriteList();
}

void SpriteSelectModal::EnsureTextureLoaded(SpriteSelectItem& item) {
    if (item.isResourceLoaded) return;
    item.isResourceLoaded = true;

    try {
        if (std::filesystem::exists(item.fullPath)) {
            uint32_t texIdx = TextureManager::GetInstance()->Load(item.fullPath);
            item.textureGpuHandle = TextureManager::GetInstance()->GetGpuHandle(texIdx);
            item.hasTexture = (item.textureGpuHandle.ptr != 0);
        }
    } catch (...) {
        item.hasTexture = false;
    }
}

void SpriteSelectModal::RefreshSpriteList() {
    spriteList_.clear();
    categories_.clear();
    categories_.push_back("すべて (All)");
    selectedSpriteIdx_ = -1;

    auto scanDir = [&](const std::string& rootDir) {
        if (!std::filesystem::exists(rootDir)) return;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDir)) {
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    std::string ext = path.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });

                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga") {
                        SpriteSelectItem item;
                        std::string fullPath = path.string();
                        std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

                        std::string dirPath = path.parent_path().string();
                        std::replace(dirPath.begin(), dirPath.end(), '\\', '/');

                        std::string subCat = "";
                        std::string spriteRoot = "resources/Sprite";
                        if (dirPath.find(spriteRoot) == 0 && dirPath.length() > spriteRoot.length()) {
                            subCat = dirPath.substr(spriteRoot.length() + 1);
                        } else {
                            subCat = dirPath;
                        }

                        item.fullPath = fullPath;
                        item.directoryPath = dirPath;
                        item.fileName = path.filename().string();
                        item.extension = ext;
                        item.displayName = path.stem().string();
                        item.subCategory = subCat.empty() ? "Sprite" : subCat;

                        if (std::find(categories_.begin(), categories_.end(), item.subCategory) == categories_.end()) {
                            categories_.push_back(item.subCategory);
                        }

                        spriteList_.push_back(item);
                    }
                }
            }
        } catch (...) {}
    };

    scanDir("resources/Sprite");
    scanDir("resources");

    // 重複除去
    std::vector<SpriteSelectItem> uniqueList;
    for (const auto& item : spriteList_) {
        bool exists = false;
        for (const auto& u : uniqueList) {
            if (u.fullPath == item.fullPath) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            uniqueList.push_back(item);
        }
    }
    spriteList_ = uniqueList;
    std::sort(spriteList_.begin(), spriteList_.end(), [](const SpriteSelectItem& a, const SpriteSelectItem& b) {
        return a.fullPath < b.fullPath;
    });
}

void SpriteSelectModal::DrawModal(const char* popupId, bool& isOpen, const std::function<void(const std::string&)>& onSelect) {
    if (!isOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800.0f, 560.0f), ImGuiCond_Appearing);

    bool modalOpen = true;
    if (ImGui::BeginPopupModal(popupId, &modalOpen, ImGuiWindowFlags_NoScrollbar)) {
        if (!modalOpen) {
            isOpen = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        if (spriteList_.empty()) {
            RefreshSpriteList();
        }

        // --- 1. ヘッダー部 ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "スプライト / テクスチャの選択");
        ImGui::SameLine();
        if (ImGui::Button("更新 (Refresh)")) {
            RefreshSpriteList();
        }
        ImGui::SameLine();
        if (ImGui::Button("なし (解除 / デフォルトに戻す)")) {
            if (onSelect) {
                onSelect("");
            }
            isOpen = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        ImGui::Spacing();

        // 検索バー
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##SearchFilter", "ファイル名 / パスで検索...", searchFilter_, sizeof(searchFilter_));
        ImGui::SameLine();
        if (ImGui::Button("クリア##ClearSearch")) {
            searchFilter_[0] = '\0';
        }

        // カテゴリコンボボックス
        ImGui::SameLine();
        ImGui::Text("カテゴリ:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        std::string currentCategoryName = (selectedCategoryIdx_ >= 0 && selectedCategoryIdx_ < static_cast<int>(categories_.size()))
            ? categories_[selectedCategoryIdx_] : "すべて (All)";
        if (ImGui::BeginCombo("##CategoryCombo", currentCategoryName.c_str())) {
            for (int i = 0; i < static_cast<int>(categories_.size()); ++i) {
                bool isSelected = (selectedCategoryIdx_ == i);
                if (ImGui::Selectable(categories_[i].c_str(), isSelected)) {
                    selectedCategoryIdx_ = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        // 絞り込みアイテムの抽出
        std::string filterStr = searchFilter_;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        std::string activeCat = (selectedCategoryIdx_ > 0 && selectedCategoryIdx_ < static_cast<int>(categories_.size()))
            ? categories_[selectedCategoryIdx_] : "";

        std::vector<int> filteredIndices;
        for (int i = 0; i < static_cast<int>(spriteList_.size()); ++i) {
            const auto& item = spriteList_[i];
            if (!activeCat.empty() && item.subCategory != activeCat) {
                continue;
            }
            if (!filterStr.empty()) {
                std::string lowerName = item.fileName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
                std::string lowerPath = item.fullPath;
                std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
                if (lowerName.find(filterStr) == std::string::npos && lowerPath.find(filterStr) == std::string::npos) {
                    continue;
                }
            }
            filteredIndices.push_back(i);
        }

        ImGui::TextDisabled("該当件数: %zu 件", filteredIndices.size());

        // --- 2. メイン表示エリア (サムネイルグリッド) ---
        float footerHeight = 44.0f;
        ImVec2 contentSize = ImVec2(0.0f, ImGui::GetContentRegionAvail().y - footerHeight);
        if (ImGui::BeginChild("SpriteListChild", contentSize, true)) {
            const float itemWidth = 105.0f;
            const float itemHeight = 115.0f;
            float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

            for (size_t fIdx = 0; fIdx < filteredIndices.size(); ++fIdx) {
                int itemIdx = filteredIndices[fIdx];
                auto& item = spriteList_[itemIdx];
                EnsureTextureLoaded(item);

                ImGui::PushID(itemIdx);
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();

                bool isSelected = (selectedSpriteIdx_ == itemIdx);
                ImVec4 bgColor = isSelected ? ImVec4(0.25f, 0.45f, 0.75f, 0.6f) : ImVec4(0.18f, 0.18f, 0.20f, 0.5f);
                ImVec4 borderColor = isSelected ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : ImVec4(0.3f, 0.3f, 0.35f, 0.5f);

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 boxMin = cursorPos;
                ImVec2 boxMax = ImVec2(boxMin.x + itemWidth, boxMin.y + itemHeight);

                // 背景
                drawList->AddRectFilled(boxMin, boxMax, ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);
                drawList->AddRect(boxMin, boxMax, ImGui::ColorConvertFloat4ToU32(borderColor), 4.0f, 0, isSelected ? 2.0f : 1.0f);

                // サムネイル画像描画エリア
                float thumbSize = 72.0f;
                ImVec2 thumbPos = ImVec2(boxMin.x + (itemWidth - thumbSize) * 0.5f, boxMin.y + 6.0f);
                if (item.hasTexture && item.textureGpuHandle.ptr != 0) {
                    drawList->AddImage(
                        (ImTextureID)item.textureGpuHandle.ptr,
                        thumbPos,
                        ImVec2(thumbPos.x + thumbSize, thumbPos.y + thumbSize),
                        ImVec2(0, 0), ImVec2(1, 1)
                    );
                } else {
                    // プレースホルダー四角形
                    drawList->AddRectFilled(
                        thumbPos,
                        ImVec2(thumbPos.x + thumbSize, thumbPos.y + thumbSize),
                        IM_COL32(50, 50, 50, 255), 2.0f
                    );
                    drawList->AddText(
                        ImVec2(thumbPos.x + 12.0f, thumbPos.y + 26.0f),
                        IM_COL32(180, 180, 180, 255),
                        "No Image"
                    );
                }

                // ファイル名表示
                std::string shortText = GetEllipsisText(item.fileName, itemWidth - 10.0f);
                ImVec2 textSize = ImGui::CalcTextSize(shortText.c_str());
                ImVec2 textPos = ImVec2(boxMin.x + (itemWidth - textSize.x) * 0.5f, boxMin.y + 84.0f);
                drawList->AddText(textPos, IM_COL32(230, 230, 230, 255), shortText.c_str());

                // クリック判定用不可視ボタン
                ImGui::SetCursorScreenPos(boxMin);
                if (ImGui::InvisibleButton("##CardBtn", ImVec2(itemWidth, itemHeight))) {
                    selectedSpriteIdx_ = itemIdx;
                }

                // ダブルクリック判定
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    selectedSpriteIdx_ = itemIdx;
                    if (onSelect) {
                        onSelect(item.fullPath);
                    }
                    isOpen = false;
                    ImGui::CloseCurrentPopup();
                    ImGui::PopID();
                    break;
                }

                // ツールチップでフルパスと解像度などの情報を確認可能に
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", item.fileName.c_str());
                    ImGui::Separator();
                    ImGui::TextDisabled("パス: %s", item.fullPath.c_str());
                    ImGui::TextDisabled("カテゴリ: %s", item.subCategory.c_str());
                    if (item.hasTexture && item.textureGpuHandle.ptr != 0) {
                        ImGui::Image((ImTextureID)item.textureGpuHandle.ptr, ImVec2(128.0f, 128.0f));
                    }
                    ImGui::EndTooltip();
                }

                // 改行レイアウト計算
                float lastX2 = boxMax.x;
                float nextX2 = lastX2 + ImGui::GetStyle().ItemSpacing.x + itemWidth;
                if (fIdx + 1 < filteredIndices.size() && nextX2 < windowVisibleX2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // --- 3. フッター部 (選択確定・キャンセル) ---
        ImGui::Spacing();
        std::string selectedInfo = "未選択";
        if (selectedSpriteIdx_ >= 0 && selectedSpriteIdx_ < static_cast<int>(spriteList_.size())) {
            selectedInfo = spriteList_[selectedSpriteIdx_].fullPath;
        }
        ImGui::Text("選択中: %s", selectedInfo.c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 190.0f);

        bool canSelect = (selectedSpriteIdx_ >= 0 && selectedSpriteIdx_ < static_cast<int>(spriteList_.size()));
        if (!canSelect) ImGui::BeginDisabled();
        if (ImGui::Button("適用 (Apply)", ImVec2(85, 26))) {
            if (canSelect && onSelect) {
                onSelect(spriteList_[selectedSpriteIdx_].fullPath);
            }
            isOpen = false;
            ImGui::CloseCurrentPopup();
        }
        if (!canSelect) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(85, 26))) {
            isOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
#endif
