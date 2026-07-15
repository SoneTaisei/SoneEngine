import os

util_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'
with open(util_path, 'r', encoding='utf-8') as f:
    content = f.read()

if '#include "Core/Utility/LogManager.h"' not in content:
    content = '#include "Core/Utility/LogManager.h"\n' + content

with open(util_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Added LogManager include.")
