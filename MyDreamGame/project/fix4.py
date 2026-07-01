import sys

cpp_path = 'Engine/Editor/EditorManager.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    cpp = f.read()

# 1. Update Normal Mode to use pendingBlocks_
old_normal = '''                                if (mapEditMode_ == MapEditMode::Normal) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isSelectableTool) {
                                        if (inBounds) {
                                            BeginMapHistoryCapture(mapChip);
                                            if (mapChip->GetChip(gridX, gridY) != static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_)) {
                                                mapChip->SetChip(gridX, gridY, static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_));
                                            }
                                        }
                                        prevGridX_ = gridX;
                                        prevGridY_ = gridY;
                                    }
                                    else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && isSelectableTool) {
                                        if (prevGridX_ != -1 && prevGridY_ != -1 && (prevGridX_ != gridX || prevGridY_ != gridY)) {
                                            int x0 = prevGridX_;
                                            int y0 = prevGridY_;
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
                                                    if (mapChip->GetChip(x0, y0) != static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_)) {
                                                        mapChip->SetChip(x0, y0, static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_));
                                                    }
                                                }
                                                if (x0 == x1 && y0 == y1) break;
                                                e2 = err;
                                                if (e2 > -dx) { err -= dy; x0 += sx; }
                                                if (e2 < dy) { err += dx; y0 += sy; }
                                            }
                                        }
                                        prevGridX_ = gridX;
                                        prevGridY_ = gridY;
                                    }
                                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                        EndMapHistoryCapture(mapChip);
                                        prevGridX_ = -1;
                                        prevGridY_ = -1;
                                    }
                                }'''

new_normal = '''                                if (mapEditMode_ == MapEditMode::Normal) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isSelectableTool) {
                                        if (inBounds) {
                                            if (std::find(pendingBlocks_.begin(), pendingBlocks_.end(), std::make_pair(gridX, gridY)) == pendingBlocks_.end()) {
                                                pendingBlocks_.push_back({gridX, gridY});
                                            }
                                        }
                                        prevGridX_ = gridX;
                                        prevGridY_ = gridY;
                                    }
                                    else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && isSelectableTool) {
                                        if (prevGridX_ != -1 && prevGridY_ != -1 && (prevGridX_ != gridX || prevGridY_ != gridY)) {
                                            int x0 = prevGridX_;
                                            int y0 = prevGridY_;
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
                                                    if (std::find(pendingBlocks_.begin(), pendingBlocks_.end(), std::make_pair(x0, y0)) == pendingBlocks_.end()) {
                                                        pendingBlocks_.push_back({x0, y0});
                                                    }
                                                }
                                                if (x0 == x1 && y0 == y1) break;
                                                e2 = err;
                                                if (e2 > -dx) { err -= dy; x0 += sx; }
                                                if (e2 < dy) { err += dx; y0 += sy; }
                                            }
                                        }
                                        prevGridX_ = gridX;
                                        prevGridY_ = gridY;
                                    }
                                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                        if (!pendingBlocks_.empty()) {
                                            BeginMapHistoryCapture(mapChip);
                                            for (const auto& pos : pendingBlocks_) {
                                                if (mapChip->GetChip(pos.first, pos.second) != static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_)) {
                                                    mapChip->SetChip(pos.first, pos.second, static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_));
                                                }
                                            }
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                            pendingBlocks_.clear();
                                        }
                                        prevGridX_ = -1;
                                        prevGridY_ = -1;
                                    }
                                }'''

cpp = cpp.replace(old_normal, new_normal)

# 2. Add SetDirty for other modes (Select/Move, Paste, BucketFill)
old_select_release = '''                                            EndMapHistoryCapture(mapChip);
                                            isDraggingSelection_ = false;
                                        }
                                    }
                                }'''
new_select_release = '''                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                            isDraggingSelection_ = false;
                                        }
                                    }
                                }'''
cpp = cpp.replace(old_select_release, new_select_release)

old_paste = '''                                            EndMapHistoryCapture(mapChip);
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::BucketFill) {'''
new_paste = '''                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::BucketFill) {'''
cpp = cpp.replace(old_paste, new_paste)

old_bucket = '''                                            BeginMapHistoryCapture(mapChip);
                                            mapChip->BucketFill(gridX, gridY, targetType, replacementType);
                                            EndMapHistoryCapture(mapChip);
                                        }'''
new_bucket = '''                                            BeginMapHistoryCapture(mapChip);
                                            mapChip->BucketFill(gridX, gridY, targetType, replacementType);
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                        }'''
cpp = cpp.replace(old_bucket, new_bucket)

# 3. Add Preview Draw Logic
old_draw = '''                                            drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                                        }

                                        drawList->PopClipRect();'''

new_draw = '''                                            drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                                        }

                                        // プレビュー描画
                                        if (mapEditMode_ == MapEditMode::Normal && !pendingBlocks_.empty()) {
                                            for (const auto& pos : pendingBlocks_) {
                                                ImVec2 pTL = WorldToScreen(static_cast<float>(pos.first), static_cast<float>(pos.second + 1));
                                                ImVec2 pBR = WorldToScreen(static_cast<float>(pos.first + 1), static_cast<float>(pos.second));
                                                drawList->AddRectFilled(pTL, pBR, IM_COL32(255, 100, 100, 150));
                                                drawList->AddRect(pTL, pBR, IM_COL32(255, 100, 100, 255), 0.0f, 0, 2.0f);
                                            }
                                        }

                                        drawList->PopClipRect();'''

cpp = cpp.replace(old_draw, new_draw)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(cpp)
print('Done!')
