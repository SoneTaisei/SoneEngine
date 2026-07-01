import re

cpp_path = 'Engine/Editor/EditorManager.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    cpp = f.read()

pattern_draw = re.compile(r'(drawList->AddRect\(pTL,\s*pBR,\s*IM_COL32\(0,\s*255,\s*255,\s*255\),\s*0\.0f,\s*0,\s*2\.0f\);\s*\n\s*\}\s*\n\n\s*)(drawList->PopClipRect\(\);)', re.DOTALL)
match = pattern_draw.search(cpp)

if match:
    new_draw = match.group(1) + '''// プレビュー描画
                        if (mapEditMode_ == MapEditMode::Normal && !pendingBlocks_.empty()) {
                            for (const auto& pos : pendingBlocks_) {
                                ImVec2 pTL = WorldToScreen(static_cast<float>(pos.first), static_cast<float>(pos.second + 1));
                                ImVec2 pBR = WorldToScreen(static_cast<float>(pos.first + 1), static_cast<float>(pos.second));
                                drawList->AddRectFilled(pTL, pBR, IM_COL32(255, 100, 100, 150));
                                drawList->AddRect(pTL, pBR, IM_COL32(255, 100, 100, 255), 0.0f, 0, 2.0f);
                            }
                        }\n\n                        ''' + match.group(2)
    cpp = cpp[:match.start()] + new_draw + cpp[match.end():]
    print("Replaced draw preview with regex successfully!")
else:
    print("DRAW TEXT NOT FOUND WITH REGEX!")

pattern_wheel = re.compile(r'(\s*if\s*\(io\.MouseWheel\s*!=\s*0\.0f\)\s*\{\s*int\s*mode\s*=\s*static_cast<int>\(mapEditMode_\);\s*if\s*\(io\.MouseWheel\s*>\s*0\.0f\)\s*\{\s*mode\s*=\s*\(mode\s*-\s*1\s*\+\s*5\)\s*%\s*5;\s*\}\s*else\s*\{\s*mode\s*=\s*\(mode\s*\+\s*1\)\s*%\s*5;\s*\}\s*mapEditMode_\s*=\s*static_cast<MapEditMode>\(mode\);\s*\})', re.DOTALL)

match_wheel = pattern_wheel.search(cpp)
if match_wheel:
    cpp = cpp[:match_wheel.start()] + '''                                        // Removed mouse wheel mode change''' + cpp[match_wheel.end():]
    print("Removed mouse wheel logic with regex successfully!")
else:
    print("WHEEL TEXT NOT FOUND WITH REGEX!")

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(cpp)
