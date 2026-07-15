import os

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/SkeletonDebugRenderer.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Sphereの生成をコメントアウト
sphere_init = r'''    while (jointSpheres_.size() < jointCount) {
        auto sphere = std::make_unique<PrimitiveObject>();
        sphere->Initialize(DirectXCommon::GetInstance()->GetDevice(), PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Sphere));
        sphere->GetMaterial().color = Vector4{0.2f, 0.0f, 0.0f, 1.0f}; // 赤色
        jointSpheres_.push_back(std::move(sphere));
    }'''
content = content.replace(sphere_init, '/*\n' + sphere_init + '\n    */')

# Sphereの描画をコメントアウト
sphere_draw = r'''        // Sphereの位置・スケール更新
        Vector3 pos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        jointSpheres_[i]->SetScale(Vector3{0.005f, 0.005f, 0.005f});
        jointSpheres_[i]->SetTranslation(pos);
        // jointWorldからクォータニオン等の回転を抽出するのは省略し、位置のみ描画する
        jointSpheres_[i]->Update();
        jointSpheres_[i]->Draw();'''
content = content.replace(sphere_draw, '/*\n' + sphere_draw + '\n        */\n        Vector3 pos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };')

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Disabled Sphere rendering in SkeletonDebugRenderer.")
