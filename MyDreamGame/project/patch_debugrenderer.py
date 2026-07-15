import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/SkeletonDebugRenderer.cpp'

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# インクルード追加
if '#include "DirectXCommon/DirectXCommon.h"' not in content:
    content = content.replace('#include "Resource/Primitive/PrimitiveManager.h"', '#include "Resource/Primitive/PrimitiveManager.h"\n#include "Renderer/DirectXCommon/DirectXCommon.h"\n')

# Sphere初期化の修正
sphere_old = r'''
        auto sphere = std::make_unique<PrimitiveObject>();
        sphere->Initialize(PrimitiveType::Sphere);
        sphere->SetMaterialColor(Vector4{1.0f, 0.0f, 0.0f, 1.0f}); // 赤色
'''
sphere_new = r'''
        auto sphere = std::make_unique<PrimitiveObject>();
        sphere->Initialize(DirectXCommon::GetInstance()->GetDevice().Get(), PrimitiveManager::GetInstance()->GetSphere());
        sphere->GetMaterial().color = Vector4{1.0f, 0.0f, 0.0f, 1.0f}; // 赤色
'''
content = content.replace(sphere_old.strip(), sphere_new.strip())

# Cylinder初期化の修正
cylinder_old = r'''
        auto cylinder = std::make_unique<PrimitiveObject>();
        // 線の代わりとして細いシリンダーを使う
        cylinder->Initialize(PrimitiveType::Cylinder);
        cylinder->SetMaterialColor(Vector4{0.0f, 1.0f, 0.0f, 1.0f}); // 緑色
'''
cylinder_new = r'''
        auto cylinder = std::make_unique<PrimitiveObject>();
        cylinder->Initialize(DirectXCommon::GetInstance()->GetDevice().Get(), PrimitiveManager::GetInstance()->GetCylinder());
        cylinder->GetMaterial().color = Vector4{0.0f, 1.0f, 0.0f, 1.0f}; // 緑色
'''
content = content.replace(cylinder_old.strip(), cylinder_new.strip())

# SetPosition と SetScale の修正
content = content.replace('SetPosition', 'SetTranslation')

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Fixed SkeletonDebugRenderer.cpp")
