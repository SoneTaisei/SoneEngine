import os

# 1. StageSelectScene.cppの修正
stage_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Project/Scenes/StageSelectScene.cpp'
with open(stage_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Skydome(モンスターボール)関連のコードをコメントアウト
monster_ball_code = r'''
    // 1. マネージャーから素材を借りる（頂点バッファを重複させない）
    Model *skydomeModelResource = ModelManager::GetInstance()->GetModel("resources/Object/Original/sphere", "sphere.gltf");
    uint32_t skydomeIndex = TextureManager::GetInstance()->Load("resources/Sprite/School/monsterBall.png");
    D3D12_GPU_DESCRIPTOR_HANDLE skydomeTH = TextureManager::GetInstance()->GetGpuHandle(skydomeIndex);

    // 2. GameObjectを作る
    auto skydomeObject = std::make_shared<GameObject>("Skydome");
    auto transform = skydomeObject->AddComponent<TransformComponent>();
    transform->SetRotation({0.0f, 0.0f, 0.0f});

    // 3. 描画コンポーネントのアタッチとテクスチャの設定
    auto skydomeRenderer = skydomeObject->AddComponent<MeshRendererComponent>();
    skydomeRenderer->Initialize(device.Get(), skydomeModelResource);
    skydomeRenderer->SetTextureHandle(skydomeTH);
    skydomeModelResource->SetTextureHandle(skydomeTH);

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    gameObjects_.push_back(skydomeObject);
'''
# 正規表現だと面倒なので、特徴的な行を順番に置換していく
content = content.replace('Model *skydomeModelResource', '// Model *skydomeModelResource')
content = content.replace('uint32_t skydomeIndex', '// uint32_t skydomeIndex')
content = content.replace('D3D12_GPU_DESCRIPTOR_HANDLE skydomeTH = TextureManager::GetInstance()->GetGpuHandle(skydomeIndex);', '// D3D12_GPU_DESCRIPTOR_HANDLE skydomeTH...')
content = content.replace('auto skydomeObject = std::make_shared<GameObject>("Skydome");', '// auto skydomeObject = ...')
content = content.replace('auto transform = skydomeObject->AddComponent<TransformComponent>();', '// auto transform = ...')
content = content.replace('transform->SetRotation({0.0f, 0.0f, 0.0f});', '// transform->SetRotation({0.0f, 0.0f, 0.0f});')
content = content.replace('auto skydomeRenderer = skydomeObject->AddComponent<MeshRendererComponent>();', '// auto skydomeRenderer = ...')
content = content.replace('skydomeRenderer->Initialize(device.Get(), skydomeModelResource);', '// skydomeRenderer->Initialize...')
content = content.replace('skydomeRenderer->SetTextureHandle(skydomeTH);', '// skydomeRenderer->SetTextureHandle...')
content = content.replace('skydomeModelResource->SetTextureHandle(skydomeTH);', '// skydomeModelResource->SetTextureHandle...')
content = content.replace('gameObjects_.push_back(skydomeObject);', '// gameObjects_.push_back(skydomeObject);')

with open(stage_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 2. SkeletonDebugRenderer.cppの修正（Sphere描画の復活）
cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/SkeletonDebugRenderer.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Sphereの生成コメントを外す
content = content.replace('/*\n    while (jointSpheres_.size() < jointCount)', '    while (jointSpheres_.size() < jointCount)')
content = content.replace('jointSpheres_.push_back(std::move(sphere));\n    }\n    */', 'jointSpheres_.push_back(std::move(sphere));\n    }')

# Sphereの描画コメントを外す
content = content.replace('/*\n        // Sphereの位置・スケール更新', '        // Sphereの位置・スケール更新')
content = content.replace('jointSpheres_[i]->Draw();\n        */\n        Vector3 pos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };', 'jointSpheres_[i]->Draw();')

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Removed monster ball and restored skeleton spheres.")
