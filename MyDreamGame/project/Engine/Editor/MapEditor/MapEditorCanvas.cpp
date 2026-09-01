#ifdef USE_IMGUI
#include "MapEditorCanvas.h"
#include "MapEditorContext.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Scene/SceneFactory.h"
#include "Graphics/Camera.h"
#include "Core/Utility/TransformFunctions.h"
#include <algorithm>
#include <cmath>

MapEditorCanvas::MapEditorCanvas(MapEditorContext* context)
    : context_(context) {
}

void MapEditorCanvas::Draw(
    SceneManager* sceneManager,
    Camera** activeCamera,
    D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle,
    bool& isMapEditorVisible,
    bool& isMapEditorHovered,
    const std::function<void()>& onTabActive
) {
    if (!context_) return;

    if (ImGui::Begin("マップチップ画面", &isMapEditorVisible)) {
        bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        if (isFocused && onTabActive) {
            onTabActive();
        }

        IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
        if (!activeScene || !activeScene->GetMapChip()) {
            if (sceneManager) {
                // シーン切り替え等のフォールバック
            }
        }

        ImGui::Text("編集モード:");
        ImGui::SameLine();
        int modeInt = static_cast<int>(context_->GetEditMode());
        ImGui::RadioButton("通常塗", &modeInt, 0); ImGui::SameLine();
        ImGui::RadioButton("範囲選択", &modeInt, 1); ImGui::SameLine();
        ImGui::RadioButton("コピー", &modeInt, 2); ImGui::SameLine();
        ImGui::RadioButton("貼り付け", &modeInt, 3); ImGui::SameLine();
        ImGui::RadioButton("バケツ塗", &modeInt, 4);
        context_->SetEditMode(static_cast<MapEditorContext::MapEditMode>(modeInt));

        if (activeScene) {
            MapChip2D* mapChip = activeScene->GetMapChip();
            if (mapChip) {
                ImVec2 contentSize = ImGui::GetContentRegionAvail();
                if (contentSize.x < 100.0f) contentSize.x = 100.0f;
                if (contentSize.y < 100.0f) contentSize.y = 100.0f;

                float aspect = 1280.0f / 720.0f;
                float windowAspect = contentSize.x / contentSize.y;
                ImVec2 imageSize;
                if (windowAspect > aspect) {
                    imageSize.y = contentSize.y;
                    imageSize.x = contentSize.y * aspect;
                } else {
                    imageSize.x = contentSize.x;
                    imageSize.y = contentSize.x / aspect;
                }

                // 中央寄せ
                ImVec2 currentPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));

                ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

                // GameViewと同じテクスチャを表示
                ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

                // グリッド・ルーム・選択・オーバーレイ描画
                Camera* camera = activeCamera ? *activeCamera : nullptr;
                if (camera) {
                    Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());

                    auto WorldToScreen = [&](float wx, float wy) -> ImVec2 {
                        Vector3 ndc = TransformFunctions::EulerTransform({ wx, wy, 0.0f }, viewProj);
                        float screenX = imageScreenPos.x + (ndc.x + 1.0f) * 0.5f * imageSize.x;
                        float screenY = imageScreenPos.y + (1.0f - ndc.y) * 0.5f * imageSize.y;
                        return ImVec2(screenX, screenY);
                    };

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    int mapWidth = mapChip->GetWidth();
                    int mapHeight = mapChip->GetHeight();

                    drawList->PushClipRect(imageScreenPos, ImVec2(imageScreenPos.x + imageSize.x, imageScreenPos.y + imageSize.y), true);

                    // 縦線
                    for (int x = 0; x <= mapWidth; ++x) {
                        ImVec2 p1 = WorldToScreen(static_cast<float>(x), 0.0f);
                        ImVec2 p2 = WorldToScreen(static_cast<float>(x), static_cast<float>(mapHeight));
                        drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, 80), 1.0f);
                    }

                    // 横線
                    for (int y = 0; y <= mapHeight; ++y) {
                        ImVec2 p1 = WorldToScreen(0.0f, static_cast<float>(y));
                        ImVec2 p2 = WorldToScreen(static_cast<float>(mapWidth), static_cast<float>(y));
                        drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, 80), 1.0f);
                    }

                    // ルームの描画
                    const auto& rooms = mapChip->GetRooms();
                    for (size_t i = 0; i < rooms.size(); ++i) {
                        const auto& r = rooms[i];
                        ImVec2 pTL = WorldToScreen(r.x, r.y + r.height);
                        ImVec2 pBR = WorldToScreen(r.x + r.width, r.y);
                        bool isCurrentRoomDragging = (context_->IsRoomEditMode() && context_->GetDraggingRoomIndex() == static_cast<int>(i));
                        ImU32 color = isCurrentRoomDragging ? IM_COL32(255, 255, 0, 100) : IM_COL32(50, 50, 255, 50);
                        ImU32 borderColor = isCurrentRoomDragging ? IM_COL32(255, 255, 0, 255) : IM_COL32(50, 50, 255, 255);
                        drawList->AddRectFilled(pTL, pBR, color);
                        drawList->AddRect(pTL, pBR, borderColor, 0.0f, 0, 2.0f);
                    }

                    // 範囲選択の描画
                    if (context_->HasSelection()) {
                        int minX = (std::min)(context_->GetSelectStartX(), context_->GetSelectEndX());
                        int maxX = (std::max)(context_->GetSelectStartX(), context_->GetSelectEndX());
                        int minY = (std::min)(context_->GetSelectStartY(), context_->GetSelectEndY());
                        int maxY = (std::max)(context_->GetSelectStartY(), context_->GetSelectEndY());
                        ImVec2 pTL = WorldToScreen(static_cast<float>(minX), static_cast<float>(maxY + 1));
                        ImVec2 pBR = WorldToScreen(static_cast<float>(maxX + 1), static_cast<float>(minY));
                        drawList->AddRectFilled(pTL, pBR, IM_COL32(0, 255, 255, 80));
                        drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                    }

                    // プレビュー描画 (Normalモード)
                    if (context_->GetEditMode() == MapEditorContext::MapEditMode::Normal && !context_->GetPendingBlocks().empty()) {
                        for (const auto& pos : context_->GetPendingBlocks()) {
                            ImVec2 pTL = WorldToScreen(static_cast<float>(pos.first), static_cast<float>(pos.second + 1));
                            ImVec2 pBR = WorldToScreen(static_cast<float>(pos.first + 1), static_cast<float>(pos.second));
                            drawList->AddRectFilled(pTL, pBR, IM_COL32(255, 100, 100, 150));
                            drawList->AddRect(pTL, pBR, IM_COL32(255, 100, 100, 255), 0.0f, 0, 2.0f);
                        }
                    }

                    // スポーン地点とルームリスポーン地点のオーバーレイ描画
                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            MapChip2D::ChipType type = mapChip->GetChip(x, y);
                            if (type == MapChip2D::ChipType::kPlayerSpawn || type == MapChip2D::ChipType::kRoomRespawn) {
                                ImVec2 pTL = WorldToScreen(static_cast<float>(x), static_cast<float>(y + 1));
                                ImVec2 pBR = WorldToScreen(static_cast<float>(x + 1), static_cast<float>(y));
                                if (type == MapChip2D::ChipType::kPlayerSpawn) {
                                    drawList->AddRectFilled(pTL, pBR, IM_COL32(51, 153, 255, 180));
                                    drawList->AddRect(pTL, pBR, IM_COL32(51, 153, 255, 255), 0.0f, 0, 2.0f);
                                } else {
                                    drawList->AddRectFilled(pTL, pBR, IM_COL32(51, 204, 255, 180));
                                    drawList->AddRect(pTL, pBR, IM_COL32(51, 204, 255, 255), 0.0f, 0, 2.0f);
                                }
                            }
                        }
                    }

                    drawList->PopClipRect();
                }

                // クリック判定用の見えないボタン
                ImGui::SetCursorScreenPos(imageScreenPos);
                ImGui::InvisibleButton("MapCanvasImage", imageSize);
                isMapEditorHovered = ImGui::IsItemHovered();

                if (isMapEditorHovered) {
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    float localX = mousePos.x - imageScreenPos.x;
                    float localY = mousePos.y - imageScreenPos.y;

                    float u = localX / imageSize.x;
                    float v = localY / imageSize.y;

                    float ndcX = u * 2.0f - 1.0f;
                    float ndcY = 1.0f - v * 2.0f;

                    Vector3 worldPos = { 0, 0, 0 };
                    if (camera) {
                        Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                        Matrix4x4 invViewProj = TransformFunctions::Inverse(viewProj);
                        worldPos = TransformFunctions::EulerTransform({ ndcX, ndcY, 0.0f }, invViewProj);
                    }

                    // モード切り替えショートカット (Ctrl + Wheel)
                    if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
                        int m = static_cast<int>(context_->GetEditMode());
                        if (ImGui::GetIO().MouseWheel < 0.0f) m = (m + 1) % 5;
                        else m = (m + 4) % 5;
                        context_->SetEditMode(static_cast<MapEditorContext::MapEditMode>(m));
                    }

                    // Undo / Redo
                    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                        context_->Undo();
                    }
                    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                        context_->Redo();
                    }

                    if (context_->IsRoomEditMode()) {
                        float hitDist = 0.5f;
                        auto& rooms = mapChip->GetRooms();

                        // ドラッグ開始判定
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            context_->BeginRoomHistoryCapture(mapChip);
                            context_->SetDraggingRoomIndex(-1);
                            context_->SetRoomDragHandle(0);

                            for (int i = static_cast<int>(rooms.size()) - 1; i >= 0; --i) {
                                const auto& r = rooms[i];
                                bool inX = (worldPos.x >= r.x && worldPos.x <= r.x + r.width);
                                bool inY = (worldPos.y >= r.y && worldPos.y <= r.y + r.height);

                                bool onLeft = std::abs(worldPos.x - r.x) < hitDist;
                                bool onRight = std::abs(worldPos.x - (r.x + r.width)) < hitDist;
                                bool onBottom = std::abs(worldPos.y - r.y) < hitDist;
                                bool onTop = std::abs(worldPos.y - (r.y + r.height)) < hitDist;

                                if ((inX && inY) || ((onLeft || onRight) && inY) || ((onTop || onBottom) && inX)) {
                                    context_->SetDraggingRoomIndex(i);
                                    if (onLeft && onTop) context_->SetRoomDragHandle(2);
                                    else if (onRight && onTop) context_->SetRoomDragHandle(3);
                                    else if (onLeft && onBottom) context_->SetRoomDragHandle(4);
                                    else if (onRight && onBottom) context_->SetRoomDragHandle(5);
                                    else if (onLeft) context_->SetRoomDragHandle(6);
                                    else if (onRight) context_->SetRoomDragHandle(7);
                                    else if (onTop) context_->SetRoomDragHandle(8);
                                    else if (onBottom) context_->SetRoomDragHandle(9);
                                    else {
                                        context_->SetRoomDragHandle(1); // Move
                                        context_->SetRoomDragOffset(worldPos.x - r.x, worldPos.y - r.y);
                                    }
                                    break;
                                }
                            }

                            if (context_->GetDraggingRoomIndex() != -1 && ImGui::GetIO().KeyCtrl) {
                                rooms.erase(rooms.begin() + context_->GetDraggingRoomIndex());
                                context_->SetDraggingRoomIndex(-1);
                                context_->SetRoomDragHandle(0);
                            } else if (context_->GetDraggingRoomIndex() == -1 && !ImGui::GetIO().KeyCtrl) {
                                bool snap = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                                StageRoom newRoom;
                                newRoom.x = snap ? std::floor(worldPos.x) : worldPos.x;
                                newRoom.y = snap ? std::floor(worldPos.y) : worldPos.y;
                                newRoom.width = 1.0f;
                                newRoom.height = 1.0f;
                                rooms.push_back(newRoom);
                                context_->SetDraggingRoomIndex(static_cast<int>(rooms.size()) - 1);
                                context_->SetRoomDragHandle(5); // BottomRight drag
                            }
                        }

                        if ((ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)) && context_->GetDraggingRoomIndex() != -1) {
                            auto& r = rooms[context_->GetDraggingRoomIndex()];
                            bool snap = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
                            float snapX_left = snap ? std::floor(worldPos.x) : worldPos.x;
                            float snapY_bottom = snap ? std::floor(worldPos.y) : worldPos.y;
                            float snapX_right = snap ? std::floor(worldPos.x) + 1.0f : worldPos.x;
                            float snapY_top = snap ? std::floor(worldPos.y) + 1.0f : worldPos.y;
                            float minSize = snap ? 1.0f : 0.1f;

                            int handle = context_->GetRoomDragHandle();
                            if (handle == 1) { // Move
                                float targetX = snap ? std::floor(worldPos.x) : worldPos.x;
                                float targetY = snap ? std::floor(worldPos.y) : worldPos.y;
                                r.x = targetX - context_->GetRoomDragOffsetX();
                                r.y = targetY - context_->GetRoomDragOffsetY();
                                if (snap) {
                                    r.x = std::round(r.x);
                                    r.y = std::round(r.y);
                                }
                            } else {
                                if (handle == 2 || handle == 6 || handle == 4) {
                                    float right = r.x + r.width;
                                    r.x = std::fmin(snapX_left, right - minSize);
                                    r.width = right - r.x;
                                }
                                if (handle == 3 || handle == 7 || handle == 5) {
                                    r.width = std::fmax(minSize, snapX_right - r.x);
                                }
                                if (handle == 4 || handle == 9 || handle == 5) {
                                    float top = r.y + r.height;
                                    r.y = std::fmin(snapY_bottom, top - minSize);
                                    r.height = top - r.y;
                                }
                                if (handle == 2 || handle == 8 || handle == 3) {
                                    r.height = std::fmax(minSize, snapY_top - r.y);
                                }
                            }
                        }

                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                            context_->EndRoomHistoryCapture(mapChip);
                            context_->SetDraggingRoomIndex(-1);
                            context_->SetRoomDragHandle(0);
                        }
                    } else {
                        if (camera) {
                            int mapWidth = mapChip->GetWidth();
                            int mapHeight = mapChip->GetHeight();
                            int gridX = mapChip->WorldToChipX(worldPos.x);
                            int gridY = mapChip->WorldToChipY(worldPos.y);

                            bool inBounds = (gridX >= 0 && gridX < mapWidth && gridY >= 0 && gridY < mapHeight);
                            int selectedTool = context_->GetSelectedTool();
                            bool isSelectableTool = (selectedTool >= 0);

                            auto editMode = context_->GetEditMode();
                            if (editMode == MapEditorContext::MapEditMode::Normal) {
                                auto& pending = context_->GetPendingBlocks();
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isSelectableTool) {
                                    if (inBounds) {
                                        if (std::find(pending.begin(), pending.end(), std::make_pair(gridX, gridY)) == pending.end()) {
                                            pending.push_back({ gridX, gridY });
                                        }
                                    }
                                    context_->SetPrevGrid(gridX, gridY);
                                } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && isSelectableTool) {
                                    int prevX = context_->GetPrevGridX();
                                    int prevY = context_->GetPrevGridY();
                                    if (prevX != -1 && prevY != -1 && (prevX != gridX || prevY != gridY)) {
                                        int x0 = prevX;
                                        int y0 = prevY;
                                        int x1 = gridX;
                                        int y1 = gridY;
                                        int dx = std::abs(x1 - x0);
                                        int dy = std::abs(y1 - y0);
                                        int sx = x0 < x1 ? 1 : -1;
                                        int sy = y0 < y1 ? 1 : -1;
                                        int err = (dx > dy ? dx : -dy) / 2;
                                        int e2;

                                        while (true) {
                                            if (x0 >= 0 && x0 < mapWidth && y0 >= 0 && y0 < mapHeight) {
                                                if (std::find(pending.begin(), pending.end(), std::make_pair(x0, y0)) == pending.end()) {
                                                    pending.push_back({ x0, y0 });
                                                }
                                            }
                                            if (x0 == x1 && y0 == y1) break;
                                            e2 = err;
                                            if (e2 > -dx) { err -= dy; x0 += sx; }
                                            if (e2 < dy) { err += dx; y0 += sy; }
                                        }
                                    }
                                    context_->SetPrevGrid(gridX, gridY);
                                }
                                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                    if (!pending.empty()) {
                                        context_->BeginMapHistoryCapture(mapChip);
                                        for (const auto& pos : pending) {
                                            if (mapChip->GetChip(pos.first, pos.second) != static_cast<MapChip2D::ChipType>(selectedTool)) {
                                                mapChip->SetChip(pos.first, pos.second, static_cast<MapChip2D::ChipType>(selectedTool));
                                            }
                                        }
                                        context_->EndMapHistoryCapture(mapChip);
                                        mapChip->SetDirty();
                                        pending.clear();
                                    }
                                    context_->SetPrevGrid(-1, -1);
                                }
                            } else if (editMode == MapEditorContext::MapEditMode::Select) {
                                bool isInsideSelection = false;
                                if (context_->HasSelection()) {
                                    int minX = (std::min)(context_->GetSelectStartX(), context_->GetSelectEndX());
                                    int maxX = (std::max)(context_->GetSelectStartX(), context_->GetSelectEndX());
                                    int minY = (std::min)(context_->GetSelectStartY(), context_->GetSelectEndY());
                                    int maxY = (std::max)(context_->GetSelectStartY(), context_->GetSelectEndY());
                                    if (gridX >= minX && gridX <= maxX && gridY >= minY && gridY <= maxY) {
                                        isInsideSelection = true;
                                    }
                                }

                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                    if (isInsideSelection) {
                                        context_->SetDraggingSelection(true);
                                        context_->SetDragStartGrid(gridX, gridY);

                                        int minX = (std::min)(context_->GetSelectStartX(), context_->GetSelectEndX());
                                        int maxX = (std::max)(context_->GetSelectStartX(), context_->GetSelectEndX());
                                        int minY = (std::min)(context_->GetSelectStartY(), context_->GetSelectEndY());
                                        int maxY = (std::max)(context_->GetSelectStartY(), context_->GetSelectEndY());

                                        auto& clip = context_->GetClipboardData();
                                        clip.clear();
                                        for (int y = minY; y <= maxY; ++y) {
                                            std::vector<int> row;
                                            for (int x = minX; x <= maxX; ++x) {
                                                row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
                                            }
                                            clip.push_back(row);
                                        }
                                        context_->BeginMapHistoryCapture(mapChip);
                                        for (int y = minY; y <= maxY; ++y) {
                                            for (int x = minX; x <= maxX; ++x) {
                                                mapChip->SetChip(x, y, MapChip2D::ChipType::kNone);
                                            }
                                        }
                                    } else {
                                        context_->SetSelectRect(gridX, gridY, gridX, gridY);
                                    }
                                } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                                    if (!context_->IsDraggingSelection()) {
                                        context_->SetSelectRect(context_->GetSelectStartX(), context_->GetSelectStartY(), gridX, gridY);
                                    }
                                } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                    if (context_->IsDraggingSelection()) {
                                        int deltaX = gridX - context_->GetDragStartGridX();
                                        int deltaY = gridY - context_->GetDragStartGridY();

                                        int minX = (std::min)(context_->GetSelectStartX(), context_->GetSelectEndX());
                                        int minY = (std::min)(context_->GetSelectStartY(), context_->GetSelectEndY());

                                        const auto& clip = context_->GetClipboardData();
                                        for (size_t r = 0; r < clip.size(); ++r) {
                                            for (size_t c = 0; c < clip[r].size(); ++c) {
                                                int tx = minX + deltaX + static_cast<int>(c);
                                                int ty = minY + deltaY + static_cast<int>(r);
                                                if (tx >= 0 && tx < mapWidth && ty >= 0 && ty < mapHeight) {
                                                    if (clip[r][c] != static_cast<int>(MapChip2D::ChipType::kNone)) {
                                                        mapChip->SetChip(tx, ty, static_cast<MapChip2D::ChipType>(clip[r][c]));
                                                    }
                                                }
                                            }
                                        }
                                        context_->EndMapHistoryCapture(mapChip);
                                        mapChip->SetDirty();

                                        context_->SetSelectRect(
                                            context_->GetSelectStartX() + deltaX,
                                            context_->GetSelectStartY() + deltaY,
                                            context_->GetSelectEndX() + deltaX,
                                            context_->GetSelectEndY() + deltaY
                                        );

                                        context_->SetDraggingSelection(false);
                                    }
                                }
                            } else if (editMode == MapEditorContext::MapEditMode::Copy) {
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                    if (context_->HasSelection()) {
                                        int minX = (std::max)(0, (std::min)(context_->GetSelectStartX(), context_->GetSelectEndX()));
                                        int maxX = (std::min)(mapWidth - 1, (std::max)(context_->GetSelectStartX(), context_->GetSelectEndX()));
                                        int minY = (std::max)(0, (std::min)(context_->GetSelectStartY(), context_->GetSelectEndY()));
                                        int maxY = (std::min)(mapHeight - 1, (std::max)(context_->GetSelectStartY(), context_->GetSelectEndY()));

                                        auto& clip = context_->GetClipboardData();
                                        clip.clear();
                                        for (int y = minY; y <= maxY; ++y) {
                                            std::vector<int> row;
                                            for (int x = minX; x <= maxX; ++x) {
                                                row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
                                            }
                                            clip.push_back(row);
                                        }
                                    }
                                }
                            } else if (editMode == MapEditorContext::MapEditMode::Paste) {
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inBounds) {
                                    const auto& clip = context_->GetClipboardData();
                                    if (!clip.empty()) {
                                        context_->BeginMapHistoryCapture(mapChip);
                                        for (int y = 0; y < static_cast<int>(clip.size()); ++y) {
                                            for (int x = 0; x < static_cast<int>(clip[y].size()); ++x) {
                                                int targetX = gridX + x;
                                                int targetY = gridY + y;
                                                if (targetX >= 0 && targetX < mapWidth && targetY >= 0 && targetY < mapHeight) {
                                                    mapChip->SetChip(targetX, targetY, static_cast<MapChip2D::ChipType>(clip[y][x]));
                                                }
                                            }
                                        }
                                        context_->EndMapHistoryCapture(mapChip);
                                        mapChip->SetDirty();
                                    }
                                }
                            } else if (editMode == MapEditorContext::MapEditMode::BucketFill) {
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inBounds && isSelectableTool) {
                                    MapChip2D::ChipType targetType = mapChip->GetChip(gridX, gridY);
                                    MapChip2D::ChipType replacementType = static_cast<MapChip2D::ChipType>(selectedTool);
                                    if (targetType != replacementType) {
                                        context_->BeginMapHistoryCapture(mapChip);
                                        mapChip->BucketFill(gridX, gridY, targetType, replacementType);
                                        context_->EndMapHistoryCapture(mapChip);
                                        mapChip->SetDirty();
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                ImGui::Text("Active scene does not support 2D map editing.");
            }
        } else {
            ImGui::Text("No active scene.");
        }
    }
    ImGui::End();
}
#endif
