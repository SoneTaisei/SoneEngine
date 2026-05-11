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

Primitive* PrimitiveManager::GetRing(float innerRadius, float outerRadius, uint32_t segments, float startAngle, float endAngle, const Vector4& innerColor, const Vector4& outerColor, bool isRadialUV) {
    // 全てのパラメータをキーに含める
    std::string key = std::format("Ring_R{:.2f}_r{:.2f}_S{}_A{:.2f}_a{:.2f}_C{:.2f}{:.2f}{:.2f}{:.2f}_c{:.2f}{:.2f}{:.2f}{:.2f}_{}",
        outerRadius, innerRadius, segments, startAngle, endAngle,
        innerColor.x, innerColor.y, innerColor.z, innerColor.w,
        outerColor.x, outerColor.y, outerColor.z, outerColor.w,
        isRadialUV ? 1 : 0);

    if (primitiveRegistry_.contains(key)) {
        return primitiveRegistry_[key].get();
    }

    auto primitive = std::make_unique<Primitive>();
    // Primitive クラスの初期化をバイパスして直接生成メソッドを呼ぶために、Initializeを少し工夫するか
    // ここでは Initialize を使わず直接 CreateRing を呼びたいが、Primitive::Initialize は CreateBuffers を呼んでいる
    // Initialize のインターフェースを変えるか、CreateRing を Public にするか...
    // Primitive.h で CreateRing を Public にするのが楽
    
    // 一旦 Initialize(device_, type) を呼んでから CreateRing を呼び直すと無駄なので
    // Primitive クラスに初期化済みの状態で Buffers を作るメソッドが必要
    
    // Primitive.h を見ると Initialize は CreateBuffers を呼んでいる。
    // 手っ取り早いのは、GetRing 内で primitive->CreateRing(...) と primitive->CreateBuffers(...) を呼ぶこと
    // そのためには CreateRing と CreateBuffers を Public にする必要がある。
    
    // または Primitive::InitializeRing を追加する。これが一番綺麗。
    primitive->InitializeRing(device_, innerRadius, outerRadius, segments, startAngle, endAngle, innerColor, outerColor, isRadialUV);
    primitiveRegistry_[key] = std::move(primitive);

    return primitiveRegistry_[key].get();
}
