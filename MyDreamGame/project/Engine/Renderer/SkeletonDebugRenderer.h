#pragma once

#include "GameObject/PrimitiveObject.h"
#include "Core/Utility/Structs.h"
#include <vector>
#include <memory>

class SkeletonDebugRenderer {
public:
    void Initialize();
    void Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix);

private:
    std::vector<std::unique_ptr<PrimitiveObject>> jointSpheres_;
    std::vector<std::unique_ptr<PrimitiveObject>> boneCylinders_;
};
