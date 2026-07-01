import re

with open('Engine/Editor/EditorManager.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

replacement = '''                                if (mapEditMode_ == MapEditMode::Normal) {
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
                                            mapChip->SetDirty(); // Greedy Meshingの再計算
                                            pendingBlocks_.clear();
                                        }
                                        prevGridX_ = -1;
                                        prevGridY_ = -1;
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::Select) {
                                    bool isInsideSelection = false;
                                    if (selectStartX_ != -1 && selectStartY_ != -1 && selectEndX_ != -1 && selectEndY_ != -1) {
                                        int minX = (std::min)(selectStartX_, selectEndX_);
                                        int maxX = (std::max)(selectStartX_, selectEndX_);
                                        int minY = (std::min)(selectStartY_, selectEndY_);
                                        int maxY = (std::max)(selectStartY_, selectEndY_);
                                        if (gridX >= minX && gridX <= maxX && gridY >= minY && gridY <= maxY) {
                                            isInsideSelection = true;
                                        }
                                    }

                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                        if (isInsideSelection) {
                                            isDraggingSelection_ = true;
                                            dragStartGridX_ = gridX;
                                            dragStartGridY_ = gridY;
                                            
                                            // Copy the selection
                                            int minX = (std::min)(selectStartX_, selectEndX_);
                                            int maxX = (std::max)(selectStartX_, selectEndX_);
                                            int minY = (std::min)(selectStartY_, selectEndY_);
                                            int maxY = (std::max)(selectStartY_, selectEndY_);
                                            
                                            clipboardMapData_.clear();
                                            for (int y = minY; y <= maxY; ++y) {
                                                std::vector<int> row;
                                                for (int x = minX; x <= maxX; ++x) {
                                                    row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
                                                }
                                                clipboardMapData_.push_back(row);
                                            }
                                            BeginMapHistoryCapture(mapChip);
                                            // Clear original area
                                            for (int y = minY; y <= maxY; ++y) {
                                                for (int x = minX; x <= maxX; ++x) {
                                                    mapChip->SetChip(x, y, MapChip2D::ChipType::kNone);
                                                }
                                            }
                                        } else {
                                            selectStartX_ = gridX;
                                            selectStartY_ = gridY;
                                            selectEndX_ = gridX;
                                            selectEndY_ = gridY;
                                        }
                                    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                                        if (!isDraggingSelection_) {
                                            selectEndX_ = gridX;
                                            selectEndY_ = gridY;
                                        }
                                    } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                        if (isDraggingSelection_) {
                                            int deltaX = gridX - dragStartGridX_;
                                            int deltaY = gridY - dragStartGridY_;
                                            
                                            int minX = (std::min)(selectStartX_, selectEndX_);
                                            int minY = (std::min)(selectStartY_, selectEndY_);
                                            
                                            // Paste to new location
                                            for (size_t r = 0; r < clipboardMapData_.size(); ++r) {
                                                for (size_t c = 0; c < clipboardMapData_[r].size(); ++c) {
                                                    int tx = minX + deltaX + static_cast<int>(c);
                                                    int ty = minY + deltaY + static_cast<int>(r);
                                                    if (tx >= 0 && tx < mapWidth && ty >= 0 && ty < mapHeight) {
                                                        if (clipboardMapData_[r][c] != static_cast<int>(MapChip2D::ChipType::kNone)) {
                                                            mapChip->SetChip(tx, ty, static_cast<MapChip2D::ChipType>(clipboardMapData_[r][c]));
                                                        }
                                                    }
                                                }
                                            }
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                            
                                            // Update selection rect
                                            selectStartX_ += deltaX;
                                            selectEndX_ += deltaX;
                                            selectStartY_ += deltaY;
                                            selectEndY_ += deltaY;
                                            
                                            isDraggingSelection_ = false;
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::Copy) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                        if (selectStartX_ != -1 && selectStartY_ != -1 && selectEndX_ != -1 && selectEndY_ != -1) {
                                            int minX = (std::max)(0, (std::min)(selectStartX_, selectEndX_));
                                            int maxX = (std::min)(mapWidth - 1, (std::max)(selectStartX_, selectEndX_));
                                            int minY = (std::max)(0, (std::min)(selectStartY_, selectEndY_));
                                            int maxY = (std::min)(mapHeight - 1, (std::max)(selectStartY_, selectEndY_));
                                            
                                            clipboardMapData_.clear();
                                            for (int y = minY; y <= maxY; ++y) {
                                                std::vector<int> row;
                                                for (int x = minX; x <= maxX; ++x) {
                                                    row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
                                                }
                                                clipboardMapData_.push_back(row);
                                            }
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::Paste) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inBounds) {
                                        if (!clipboardMapData_.empty()) {
                                            BeginMapHistoryCapture(mapChip);
                                            for (size_t y = 0; y < clipboardMapData_.size(); ++y) {
                                                for (size_t x = 0; x < clipboardMapData_[y].size(); ++x) {
                                                    int targetX = gridX + static_cast<int>(x);
                                                    int targetY = gridY + static_cast<int>(y);
                                                    if (targetX < mapWidth && targetY < mapHeight) {
                                                        mapChip->SetChip(targetX, targetY, static_cast<MapChip2D::ChipType>(clipboardMapData_[y][x]));
                                                    }
                                                }
                                            }
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::BucketFill) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inBounds && isSelectableTool) {
                                        MapChip2D::ChipType targetType = mapChip->GetChip(gridX, gridY);
                                        MapChip2D::ChipType replacementType = static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_);
                                        if (targetType != replacementType) {
                                            BeginMapHistoryCapture(mapChip);
                                            mapChip->BucketFill(gridX, gridY, targetType, replacementType);
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                        }
                                    }
                                }'''

start_idx = cpp.find('                                if (mapEditMode_ == MapEditMode::Normal) {')
end_idx = cpp.find('                                    }', cpp.find('else if (mapEditMode_ == MapEditMode::BucketFill)')) + 38

if start_idx != -1 and end_idx != -1:
    cpp = cpp[:start_idx] + replacement + cpp[end_idx:]
    print("Replaced logic correctly.")
else:
    print("Could not find bounds.")

# Add Draw Preview
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
if old_draw in cpp:
    cpp = cpp.replace(old_draw, new_draw)
    print("Replaced draw preview.")

# Add remove mouse wheel mode change if it was lost
old_wheel = '''                                        if (io.MouseWheel != 0.0f) {
                                            int mode = static_cast<int>(mapEditMode_);
                                            if (io.MouseWheel > 0.0f) {
                                                mode = (mode - 1 + 5) % 5;
                                            } else {
                                                mode = (mode + 1) % 5;
                                            }
                                            mapEditMode_ = static_cast<MapEditMode>(mode);
                                        }'''
new_wheel = '''                                        // Removed mouse wheel mode change as requested'''
if old_wheel in cpp:
    cpp = cpp.replace(old_wheel, new_wheel)
    print("Removed mouse wheel.")

with open('Engine/Editor/EditorManager.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp)
