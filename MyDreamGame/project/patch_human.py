import os
import re

stage_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Project/Scenes/StageSelectScene.cpp'
with open(stage_path, 'r', encoding='utf-8') as f:
    content = f.read()

# AnimatedCube.gltf -> walk.gltf (human)
old_code = r'''
    // AnimatedCubeの追加
    Model* animatedCubeModel = ModelManager::GetInstance()->GetModel("resources/Object/School/AnimatedCube", "AnimatedCube.gltf");
    Animation cubeAnimation = LoadAnimationFile("resources/Object/School/AnimatedCube", "AnimatedCube.gltf");
    
    auto animatedCubeObject = std::make_shared<GameObject>("AnimatedCube");
    auto cubeTransform = animatedCubeObject->AddComponent<TransformComponent>();
    cubeTransform->SetPosition({0.0f, 0.0f, 0.0f}); // 中央に配置
    
    uint32_t cubeTexIndex = TextureManager::GetInstance()->Load("resources/Object/School/AnimatedCube/AnimatedCube_BaseColor.png");
'''
new_code = r'''
    // human walkの追加
    Model* animatedCubeModel = ModelManager::GetInstance()->GetModel("resources/Object/School/human", "walk.gltf");
    Animation cubeAnimation = LoadAnimationFile("resources/Object/School/human", "walk.gltf");
    
    auto animatedCubeObject = std::make_shared<GameObject>("human_walk");
    auto cubeTransform = animatedCubeObject->AddComponent<TransformComponent>();
    cubeTransform->SetPosition({0.0f, 0.0f, 0.0f}); // 中央に配置
    
    uint32_t cubeTexIndex = TextureManager::GetInstance()->Load("resources/Object/School/human/white.png");
'''

if 'AnimatedCube.gltf' in content:
    # re.subが改行などで上手くいかないことがあるため、単純なreplaceを試す。
    content = content.replace('"resources/Object/School/AnimatedCube", "AnimatedCube.gltf"', '"resources/Object/School/human", "walk.gltf"')
    content = content.replace('std::make_shared<GameObject>("AnimatedCube")', 'std::make_shared<GameObject>("human_walk")')
    content = content.replace('TextureManager::GetInstance()->Load("resources/Object/School/AnimatedCube/AnimatedCube_BaseColor.png")', 'TextureManager::GetInstance()->Load("resources/Object/School/human/white.png")')

with open(stage_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated StageSelectScene.cpp to use human/walk.gltf")
