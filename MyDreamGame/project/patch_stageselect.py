import os
import re

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Project/Scenes/StageSelectScene.cpp'

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# SetModelDataの追加
# auto cubeAnimator = animatedCubeObject->AddComponent<AnimatorComponent>();
# のあとに cubeAnimator->SetModelData(*animatedCubeModel->GetModelData()); を追加する
# GetModelData()というアクセサがあるか不明だが、modelData_へのアクセスがあればそれを使う。
# 通常Modelには GetModelData() が用意されている。
set_model_data = r'''
    auto cubeAnimator = animatedCubeObject->AddComponent<AnimatorComponent>();
    cubeAnimator->Initialize();
    cubeAnimator->SetModelData(animatedCubeModel->GetModelData()); // Skeletonの生成
'''
content = re.sub(r'auto cubeAnimator = animatedCubeObject->AddComponent<AnimatorComponent>\(\);\s*cubeAnimator->Initialize\(\);', set_model_data.strip(), content)

# DrawDebugの追加
# Draw(const ViewProjection& viewProjection) 内の最後に呼び出す
draw_debug = r'''
    for (auto& obj : gameObjects_) {
        auto animator = obj->GetComponent<AnimatorComponent>();
        if (animator) {
            animator->DrawDebug(viewProjection);
        }
    }
}
'''
content = re.sub(r'\}\s*void StageSelectScene::Finalize\(\)', draw_debug.strip() + '\n\nvoid StageSelectScene::Finalize()', content)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated StageSelectScene.cpp")
