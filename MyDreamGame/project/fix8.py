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
    print("Replaced draw block successfully!")
else:
    print("DRAW TEXT NOT FOUND!")

target_wheel = '''                                        if (io.MouseWheel != 0.0f) {
                                            int mode = static_cast<int>(mapEditMode_);
                                            if (io.MouseWheel > 0.0f) {
                                                mode = (mode - 1 + 5) % 5;
                                            } else {
                                                mode = (mode + 1) % 5;
                                            }
                                            mapEditMode_ = static_cast<MapEditMode>(mode);
                                        }'''
new_wheel = '''                                        // Removed mouse wheel mode change'''
if target_wheel in cpp:
    cpp = cpp.replace(target_wheel, new_wheel)
    print("Removed wheel logic successfully!")
else:
    print("WHEEL TEXT NOT FOUND!")

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(cpp)
