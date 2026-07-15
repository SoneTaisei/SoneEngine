import os
import re

# 1. SkeletonDebugRenderer.cppのスケール調整
renderer_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Renderer/SkeletonDebugRenderer.cpp'
with open(renderer_path, 'r', encoding='utf-8') as f:
    content = f.read()

# jointSpheres_[i]->SetScale(Vector3{0.05f, 0.05f, 0.05f}); -> 0.005f にする
content = content.replace('Vector3{0.05f, 0.05f, 0.05f}', 'Vector3{0.005f, 0.005f, 0.005f}')

with open(renderer_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 2. UtilityFunctions.cppでジョイント数をLogManagerで出力
util_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Core/Utility/UtilityFunctions.cpp'
with open(util_path, 'r', encoding='utf-8') as f:
    content = f.read()

create_skeleton_old = r'''Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);

    return skeleton;
}'''

create_skeleton_new = r'''Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);
    
    // Log joint count
    LogManager::GetInstance()->Log("Skeleton created with " + std::to_string(skeleton.joints.size()) + " joints.\n");

    return skeleton;
}'''

content = content.replace(create_skeleton_old, create_skeleton_new)

with open(util_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Adjusted scale and added log.")
