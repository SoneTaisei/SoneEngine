import os
import re

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/SkeletonDebugRenderer.cpp'

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# GetDevice().Get() -> GetDevice()
content = content.replace('GetDevice().Get()', 'GetDevice()')

# GetSphere() -> GetPrimitive(PrimitiveType::Sphere)
content = content.replace('GetSphere()', 'GetPrimitive(PrimitiveType::Sphere)')

# GetCylinder() -> GetPrimitive(PrimitiveType::Cylinder)
content = content.replace('GetCylinder()', 'GetPrimitive(PrimitiveType::Cylinder)')

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Patched SkeletonDebugRenderer.cpp")
