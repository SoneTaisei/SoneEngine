#include "PrimitiveManager.h"
#include <format>

void PrimitiveManager::Initialize(ID3D12Device* device) {
    device_ = device;
}

Primitive* PrimitiveManager::GetPrimitive(PrimitiveType type, float size, uint32_t segments) {
    // キーを作成 (例: "Sphere_1.0_32")
    std::string key = std::format("{}_{:.2f}_{}", static_cast<int>(type), size, segments);

    // 登録済みならそれを返す
    if (primitiveRegistry_.contains(key)) {
        return primitiveRegistry_[key].get();
    }

    // 未登録なら生成
    auto primitive = std::make_unique<Primitive>();
    primitive->Initialize(device_, type, size, segments);
    primitiveRegistry_[key] = std::move(primitive);

    return primitiveRegistry_[key].get();
}
