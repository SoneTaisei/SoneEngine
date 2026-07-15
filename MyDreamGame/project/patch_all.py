import os
import re

# 1. AnimatorComponent.h の修正
h_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.h'
with open(h_path, 'r', encoding='utf-8') as f:
    content = f.read()

# ViewProjection への依存を削除し、Matrix4x4 に変更
content = content.replace('void DrawDebug(const ViewProjection& viewProjection);', 'void DrawDebug(const Matrix4x4& worldMatrix);')
if '#include "Renderer/SkeletonDebugRenderer.h"' not in content:
    content = content.replace('#include "Core/Utility/Structs.h"', '#include "Core/Utility/Structs.h"\n#include "Renderer/SkeletonDebugRenderer.h"\n#include <memory>')

if 'std::unique_ptr<SkeletonDebugRenderer>' not in content:
    content = content.replace('bool hasSkeleton_ = false;', 'bool hasSkeleton_ = false;\n    std::unique_ptr<SkeletonDebugRenderer> debugRenderer_;')

with open(h_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 2. AnimatorComponent.cpp の修正
cpp_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Engine/Component/AnimatorComponent.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Update() -> ::Update() に修正
content = content.replace('Update(skeleton_); // 骨格空間のローカル・ワールド行列を計算', '::Update(skeleton_); // 骨格空間のローカル・ワールド行列を計算')

# SetModelData の修正
new_set_model = r'''void AnimatorComponent::SetModelData(const ModelData& modelData) {
    skeleton_ = CreateSkeleton(modelData.rootNode);
    hasSkeleton_ = true;
    debugRenderer_ = std::make_unique<SkeletonDebugRenderer>();
    debugRenderer_->Initialize();
}'''
content = re.sub(r'void AnimatorComponent::SetModelData\(const ModelData& modelData\) \{.*?hasSkeleton_ = true;\s*\}', new_set_model, content, flags=re.DOTALL)

# DrawDebug の修正
new_draw_debug = r'''void AnimatorComponent::DrawDebug(const Matrix4x4& worldMatrix) {
    if (hasSkeleton_ && debugRenderer_) {
        debugRenderer_->Draw(skeleton_, worldMatrix);
    }
}'''
content = re.sub(r'void AnimatorComponent::DrawDebug\(const ViewProjection& viewProjection\) \{.*?\}', new_draw_debug, content, flags=re.DOTALL)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)

# 3. StageSelectScene.cpp の修正
stage_path = 'c:/授業/学年/3年前期/自作エンジン/MyDreamGame/project/Project/Scenes/StageSelectScene.cpp'
with open(stage_path, 'r', encoding='utf-8') as f:
    content = f.read()

# DrawDebug(viewProjection) -> DrawDebug(worldMatrix) に変更
# GetWorldMatrix が無い可能性があるので、TransformComponentから計算するか、Object3DがWorldを持っているか。
# 今回は TransformComponent から直接取得する。TransformComponentには GetWorldMatrix があるか？
# もし無ければ TransformFunctions::MakeAffineMatrix で計算する
new_draw = r'''
    for (auto& obj : gameObjects_) {
        auto animator = obj->GetComponent<AnimatorComponent>();
        auto transform = obj->GetComponent<TransformComponent>();
        if (animator && transform) {
            Matrix4x4 world = TransformFunctions::MakeAffineMatrix(transform->scale_, transform->rotate_, transform->translate_);
            animator->DrawDebug(world);
        }
    }
'''
content = re.sub(r'for\s*\(\s*auto&\s*obj\s*:\s*gameObjects_\s*\)\s*\{\s*auto\s*animator\s*=\s*obj->GetComponent<AnimatorComponent>\(\);\s*if\s*\(animator\)\s*\{\s*animator->DrawDebug\(viewProjection\);\s*\}\s*\}', new_draw.strip(), content, flags=re.DOTALL)

with open(stage_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Patched Component and Scene.")
