#ifdef USE_IMGUI
#include "MapEditorPalette.h"
#include "MapEditorContext.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Game2D/MapChip2D.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Model/Model.h"
#include "Graphics/TextureManager.h"
#include "BlockClassGenerator.h"
#include "Game2D/Blocks/BlockFactory.h"
#include <vector>
#include <string>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace {
    struct ToolIcon {
        int id = 0;
        std::string name;
        std::string type;
        ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float scale = 1.0f;
        std::string modelName;
        std::string textureName;
        Model* modelPtr = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE textureGpuHandle = {};
        bool hasTexture = false;
    };

    std::tuple<Model*, D3D12_GPU_DESCRIPTOR_HANDLE, bool> ResolveToolResource(const std::string& texName, const std::string& mdlName) {
        Model* model = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE texGpuHandle = {};
        bool hasTexture = false;

        if (!mdlName.empty()) {
            if (mdlName.length() >= 4 && mdlName.substr(mdlName.length() - 4) == ".obj") {
                std::string fullPath = (mdlName.find("resources/") == 0) ? mdlName : ("resources/" + mdlName);
                std::filesystem::path p(fullPath);
                std::string dirPath = p.parent_path().string();
                std::string fileName = p.filename().string();
                std::replace(dirPath.begin(), dirPath.end(), '\\', '/');
                model = ModelManager::GetInstance()->GetModel(dirPath, fileName);
            } else {
                model = ModelManager::GetInstance()->GetModel("resources/Object/School/" + mdlName, mdlName + ".obj");
                if (!model) {
                    model = ModelManager::GetInstance()->GetModel("resources/models", mdlName + ".obj");
                }
            }
        }

        if (!texName.empty()) {
            std::string fullTex = (texName.find("resources/") == 0) ? texName : ("resources/" + texName);
            uint32_t handle = TextureManager::GetInstance()->Load(fullTex);
            texGpuHandle = TextureManager::GetInstance()->GetGpuHandle(handle);
            hasTexture = true;
        } else if (model) {
            std::string texPath = model->GetModelData().material.textureFilePath;
            if (!texPath.empty() && std::filesystem::exists(texPath)) {
                uint32_t texIdx = TextureManager::GetInstance()->Load(texPath);
                texGpuHandle = TextureManager::GetInstance()->GetGpuHandle(texIdx);
                hasTexture = true;
            }
        }

        return { model, texGpuHandle, hasTexture };
    }

    bool Draw3DModelPreview(ImDrawList* drawList, ImVec2 center, float boxSize, Model* model, ImVec4 baseColor, D3D12_GPU_DESCRIPTOR_HANDLE texGpuHandle, bool hasTexture) {
        if (!model) return false;
        const auto& modelData = model->GetModelData();
        if (modelData.vertices.empty()) return false;

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
        float scale = (boxSize * 0.85f) / maxDim;

        // 2. 回転（斜めアングル: Yaw = 35度, Pitch = 22度）
        float yaw = 35.0f * 3.14159265f / 180.0f;
        float pitch = 22.0f * 3.14159265f / 180.0f;
        float cosY = std::cos(yaw), sinY = std::sin(yaw);
        float cosP = std::cos(pitch), sinP = std::sin(pitch);

        auto TransformVertex = [&](const Vector4& pos) -> Vector3 {
            float x = (pos.x - bCenter.x) * scale;
            float y = (pos.y - bCenter.y) * scale;
            float z = (pos.z - bCenter.z) * scale;

            // Yaw回転
            float x1 = x * cosY + z * sinY;
            float y1 = y;
            float z1 = -x * sinY + z * cosY;

            // Pitch回転
            float x2 = x1;
            float y2 = y1 * cosP - z1 * sinP;
            float z2 = y1 * sinP + z1 * cosP;

            return { x2, y2, z2 };
        };

        struct Tri {
            ImVec2 p[3];
            ImVec2 uv[3];
            float avgZ;
            ImU32 col;
        };
        std::vector<Tri> triangles;
        size_t numIndices = modelData.indices.size();
        size_t numTriangles = numIndices > 0 ? numIndices / 3 : modelData.vertices.size() / 3;

        Vector3 lightDir = { 0.4f, 0.8f, 0.5f };
        float lightLen = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
        lightDir.x /= lightLen; lightDir.y /= lightLen; lightDir.z /= lightLen;

        triangles.reserve(numTriangles);

        for (size_t t = 0; t < numTriangles; ++t) {
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

            ImVec2 s0(center.x + t0.x, center.y - t0.y);
            ImVec2 s1(center.x + t1.x, center.y - t1.y);
            ImVec2 s2(center.x + t2.x, center.y - t2.y);

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

            Tri tri;
            tri.p[0] = s0; tri.p[1] = s1; tri.p[2] = s2;
            tri.uv[0] = ImVec2(v0.texcoord.x, v0.texcoord.y);
            tri.uv[1] = ImVec2(v1.texcoord.x, v1.texcoord.y);
            tri.uv[2] = ImVec2(v2.texcoord.x, v2.texcoord.y);
            tri.avgZ = (t0.z + t1.z + t2.z) / 3.0f;
            tri.col = col;
            triangles.push_back(tri);
        }

        std::sort(triangles.begin(), triangles.end(), [](const Tri& a, const Tri& b) {
            return a.avgZ < b.avgZ;
        });

        if (hasTexture && texGpuHandle.ptr != 0) {
            drawList->PushTextureID((ImTextureID)texGpuHandle.ptr);
            for (const auto& tri : triangles) {
                drawList->PrimReserve(3, 3);
                ImDrawIdx vidx = drawList->_VtxCurrentIdx;
                drawList->PrimWriteIdx(vidx);
                drawList->PrimWriteIdx(static_cast<ImDrawIdx>(vidx + 1));
                drawList->PrimWriteIdx(static_cast<ImDrawIdx>(vidx + 2));
                drawList->PrimWriteVtx(tri.p[0], tri.uv[0], tri.col);
                drawList->PrimWriteVtx(tri.p[1], tri.uv[1], tri.col);
                drawList->PrimWriteVtx(tri.p[2], tri.uv[2], tri.col);
            }
            drawList->PopTextureID();
        } else {
            for (const auto& tri : triangles) {
                drawList->AddTriangleFilled(tri.p[0], tri.p[1], tri.p[2], tri.col);
            }
        }
        return true;
    }

    void Draw3DIsometricBox(ImDrawList* drawList, ImVec2 center, float size, ImVec4 baseColor, float heightRatio, bool isWireframe) {
        float rx = size * 0.866f;
        float ry = size * 0.5f;
        float h = size * 1.15f * heightRatio;

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

        if (isWireframe) {
            ImU32 colWire = AdjustColor(baseColor, 1.4f);
            drawList->AddLine(pTop, pRight, colWire, 1.5f);
            drawList->AddLine(pRight, pBottom, colWire, 1.5f);
            drawList->AddLine(pBottom, pLeft, colWire, 1.5f);
            drawList->AddLine(pLeft, pTop, colWire, 1.5f);

            drawList->AddLine(pLeft, pBotLeft, colWire, 1.5f);
            drawList->AddLine(pBottom, pBotCenter, colWire, 1.5f);
            drawList->AddLine(pRight, pBotRight, colWire, 1.5f);

            drawList->AddLine(pBotLeft, pBotCenter, colWire, 1.5f);
            drawList->AddLine(pBotCenter, pBotRight, colWire, 1.5f);
        } else {
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

MapEditorPalette::MapEditorPalette(MapEditorContext* context)
    : context_(context) {
}

void MapEditorPalette::Draw(SceneManager* sceneManager, const std::function<void()>& onSelectionCleared) {
    if (!context_) return;

    IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
    if (!activeScene) return;

    MapChip2D* mapChip = activeScene->GetMapChip();
    if (!mapChip) return;

    ImGui::BeginDisabled(context_->IsRoomEditMode());
    ImGui::Text("Paint Tool:");
    ImGui::Spacing();

    std::vector<ToolIcon> systemTools = {
        { 6, "Spawn", "PlayerSpawn", ImVec4(0.2f, 0.6f, 1.0f, 1.0f), 1.0f, "", "", nullptr, {}, false },
        { 10, "RoomSpawn", "RoomRespawn", ImVec4(0.2f, 0.8f, 1.0f, 1.0f), 1.0f, "", "", nullptr, {}, false },
        { 0, "Erase", "Erase", ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f, "", "", nullptr, {}, false }
    };

    std::vector<ToolIcon> templateTools;
    for (const auto& def : mapChip->GetTemplatePalette()) {
        auto [mdl, gpuH, hasTex] = ResolveToolResource(def.textureName, def.modelName);
        templateTools.push_back({ def.id, def.name, def.type, ImVec4(def.color.x, def.color.y, def.color.z, def.color.w), 1.0f, def.modelName, def.textureName, mdl, gpuH, hasTex });
    }

    std::set<std::string> availableTypes;
    for (const auto& def : mapChip->GetCustomPalette()) {
        availableTypes.insert(def.type);
    }

    std::vector<ToolIcon> customTools;
    const auto& filters = context_->GetCustomToolFilters();
    for (const auto& def : mapChip->GetCustomPalette()) {
        if (filters.find(def.type) != filters.end()) {
            continue;
        }
        auto [mdl, gpuH, hasTex] = ResolveToolResource(def.textureName, def.modelName);
        customTools.push_back({ def.id, def.name, def.type, ImVec4(def.color.x, def.color.y, def.color.z, def.color.w), 1.0f, def.modelName, def.textureName, mdl, gpuH, hasTex });
    }

    float cardWidth = 84.0f;
    float cardHeight = 98.0f;
    float itemSpacing = ImGui::GetStyle().ItemSpacing.x;

    auto DrawTools = [&](const std::vector<ToolIcon>& tools, int sectionType) {
        float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        int numTools = static_cast<int>(tools.size());
        int maxIter = (sectionType == 2 || sectionType == 1) ? numTools + 1 : numTools;

        for (int i = 0; i < maxIter; i++) {
            ImGui::PushID(sectionType * 1000 + i);

            ImVec2 p = ImGui::GetCursorScreenPos();
            float lastButtonX2 = 0.0f;

            if (i < numTools) {
                const ToolIcon& tool = tools[i];
                bool isSelected = (context_->GetSelectedTool() == tool.id);

                ImGui::SetNextItemAllowOverlap();
                if (ImGui::InvisibleButton("##Tool", ImVec2(cardWidth, cardHeight))) {
                    context_->SetSelectedTool(tool.id);
                    if (onSelectionCleared) {
                        onSelectionCleared();
                    }
                }

                lastButtonX2 = ImGui::GetItemRectMax().x;

                bool isHovered = ImGui::IsItemHovered();
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                ImU32 bgCol = isHovered ? IM_COL32(55, 62, 75, 255) : (isSelected && sectionType != 1 ? IM_COL32(40, 52, 70, 255) : IM_COL32(36, 39, 46, 255));
                drawList->AddRectFilled(p, ImVec2(p.x + cardWidth, p.y + cardHeight), bgCol, 6.0f);

                ImU32 borderCol = isSelected && sectionType != 1 ? IM_COL32(255, 205, 50, 255) : (isHovered ? IM_COL32(100, 130, 170, 255) : IM_COL32(50, 55, 65, 255));
                float borderWidth = isSelected && sectionType != 1 ? 2.0f : 1.0f;
                drawList->AddRect(p, ImVec2(p.x + cardWidth, p.y + cardHeight), borderCol, 6.0f, 0, borderWidth);

                ImVec2 previewCenter(p.x + cardWidth * 0.5f, p.y + 34.0f);
                float previewRadius = 20.0f * tool.scale;

                bool modelRendered = false;
                if (tool.modelPtr) {
                    modelRendered = Draw3DModelPreview(drawList, previewCenter, 46.0f * tool.scale, tool.modelPtr, tool.color, tool.textureGpuHandle, tool.hasTexture);
                }

                if (!modelRendered) {
                    if (tool.id == 0) {
                        Draw3DIsometricBox(drawList, previewCenter, previewRadius, tool.color, 1.0f, true);
                    } else if (tool.type == "OneWayBlock") {
                        Draw3DIsometricBox(drawList, previewCenter, previewRadius, tool.color, 0.35f, false);
                        ImVec2 arrowTop(previewCenter.x, previewCenter.y - 12.0f);
                        ImVec2 arrowLeft(previewCenter.x - 7.0f, previewCenter.y - 3.0f);
                        ImVec2 arrowRight(previewCenter.x + 7.0f, previewCenter.y - 3.0f);
                        drawList->AddTriangleFilled(arrowTop, arrowLeft, arrowRight, IM_COL32(255, 255, 255, 230));
                    } else if (tool.type == "DeathBlock") {
                        Draw3DIsometricBox(drawList, previewCenter, previewRadius, tool.color, 1.0f, false);
                        drawList->AddLine(ImVec2(previewCenter.x - 6, previewCenter.y - 6), ImVec2(previewCenter.x + 6, previewCenter.y + 6), IM_COL32(255, 255, 255, 230), 2.0f);
                        drawList->AddLine(ImVec2(previewCenter.x + 6, previewCenter.y - 6), ImVec2(previewCenter.x - 6, previewCenter.y + 6), IM_COL32(255, 255, 255, 230), 2.0f);
                    } else if (tool.type == "GoalBlock") {
                        Draw3DIsometricBox(drawList, previewCenter, previewRadius, tool.color, 1.0f, false);
                        drawList->AddLine(ImVec2(previewCenter.x - 4, previewCenter.y + 8), ImVec2(previewCenter.x - 4, previewCenter.y - 8), IM_COL32(255, 255, 255, 240), 1.5f);
                        drawList->AddTriangleFilled(ImVec2(previewCenter.x - 3, previewCenter.y - 8), ImVec2(previewCenter.x + 6, previewCenter.y - 4), ImVec2(previewCenter.x - 3, previewCenter.y), IM_COL32(255, 220, 50, 240));
                    } else if (tool.id == 6 || tool.id == 10) {
                        Draw3DIsometricBox(drawList, previewCenter, previewRadius, tool.color, 0.35f, false);
                        drawList->AddCircleFilled(ImVec2(previewCenter.x, previewCenter.y - 6.0f), 5.0f, IM_COL32(255, 255, 255, 240));
                        drawList->AddCircle(ImVec2(previewCenter.x, previewCenter.y - 6.0f), 8.0f, IM_COL32(255, 255, 255, 160), 12, 1.5f);
                    } else {
                        Draw3DIsometricBox(drawList, previewCenter, previewRadius, tool.color, 1.0f, false);
                    }
                }

                if (!tool.modelName.empty()) {
                    ImVec2 badgeMin(p.x + 5.0f, p.y + 5.0f);
                    ImVec2 badgeMax(p.x + 27.0f, p.y + 18.0f);
                    drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(30, 100, 200, 230), 3.0f);
                    drawList->AddRect(badgeMin, badgeMax, IM_COL32(100, 180, 255, 200), 3.0f);
                    drawList->AddText(ImVec2(badgeMin.x + 3.0f, badgeMin.y + 1.0f), IM_COL32(255, 255, 255, 255), "3D");
                }

                drawList->PushClipRect(ImVec2(p.x + 2.0f, p.y + 2.0f), ImVec2(p.x + cardWidth - 2.0f, p.y + cardHeight - 2.0f), true);

                std::string dispText = GetEllipsisText(tool.name, cardWidth - 8.0f);
                ImVec2 textSize = ImGui::CalcTextSize(dispText.c_str());
                float textX = p.x + (cardWidth - textSize.x) * 0.5f;
                float textY = p.y + cardHeight - textSize.y - 6.0f;
                drawList->AddText(ImVec2(textX, textY), isSelected && sectionType != 1 ? IM_COL32(255, 230, 120, 255) : IM_COL32(220, 220, 220, 255), dispText.c_str());

                drawList->PopClipRect();

                if (isHovered) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", tool.name.c_str());
                    ImGui::Separator();
                    ImGui::Text("ID: %d", tool.id);
                    if (!tool.type.empty()) {
                        ImGui::Text("Type: %s", tool.type.c_str());
                    }
                    if (!tool.modelName.empty()) {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Model: %s", tool.modelName.c_str());
                    } else {
                        std::string primName = (tool.type == "OneWayBlock") ? "Thin Box (Primitive)" : (tool.id == 0 ? "None (Erase)" : "Box (Primitive)");
                        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Shape: %s", primName.c_str());
                    }
                    if (!tool.textureName.empty()) {
                        ImGui::Text("Texture: %s", tool.textureName.c_str());
                    }
                    ImGui::Spacing();
                    ImGui::ColorButton("##ColPreview", tool.color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker, ImVec2(50, 16));
                    ImGui::EndTooltip();
                }

                if (sectionType == 2) {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("DND_CUSTOM_TOOL", &tool.id, sizeof(int));
                        ImGui::Text("Move %s", tool.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_CUSTOM_TOOL")) {
                            IM_ASSERT(payload->DataSize == sizeof(int));
                            int payload_id = *(const int*)payload->Data;
                            int target_id = tool.id;

                            auto& palette = mapChip->GetCustomPalette();
                            int srcIdx = -1, dstIdx = -1;
                            for (size_t k = 0; k < palette.size(); ++k) {
                                if (palette[k].id == payload_id) srcIdx = static_cast<int>(k);
                                if (palette[k].id == target_id) dstIdx = static_cast<int>(k);
                            }
                            if (srcIdx != -1 && dstIdx != -1 && srcIdx != dstIdx) {
                                auto item = palette[srcIdx];
                                palette.erase(palette.begin() + srcIdx);
                                if (srcIdx < dstIdx) dstIdx--;
                                palette.insert(palette.begin() + dstIdx, item);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImVec2 delMin(p.x + cardWidth - 18.0f, p.y + 4.0f);
                    ImVec2 delMax(p.x + cardWidth - 4.0f, p.y + 18.0f);
                    ImVec2 mousePos = ImGui::GetMousePos();
                    bool isDelHovered = (mousePos.x >= delMin.x && mousePos.x <= delMax.x && mousePos.y >= delMin.y && mousePos.y <= delMax.y);

                    ImU32 delBgCol = isDelHovered ? IM_COL32(235, 60, 60, 255) : IM_COL32(180, 50, 50, 200);
                    drawList->AddRectFilled(delMin, delMax, delBgCol, 3.0f);

                    float xPad = 3.5f;
                    drawList->AddLine(ImVec2(delMin.x + xPad, delMin.y + xPad), ImVec2(delMax.x - xPad, delMax.y - xPad), IM_COL32(255, 255, 255, 255), 1.5f);
                    drawList->AddLine(ImVec2(delMax.x - xPad, delMin.y + xPad), ImVec2(delMin.x + xPad, delMax.y - xPad), IM_COL32(255, 255, 255, 255), 1.5f);

                    if (isDelHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        toolToDelete_ = tool.id;
                        openDeletePopup_ = true;
                    }
                } else if (sectionType == 1) {
                    // Basic Tools (テンプレート) の削除ボタン
                    ImVec2 delMin(p.x + cardWidth - 18.0f, p.y + 4.0f);
                    ImVec2 delMax(p.x + cardWidth - 4.0f, p.y + 18.0f);
                    ImVec2 mousePos = ImGui::GetMousePos();
                    bool isDelHovered = (mousePos.x >= delMin.x && mousePos.x <= delMax.x && mousePos.y >= delMin.y && mousePos.y <= delMax.y);

                    ImU32 delBgCol = isDelHovered ? IM_COL32(235, 60, 60, 255) : IM_COL32(180, 50, 50, 200);
                    drawList->AddRectFilled(delMin, delMax, delBgCol, 3.0f);

                    float xPad = 3.5f;
                    drawList->AddLine(ImVec2(delMin.x + xPad, delMin.y + xPad), ImVec2(delMax.x - xPad, delMax.y - xPad), IM_COL32(255, 255, 255, 255), 1.5f);
                    drawList->AddLine(ImVec2(delMax.x - xPad, delMin.y + xPad), ImVec2(delMin.x + xPad, delMax.y - xPad), IM_COL32(255, 255, 255, 255), 1.5f);

                    if (isDelHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        templateToDelete_ = tool.id;
                        openDeleteTemplatePopup_ = true;
                    }
                }
            } else if (sectionType == 2) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.32f, 0.42f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.20f, 0.26f, 1.0f));
                if (ImGui::Button("＋\n追加", ImVec2(cardWidth, cardHeight))) {
                    auto& palette = mapChip->GetCustomPalette();
                    MapChip2D::CustomBlockDef newDef;
                    newDef.id = 100 + static_cast<int>(palette.size());
                    newDef.name = "Custom " + std::to_string(palette.size() + 1);
                    newDef.type = "NormalBlock";
                    palette.push_back(newDef);
                    context_->SetSelectedTool(newDef.id);
                    if (onSelectionCleared) {
                        onSelectionCleared();
                    }
                    mapChip->SaveToFile(context_->GetFullFilePath(context_->GetStageFilename()));
                }
                ImGui::PopStyleColor(3);
                lastButtonX2 = ImGui::GetItemRectMax().x;
            } else if (sectionType == 1) {
                // Basic Tools 用の「＋ 新規クラス作成」ボタン
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.38f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.38f, 0.52f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.24f, 0.32f, 1.0f));
                if (ImGui::Button("＋\nクラス\n作成", ImVec2(cardWidth, cardHeight))) {
                    openCreateBlockPopup_ = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("新しいブロックのC++クラス（.h / .cpp）を自動生成し、\nVisual Studioプロジェクトに登録します。");
                }
                ImGui::PopStyleColor(3);
                lastButtonX2 = ImGui::GetItemRectMax().x;
            }

            ImGui::PopID();

            float nextButtonX2 = lastButtonX2 + itemSpacing + cardWidth;
            if (nextButtonX2 < windowVisibleX && i + 1 < maxIter) {
                ImGui::SameLine(0.0f, itemSpacing);
            }
        }
        if (maxIter > 0) {
            ImGui::NewLine();
        }
    };

    if (ImGui::CollapsingHeader("System Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawTools(systemTools, 0);
    }
    if (ImGui::CollapsingHeader("Basic Tools (Settings)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("※これらのブロックはマップ設定用のテンプレートです。（直接設置はできません）");
        
        if (ImGui::Button("テンプレート再読み込み (Reload)")) {
            mapChip->LoadTemplatesFromFile("resources/json/shared/templates_config.json");
            mapChip->RebuildChipObjects();
        }
        ImGui::SameLine();
        if (ImGui::Button("＋ 新規ブロッククラス作成...")) {
            openCreateBlockPopup_ = true;
        }
        ImGui::Spacing();

        DrawTools(templateTools, 1);

        // テンプレート削除確認ポップアップ
        if (openDeleteTemplatePopup_) {
            ImGui::OpenPopup("DeleteTemplateConfirmPopup");
            openDeleteTemplatePopup_ = false;
        }
        if (ImGui::BeginPopupModal("DeleteTemplateConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            const MapChip2D::CustomBlockDef* targetDef = nullptr;
            for (const auto& t : mapChip->GetTemplatePalette()) {
                if (t.id == templateToDelete_) {
                    targetDef = &t;
                    break;
                }
            }

            std::string toolName = targetDef ? targetDef->name : "選択されたツール";
            std::string className = targetDef ? targetDef->type : "";

            ImGui::Text("本当にこのベーシックツール「%s（クラス: %s）」を削除しますか？", toolName.c_str(), className.c_str());
            ImGui::Spacing();
            ImGui::Checkbox("C++クラスファイル (.h / .cpp) とVSプロジェクト登録も完全に削除する", &deleteClassSourceFiles_);
            if (deleteClassSourceFiles_) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "※Project/Game2D/Blocks/%s.h および .cpp がディスクから削除されます。", className.c_str());
            }

            ImGui::Separator();
            if (ImGui::Button("はい", ImVec2(120, 0))) {
                auto& templates = mapChip->GetTemplatePalette();
                std::string typeToDelete = "";
                for (const auto& d : templates) {
                    if (d.id == templateToDelete_) {
                        typeToDelete = d.type;
                        break;
                    }
                }

                auto it = std::remove_if(templates.begin(), templates.end(), [&](const MapChip2D::CustomBlockDef& d) { return d.id == templateToDelete_; });
                templates.erase(it, templates.end());
                mapChip->SaveTemplatesToFile("resources/json/shared/templates_config.json");
                
                if (context_->GetSelectedTool() == templateToDelete_) {
                    context_->SetSelectedTool(0);
                }
                templateToDelete_ = -1;

                if (deleteClassSourceFiles_ && !typeToDelete.empty()) {
                    std::string delMsg;
                    BlockClassGenerator::DeleteBlockClass(typeToDelete, delMsg);
                    createResultStatus_ = delMsg;
                    showCreateResultPopup_ = true;
                }

                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("いいえ", ImVec2(120, 0))) {
                templateToDelete_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 新規クラス作成ポップアップ
        if (openCreateBlockPopup_) {
            ImGui::OpenPopup("CreateBlockClassModal");
            openCreateBlockPopup_ = false;
        }

        if (ImGui::BeginPopupModal("CreateBlockClassModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "新規ブロッククラスの作成");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::InputText("クラス名 (Class Name)", newClassNameBuf_, sizeof(newClassNameBuf_));
            ImGui::TextDisabled("※半角英数字（例: JumpBlock, IceBlock, SpringBlock）");

            ImGui::InputText("表示名 (Display Name)", newDisplayNameBuf_, sizeof(newDisplayNameBuf_));

            const char* behaviorTypes[] = {
                "通常ブロック (Solid / 地面・壁)",
                "一方向すり抜け足場 (One-Way Platform)",
                "トゲ・ダメージ (Hazard / Death)",
                "ゴール (Goal)",
                "トリガー・すり抜け (Trigger / Non-solid)"
            };
            ImGui::Combo("基本挙動タイプ", &newBlockBehaviorType_, behaviorTypes, IM_ARRAYSIZE(behaviorTypes));

            ImGui::ColorEdit4("初期カラー", newBlockColor_);

            ImGui::Separator();
            ImGui::TextDisabled("生成先:");
            ImGui::TextDisabled("  - Project/Game2D/Blocks/%s.h", newClassNameBuf_);
            ImGui::TextDisabled("  - Project/Game2D/Blocks/%s.cpp", newClassNameBuf_);
            ImGui::TextDisabled("  - MyDreamGame.vcxproj (自動登録)");

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("クラス生成 (Create)", ImVec2(140, 0))) {
                BlockClassGenParams params;
                params.className = newClassNameBuf_;
                params.displayName = newDisplayNameBuf_;
                params.isSolid = (newBlockBehaviorType_ != 4 && newBlockBehaviorType_ != 1);
                params.isOneWay = (newBlockBehaviorType_ == 1);
                params.color = { newBlockColor_[0], newBlockColor_[1], newBlockColor_[2], newBlockColor_[3] };

                auto genRes = BlockClassGenerator::GenerateBlockClass(params);
                if (genRes.success) {
                    // テンプレートパレットにも登録
                    auto& templates = mapChip->GetTemplatePalette();
                    int maxId = 0;
                    for (const auto& t : templates) {
                        if (t.id > maxId) maxId = t.id;
                    }
                    MapChip2D::CustomBlockDef newDef;
                    newDef.id = (std::max)(maxId + 1, 7);
                    newDef.name = params.displayName.empty() ? params.className : params.displayName;
                    newDef.type = params.className;
                    newDef.color = params.color;
                    newDef.scale = { 1.0f, 1.0f, 1.0f };
                    templates.push_back(newDef);

                    mapChip->SaveTemplatesToFile("resources/json/shared/templates_config.json");

                    createResultStatus_ = genRes.message;
                    showCreateResultPopup_ = true;
                    ImGui::CloseCurrentPopup();
                } else {
                    createResultStatus_ = "エラー: " + genRes.message;
                    showCreateResultPopup_ = true;
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル (Cancel)", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (showCreateResultPopup_) {
            ImGui::OpenPopup("CreateResultPopup");
            showCreateResultPopup_ = false;
        }

        if (ImGui::BeginPopupModal("CreateResultPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", createResultStatus_.c_str());
            ImGui::Separator();
            if (ImGui::Button("閉じる (OK)", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if (ImGui::CollapsingHeader("Custom Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("フィルター設定...")) {
            ImGui::OpenPopup("FilterPopup");
        }

        if (ImGui::BeginPopup("FilterPopup")) {
            ImGui::Text("表示するブロックの種類:");
            ImGui::Separator();
            auto& customFilters = context_->GetCustomToolFilters();
            for (const auto& type : availableTypes) {
                bool isChecked = (customFilters.find(type) == customFilters.end());
                if (ImGui::Checkbox(type.c_str(), &isChecked)) {
                    if (isChecked) {
                        customFilters.erase(type);
                    } else {
                        customFilters.insert(type);
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::Button("閉じる", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::Spacing();

        DrawTools(customTools, 2);

        if (openDeletePopup_) {
            ImGui::OpenPopup("DeleteConfirmPopup");
            openDeletePopup_ = false;
        }
        if (ImGui::BeginPopupModal("DeleteConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("本当に削除しますか？\n(マップ上で使用されている場合はエラーになる可能性があります)");
            ImGui::Separator();
            if (ImGui::Button("はい", ImVec2(120, 0))) {
                auto& palette = mapChip->GetCustomPalette();
                auto it = std::remove_if(palette.begin(), palette.end(), [&](const MapChip2D::CustomBlockDef& d) { return d.id == toolToDelete_; });
                palette.erase(it, palette.end());
                mapChip->SaveToFile(context_->GetFullFilePath(context_->GetStageFilename()));
                toolToDelete_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("いいえ", ImVec2(120, 0))) {
                toolToDelete_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Spacing();
    ImGui::EndDisabled();
}
#endif
