import sys

cpp_path = 'Engine/Editor/EditorManager.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    cpp = f.read()

target_draw = '''                            drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                        }
                        
                        drawList->PopClipRect();'''

new_draw = '''                            drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
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

if target_draw in cpp:
    cpp = cpp.replace(target_draw, new_draw)
    print("Replaced draw preview block!")
else:
    print("DRAW PREVIEW BLOCK NOT FOUND")

target_wheel = '''                        } else {
                            // モード切り替えショートカット
                            if ((ImGui::GetIO().KeyCtrl || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) && ImGui::GetIO().MouseWheel != 0.0f) {
                                int m = static_cast<int>(mapEditMode_);
                                if (ImGui::GetIO().MouseWheel < 0.0f) m = (m + 1) % 5;
                                else m = (m + 4) % 5;
                                mapEditMode_ = static_cast<MapEditMode>(m);
                            }'''

new_wheel = '''                        } else {
                            // モード切り替えショートカット
                            if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
                                int m = static_cast<int>(mapEditMode_);
                                if (ImGui::GetIO().MouseWheel < 0.0f) m = (m + 1) % 5;
                                else m = (m + 4) % 5;
                                mapEditMode_ = static_cast<MapEditMode>(m);
                            }'''

if target_wheel in cpp:
    cpp = cpp.replace(target_wheel, new_wheel)
    print("Replaced wheel block!")
else:
    print("WHEEL BLOCK NOT FOUND")

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(cpp)
