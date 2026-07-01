with open('Engine/Editor/EditorManager.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
skip = False
for i, line in enumerate(lines):
    if 'ImVec2 dragDelta =' in line:
        pass
    elif 'bool wasDragging =' in line:
        pass
    elif 'if (wasDragging) {' in line:
        pass
    elif '// hbOĂH' in line:
        pass
    else:
        new_lines.append(line)

with open('Engine/Editor/EditorManager.cpp', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)
