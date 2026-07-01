import re

with open('Engine/Editor/EditorManager.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

# 2. Add Preview Draw Logic
old_draw = '''    drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
    }

    drawList->PopClipRect();'''

new_draw = '''    drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
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
    print("Replaced draw logic directly.")
else:
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
        print("Replaced draw logic via regex.")
    else:
        print("Could not find draw logic.")

with open('Engine/Editor/EditorManager.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp)
