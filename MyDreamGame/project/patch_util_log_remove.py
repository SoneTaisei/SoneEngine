import os

util_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'
with open(util_path, 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('LogManager::GetInstance()->Log("Skeleton created with " + std::to_string(skeleton.joints.size()) + " joints.\\n");', '')

with open(util_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Removed LogManager call.")
