#ifdef USE_IMGUI
#include "Model3DEditorPalette.h"
#include "Model3DEditorContext.h"
#include "Scene/SceneManager.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Model/Model.h"
#include "Graphics/TextureManager.h"
#include <filesystem>
#include <algorithm>
#include <cstring>
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

    void BuildModelPreviewTriangles(ModelAssetItem& item, float boxSize, ImVec4 baseColor) {
        item.cachedTriangles.clear();
        item.isPreviewGenerated = true;
        if (!item.modelPtr) return;

        const auto& modelData = item.modelPtr->GetModelData();
        if (modelData.vertices.empty()) return;

        // 1. バウンディングボックスの計算
        Vector3 bMin = { 1e9f, 1e9f, 1e9f };
        Vector3 bMax = { -1e9f, -1e9f, -1e9f };
        for (const auto& v : modelData.vertices) {
            bMin.x = (std::min)(bMin.x, v.position.x);
            bMin.y = (std::min)(bMin.y, v.position.y);
            bMin.z = (std::min)(bMin.z, v.position.z);
            bMax.x = (std::max)(bMax.x, v.position.x);
            bMax.y = (std::max)(bMax.y, v.position.y);
            bMax.z = (std::max)(bMax.z, v.position.z);
        }
        Vector3 bCenter = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f, (bMin.z + bMax.z) * 0.5f };
        Vector3 bSize = { bMax.x - bMin.x, bMax.y - bMin.y, bMax.z - bMin.z };
        float maxDim = (std::max)({ bSize.x, bSize.y, bSize.z, 0.001f });
        float scale = (boxSize * 0.70f) / maxDim;

        // 2. 回転（斜めアングル: Yaw = 35度, Pitch = 22度）
        float yaw = 35.0f * 3.14159265f / 180.0f;
        float pitch = 22.0f * 3.14159265f / 180.0f;
        float cosY = std::cos(yaw), sinY = std::sin(yaw);
        float cosP = std::cos(pitch), sinP = std::sin(pitch);

        auto TransformVertex = [&](const Vector4& pos) -> Vector3 {
            float x = (pos.x - bCenter.x) * scale;
            float y = (pos.y - bCenter.y) * scale;
            float z = (pos.z - bCenter.z) * scale;

            float x1 = x * cosY + z * sinY;
            float y1 = y;
            float z1 = -x * sinY + z * cosY;

            float x2 = x1;
            float y2 = y1 * cosP - z1 * sinP;
            float z2 = y1 * sinP + z1 * cosP;

            return { x2, y2, z2 };
        };

        size_t numIndices = modelData.indices.size();
        size_t totalTriangles = numIndices > 0 ? numIndices / 3 : modelData.vertices.size() / 3;
        if (totalTriangles == 0) return;

        Vector3 lightDir = { 0.4f, 0.8f, 0.5f };
        float lightLen = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
        if (lightLen > 1e-4f) {
            lightDir.x /= lightLen; lightDir.y /= lightLen; lightDir.z /= lightLen;
        }

        item.cachedTriangles.reserve(totalTriangles);

        for (size_t t = 0; t < totalTriangles; ++t) {
            size_t idx0 = t * 3;
            size_t idx1 = t * 3 + 1;
            size_t idx2 = t * 3 + 2;
            if (numIndices > 0) {
                idx0 = modelData.indices[idx0];
                idx1 = modelData.indices[idx1];
                idx2 = modelData.indices[idx2];
            }
            if (idx0 >= modelData.vertices.size() || idx1 >= modelData.vertices.size() || idx2 >= modelData.vertices.size()) continue;

            const auto& v0 = modelData.vertices[idx0];
            const auto& v1 = modelData.vertices[idx1];
            const auto& v2 = modelData.vertices[idx2];

            Vector3 t0 = TransformVertex(v0.position);
            Vector3 t1 = TransformVertex(v1.position);
            Vector3 t2 = TransformVertex(v2.position);

            ImVec2 s0(t0.x, -t0.y);
            ImVec2 s1(t1.x, -t1.y);
            ImVec2 s2(t2.x, -t2.y);

            Vector3 edge1 = { t1.x - t0.x, t1.y - t0.y, t1.z - t0.z };
            Vector3 edge2 = { t2.x - t0.x, t2.y - t0.y, t2.z - t0.z };
            Vector3 norm = {
                edge1.y * edge2.z - edge1.z * edge2.y,
                edge1.z * edge2.x - edge1.x * edge2.z,
                edge1.x * edge2.y - edge1.y * edge2.x
            };
            float normLen = std::sqrt(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
            float ndotl = 0.6f;
            if (normLen > 0.0001f) {
                norm.x /= normLen; norm.y /= normLen; norm.z /= normLen;
                float dot = norm.x * lightDir.x + norm.y * lightDir.y + norm.z * lightDir.z;
                ndotl = (std::max)(0.25f, std::abs(dot));
            }

            float r = (std::min)(1.0f, baseColor.x * (0.35f + 0.65f * ndotl));
            float g = (std::min)(1.0f, baseColor.y * (0.35f + 0.65f * ndotl));
            float b = (std::min)(1.0f, baseColor.z * (0.35f + 0.65f * ndotl));
            ImU32 col = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(baseColor.w * 255));

            PreviewTriangle tri;
            tri.offset[0] = s0; tri.offset[1] = s1; tri.offset[2] = s2;
            tri.uv[0] = ImVec2(v0.texcoord.x, v0.texcoord.y);
            tri.uv[1] = ImVec2(v1.texcoord.x, v1.texcoord.y);
            tri.uv[2] = ImVec2(v2.texcoord.x, v2.texcoord.y);
            tri.avgZ = (t0.z + t1.z + t2.z) / 3.0f;
            tri.col = col;
            item.cachedTriangles.push_back(tri);
        }

        std::sort(item.cachedTriangles.begin(), item.cachedTriangles.end(), [](const PreviewTriangle& a, const PreviewTriangle& b) {
            return a.avgZ < b.avgZ;
        });
    }

    bool DrawCachedModelPreview(ImDrawList* drawList, ImVec2 center, const ModelAssetItem& item) {
        if (item.cachedTriangles.empty()) return false;

        if (item.hasTexture && item.textureGpuHandle.ptr != 0) {
            drawList->PushTextureID((ImTextureID)item.textureGpuHandle.ptr);
            for (const auto& tri : item.cachedTriangles) {
                drawList->PrimReserve(3, 3);
                ImDrawIdx vidx = drawList->_VtxCurrentIdx;
                drawList->PrimWriteIdx(vidx);
                drawList->PrimWriteIdx(static_cast<ImDrawIdx>(vidx + 1));
                drawList->PrimWriteIdx(static_cast<ImDrawIdx>(vidx + 2));
                drawList->PrimWriteVtx(ImVec2(center.x + tri.offset[0].x, center.y + tri.offset[0].y), tri.uv[0], tri.col);
                drawList->PrimWriteVtx(ImVec2(center.x + tri.offset[1].x, center.y + tri.offset[1].y), tri.uv[1], tri.col);
                drawList->PrimWriteVtx(ImVec2(center.x + tri.offset[2].x, center.y + tri.offset[2].y), tri.uv[2], tri.col);
            }
            drawList->PopTextureID();
        } else {
            for (const auto& tri : item.cachedTriangles) {
                drawList->AddTriangleFilled(
                    ImVec2(center.x + tri.offset[0].x, center.y + tri.offset[0].y),
                    ImVec2(center.x + tri.offset[1].x, center.y + tri.offset[1].y),
                    ImVec2(center.x + tri.offset[2].x, center.y + tri.offset[2].y),
                    tri.col
                );
            }
        }
        return true;
    }

    void Draw3DIsometricBoxFallback(ImDrawList* drawList, ImVec2 center, float size, ImVec4 baseColor) {
        float rx = size * 0.866f;
        float ry = size * 0.5f;
        float h = size * 1.15f;

        float topCy = center.y - h * 0.5f;
        float botCy = center.y + h * 0.5f;

        ImVec2 pTop(center.x, topCy - ry);
        ImVec2 pRight(center.x + rx, topCy);
        ImVec2 pBottom(center.x, topCy + ry);
        ImVec2 pLeft(center.x - rx, topCy);

        ImVec2 pBotRight(center.x + rx, botCy);
        ImVec2 pBotCenter(center.x, botCy + ry);
        ImVec2 pBotLeft(center.x - rx, botCy);

        auto AdjustColor = [](ImVec4 c, float factor) -> ImU32 {
            float r = (std::min)(1.0f, c.x * factor);
            float g = (std::min)(1.0f, c.y * factor);
            float b = (std::min)(1.0f, c.z * factor);
            return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(c.w * 255));
        };

        ImU32 colTop = AdjustColor(baseColor, 1.25f);
        ImU32 colRight = AdjustColor(baseColor, 0.85f);
        ImU32 colLeft = AdjustColor(baseColor, 0.65f);
        ImU32 colBorder = AdjustColor(baseColor, 0.35f);

        drawList->AddQuadFilled(pLeft, pBottom, pBotCenter, pBotLeft, colLeft);
        drawList->AddQuadFilled(pBottom, pRight, pBotRight, pBotCenter, colRight);
        drawList->AddQuadFilled(pTop, pRight, pBottom, pLeft, colTop);

        drawList->AddLine(pTop, pRight, colBorder, 1.0f);
        drawList->AddLine(pRight, pBottom, colBorder, 1.0f);
        drawList->AddLine(pBottom, pLeft, colBorder, 1.0f);
        drawList->AddLine(pLeft, pTop, colBorder, 1.0f);

        drawList->AddLine(pLeft, pBotLeft, colBorder, 1.0f);
        drawList->AddLine(pBottom, pBotCenter, colBorder, 1.0f);
        drawList->AddLine(pRight, pBotRight, colBorder, 1.0f);

        drawList->AddLine(pBotLeft, pBotCenter, colBorder, 1.0f);
        drawList->AddLine(pBotCenter, pBotRight, colBorder, 1.0f);
    }
}

Model3DEditorPalette::Model3DEditorPalette(Model3DEditorContext* context)
    : context_(context) {
}

void Model3DEditorPalette::Initialize() {
    RefreshModelList();
}

void Model3DEditorPalette::EnsureModelLoaded(ModelAssetItem& item) {
    if (item.isResourceLoaded) return;
    item.isResourceLoaded = true;

    try {
        item.modelPtr = ModelManager::GetInstance()->GetModel(item.directoryPath, item.fileName);
        if (item.modelPtr) {
            const std::string& texPath = item.modelPtr->GetModelData().material.textureFilePath;
            if (!texPath.empty() && std::filesystem::exists(texPath)) {
                uint32_t texIdx = TextureManager::GetInstance()->Load(texPath);
                item.textureGpuHandle = TextureManager::GetInstance()->GetGpuHandle(texIdx);
                item.hasTexture = true;
            }
        }
    } catch (...) {
        item.modelPtr = nullptr;
    }
}

void Model3DEditorPalette::RefreshModelList() {
    modelList_.clear();

    auto scanDir = [&](const std::string& rootDir) {
        if (!std::filesystem::exists(rootDir)) return;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDir)) {
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    std::string ext = path.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });

                    if (ext == ".obj" || ext == ".gltf" || ext == ".fbx") {
                        ModelAssetItem item;
                        std::string fullPath = path.string();
                        std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

                        std::string dirPath = path.parent_path().string();
                        std::replace(dirPath.begin(), dirPath.end(), '\\', '/');

                        item.fullPath = fullPath;
                        item.directoryPath = dirPath;
                        item.fileName = path.filename().string();
                        item.extension = ext;
                        item.displayName = path.stem().string();

                        modelList_.push_back(item);
                    }
                }
            }
        } catch (...) {}
    };

    scanDir("resources/Object");
    scanDir("resources/models");
    scanDir("resources");

    // Remove duplicates based on fullPath
    std::vector<ModelAssetItem> uniqueList;
    for (const auto& item : modelList_) {
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
    modelList_ = uniqueList;
}

void Model3DEditorPalette::Draw(bool& showModelPalette, SceneManager* sceneManager) {
    if (!showModelPalette) return;

    if (ImGui::Begin("3Dモデルパレット", &showModelPalette)) {
        // --- ツールバー ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "3Dモデルパレット");
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu models)", modelList_.size());
        ImGui::SameLine();
        if (ImGui::Button("再読み込み (Refresh)")) {
            RefreshModelList();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##Filter", "モデル名で検索...", searchFilter_, sizeof(searchFilter_));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("※ タイルを3Dモデル配置画面へドラッグ＆ドロップして配置できます (ダブルクリックで原点配置)");
        ImGui::Spacing();

        std::string filterStr = searchFilter_;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), [](unsigned char c) { return (char)::tolower(c); });

        // Filtered items list
        std::vector<int> filteredIndices;
        for (int i = 0; i < static_cast<int>(modelList_.size()); ++i) {
            const auto& item = modelList_[i];
            std::string itemLower = item.displayName + " " + item.fileName;
            std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), [](unsigned char c) { return (char)::tolower(c); });

            if (!filterStr.empty() && itemLower.find(filterStr) == std::string::npos) {
                continue;
            }
            filteredIndices.push_back(i);
        }

        const float cardWidth = 84.0f;
        const float cardHeight = 96.0f;
        const float itemSpacing = 8.0f;
        float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        for (size_t fIdx = 0; fIdx < filteredIndices.size(); ++fIdx) {
            int i = filteredIndices[fIdx];
            auto& item = modelList_[i];
            EnsureModelLoaded(item);

            ImGui::PushID(i);

            ImVec2 p = ImGui::GetCursorScreenPos();
            bool isSelected = (selectedModelIdx_ == i);

            // Invisible button for interaction
            std::string btnId = "##ModelCard_" + std::to_string(i);
            bool clicked = ImGui::InvisibleButton(btnId.c_str(), ImVec2(cardWidth, cardHeight));
            bool isHovered = ImGui::IsItemHovered();

            if (clicked) {
                selectedModelIdx_ = i;
            }

            // Double click to place at center
            if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (context_) {
                    context_->AddObject(item.displayName, item.directoryPath, item.fileName, Vector3{ 0.0f, 0.0f, 0.0f });
                }
            }

            // Drag & Drop Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string payloadData = item.directoryPath + "|" + item.fileName + "|" + item.displayName;
                ImGui::SetDragDropPayload("DND_3D_MODEL", payloadData.c_str(), payloadData.length() + 1);
                
                // Drag preview tooltip
                ImGui::Text("配置: %s", item.displayName.c_str());
                ImGui::EndDragDropSource();
            }

            // --- カード背景描画 ---
            ImU32 bgCol = isSelected ? IM_COL32(45, 60, 95, 255)
                        : isHovered  ? IM_COL32(45, 52, 68, 255)
                                     : IM_COL32(30, 34, 44, 240);

            ImU32 borderCol = isSelected ? IM_COL32(255, 205, 60, 255)
                            : isHovered  ? IM_COL32(100, 150, 240, 230)
                                         : IM_COL32(55, 60, 75, 200);

            drawList->AddRectFilled(p, ImVec2(p.x + cardWidth, p.y + cardHeight), bgCol, 6.0f);
            drawList->AddRect(p, ImVec2(p.x + cardWidth, p.y + cardHeight), borderCol, 6.0f, 0, isSelected ? 2.0f : 1.0f);

            // 画面外にスクロールされているカードのプレビュー描画はスキップ（カリング）
            bool isCardVisible = ImGui::IsRectVisible(p, ImVec2(p.x + cardWidth, p.y + cardHeight));

            if (isCardVisible) {
                // --- 3Dモデルプレビュー画像の描画 (キャッシュから超高速描画) ---
                ImVec2 previewCenter(p.x + cardWidth * 0.5f, p.y + 40.0f);
                float previewRadius = 24.0f;

                if (!item.isPreviewGenerated) {
                    BuildModelPreviewTriangles(item, previewRadius, ImVec4(0.92f, 0.92f, 0.95f, 1.0f));
                }

                bool rendered = DrawCachedModelPreview(drawList, previewCenter, item);
                if (!rendered) {
                    Draw3DIsometricBoxFallback(drawList, previewCenter, 16.0f, ImVec4(0.4f, 0.6f, 0.85f, 1.0f));
                }
            }

            // --- 拡張子バッジ (右上) ---
            std::string extTag = item.extension;
            if (!extTag.empty() && extTag[0] == '.') extTag = extTag.substr(1);
            std::transform(extTag.begin(), extTag.end(), extTag.begin(), [](unsigned char c) { return (char)::toupper(c); });

            ImVec2 badgeSize = ImGui::CalcTextSize(extTag.c_str());
            ImVec2 badgeMin(p.x + cardWidth - badgeSize.x - 7.0f, p.y + 4.0f);
            ImVec2 badgeMax(p.x + cardWidth - 3.0f, p.y + 4.0f + badgeSize.y + 2.0f);
            drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(20, 50, 90, 220), 3.0f);
            drawList->AddText(ImVec2(badgeMin.x + 2.0f, badgeMin.y + 1.0f), IM_COL32(180, 215, 255, 240), extTag.c_str());

            // --- クリーンなモデル名 (フォルダ階層なし) ---
            drawList->PushClipRect(ImVec2(p.x + 2.0f, p.y + 2.0f), ImVec2(p.x + cardWidth - 2.0f, p.y + cardHeight - 2.0f), true);

            std::string dispText = GetEllipsisText(item.displayName, cardWidth - 8.0f);
            ImVec2 textSize = ImGui::CalcTextSize(dispText.c_str());
            float textX = p.x + (cardWidth - textSize.x) * 0.5f;
            float textY = p.y + cardHeight - textSize.y - 6.0f;

            ImU32 textCol = isSelected ? IM_COL32(255, 230, 120, 255)
                          : isHovered  ? IM_COL32(255, 255, 255, 255)
                                       : IM_COL32(215, 215, 225, 240);

            drawList->AddText(ImVec2(textX, textY), textCol, dispText.c_str());
            drawList->PopClipRect();

            // --- ツールチップ ---
            if (isHovered) {
                ImGui::BeginTooltip();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", item.displayName.c_str());
                ImGui::Separator();
                ImGui::Text("ファイル: %s", item.fileName.c_str());
                ImGui::Text("形式: %s", extTag.c_str());
                if (item.modelPtr) {
                    size_t vertCount = item.modelPtr->GetModelData().vertices.size();
                    ImGui::Text("頂点数: %zu", vertCount);
                }
                ImGui::TextDisabled("パス: %s", item.directoryPath.c_str());
                ImGui::EndTooltip();
            }

            ImGui::PopID();

            float lastButtonX2 = ImGui::GetItemRectMax().x;
            float nextButtonX2 = lastButtonX2 + itemSpacing + cardWidth;
            if (nextButtonX2 < windowVisibleX && fIdx + 1 < filteredIndices.size()) {
                ImGui::SameLine(0.0f, itemSpacing);
            }
        }

        if (!filteredIndices.empty()) {
            ImGui::NewLine();
        }
    }
    ImGui::End();
}
#endif

