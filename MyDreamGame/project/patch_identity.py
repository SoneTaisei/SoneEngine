import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('Matrix4x4::Identity()', 'TransformFunctions::MakeIdentity4x4()')

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Replaced Matrix4x4::Identity() with TransformFunctions::MakeIdentity4x4()")
