import os

h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.h'

with open(h_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Structs.hのインクルード追加
if '#include "Core/Utility/Structs.h"' not in content:
    content = content.replace('#include "Core/Utility/Animation.h"', '#include "Core/Utility/Animation.h"\n#include "Core/Utility/Structs.h"')

# SetModelDataとDrawDebugの追加
new_methods = r'''
    void SetModelData(const ModelData& modelData);
    void DrawDebug(const ViewProjection& viewProjection);

    const Skeleton& GetSkeleton() const { return skeleton_; }
'''
if 'void SetModelData' not in content:
    content = content.replace('void SetTargetNodeName(const std::string& name) { targetNodeName_ = name; }', 'void SetTargetNodeName(const std::string& name) { targetNodeName_ = name; }\n' + new_methods)

# メンバ変数の追加
new_members = r'''
    Skeleton skeleton_;
    SkinCluster skinCluster_;
    bool hasSkeleton_ = false;
'''
if 'Skeleton skeleton_;' not in content:
    content = content.replace('std::string targetNodeName_ = "Cube"; // Default node name for AnimatedCube.gltf', 'std::string targetNodeName_ = "Cube";\n' + new_members)

with open(h_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Updated AnimatorComponent.h")
