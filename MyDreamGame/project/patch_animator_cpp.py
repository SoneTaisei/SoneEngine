import os
import re

cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# CreateSkinCluster を有効化（コメントアウトされていれば）
# StageSelectScene.cppなどで SetModelData を通してモデルを渡しているはずなので、
# SetModelData 時に CreateSkinCluster するようにする。

if 'skinCluster_ = CreateSkinCluster' not in content:
    content = content.replace(
        'skeletonDebugRenderer_.Initialize(skeleton_);',
        'skeletonDebugRenderer_.Initialize(skeleton_);\n        // Create SkinCluster if device is available (needs device, skeleton, modeldata)\n        // We will assume device can be obtained via DirectXCommon::GetInstance()->GetDevice()\n        skinCluster_ = CreateSkinCluster(DirectXCommon::GetInstance()->GetDevice(), skeleton_, modelData_);'
    )

# Update(skinCluster_, skeleton_) のコメントアウトを外す
content = content.replace('// Update(skinCluster_, skeleton_);', 'Update(skinCluster_, skeleton_);')

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Enabled SkinCluster update in AnimatorComponent.cpp")
