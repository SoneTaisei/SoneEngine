import os
import re

# 1. StageSelectScene.cpp の修正
stage_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Project/Scenes/StageSelectScene.cpp'
with open(stage_path, 'r', encoding='utf-8') as f:
    content = f.read()

# StageSelectScene::Draw() の中身を探す
# void StageSelectScene::Draw() {
#     ...
#     for (auto& obj : gameObjects_) {
#         obj->Draw();
#     }
#     ...
# }
# この中で描画を呼ぶ
draw_debug_code = r'''
    for (auto& obj : gameObjects_) {
        obj->Draw();
        
        auto animator = obj->GetComponent<AnimatorComponent>();
        auto transform = obj->GetComponent<TransformComponent>();
        if (animator && transform) {
            Matrix4x4 world = TransformFunctions::MakeAffineMatrix(transform->scale_, transform->rotate_, transform->translate_);
            animator->DrawDebug(world);
        }
    }
'''
if 'for (auto& obj : gameObjects_) {\n        obj->Draw();\n    }' in content:
    content = content.replace('for (auto& obj : gameObjects_) {\n        obj->Draw();\n    }', draw_debug_code.strip())
elif 'for (auto& obj : gameObjects_) {\r\n        obj->Draw();\r\n    }' in content:
    content = content.replace('for (auto& obj : gameObjects_) {\r\n        obj->Draw();\r\n    }', draw_debug_code.strip())

with open(stage_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 2. SkeletonDebugRenderer.cpp の色の修正
debug_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/SkeletonDebugRenderer.cpp'
with open(debug_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Vector4{1.0f, 0.0f, 0.0f, 1.0f} -> Vector4{0.2f, 0.0f, 0.0f, 1.0f}
content = content.replace('Vector4{1.0f, 0.0f, 0.0f, 1.0f}', 'Vector4{0.2f, 0.0f, 0.0f, 1.0f}')
# Vector4{0.0f, 1.0f, 0.0f, 1.0f} -> Vector4{0.0f, 0.2f, 0.0f, 1.0f}
content = content.replace('Vector4{0.0f, 1.0f, 0.0f, 1.0f}', 'Vector4{0.0f, 0.2f, 0.0f, 1.0f}')

with open(debug_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Patched Draw logic and Colors")
