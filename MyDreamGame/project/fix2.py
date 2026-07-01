import re

with open('Engine/Editor/EditorManager.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

# 1. Update Input Logic
pattern_input = re.compile(r'(if\s*\(mapEditMode_\s*==\s*MapEditMode::Normal\)\s*\{.*?prevGridY_\s*=\s*-1;\s*\})', re.DOTALL)
match = pattern_input.search(cpp)
if match:
    old_input = match.group(1)
    new_input = '''if (mapEditMode_ == MapEditMode::Normal) {
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
                                }'''
    cpp = cpp.replace(old_input, new_input)
    print("Replaced input logic.")
else:
    print("Could not find input logic to replace.")

# 2. Add Preview Draw Logic
pattern_draw = re.compile(r'(drawList->AddRect\(pTL,\s*pBR,\s*IM_COL32\(0,\s*255,\s*255,\s*255\),\s*0\.0f,\s*0,\s*2\.0f\);\s*\n\s*\}\s*\n\n\s*drawList->PopClipRect\(\);)', re.DOTALL)
match = pattern_draw.search(cpp)
if match:
    old_draw = match.group(1)
    new_draw = '''drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
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
    print("Replaced draw logic.")
else:
    print("Could not find draw logic to replace.")

with open('Engine/Editor/EditorManager.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp)
