import os

stage_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Project/Scenes/StageSelectScene.cpp'
with open(stage_path, 'r', encoding='utf-8') as f:
    content = f.read()

# スケールを追加
content = content.replace('cubeTransform->SetPosition({0.0f, 0.0f, 0.0f}); // 中央に配置', 'cubeTransform->SetPosition({0.0f, 0.0f, 0.0f});\n    cubeTransform->SetScale({1.0f, 1.0f, 1.0f}); // 一旦1.0倍で確認、必要なら後で調整')

with open(stage_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Adjusted scale in StageSelectScene.")
