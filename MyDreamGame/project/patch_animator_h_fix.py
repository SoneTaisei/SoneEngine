import os

h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.h'
with open(h_path, 'r', encoding='utf-8') as f:
    content = f.read()

if 'const SkinCluster& GetSkinCluster() const' not in content:
    content = content.replace(
        'const Skeleton& GetSkeleton() const { return skeleton_; }',
        'const Skeleton& GetSkeleton() const { return skeleton_; }\n    const SkinCluster& GetSkinCluster() const { return skinCluster_; }'
    )
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Added GetSkinCluster to AnimatorComponent.h")
else:
    print("GetSkinCluster already exists.")
