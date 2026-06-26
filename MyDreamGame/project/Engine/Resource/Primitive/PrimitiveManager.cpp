#include "PrimitiveManager.h"
#include "PrimitivePlane.h"
#include "PrimitiveBox.h"
#include "PrimitiveSphere.h"
#include "PrimitiveCircle.h"
#include "PrimitiveRing.h"
#include "PrimitiveCylinder.h"
#include "PrimitiveCone.h"
#include "PrimitiveTorus.h"
#include "PrimitiveTriangle.h"
#include "PrimitiveStar.h"
#include <format>

void PrimitiveManager::Initialize(ID3D12Device* device) {
    device_ = device;
}

Primitive* PrimitiveManager::GetPrimitive(PrimitiveType type, float size, uint32_t segments) {
    std::string key = std::format("{}_{:.2f}_{}", static_cast<int>(type), size, segments);

    if (primitiveRegistry_.contains(key)) {
        return primitiveRegistry_[key].get();
    }

    std::unique_ptr<Primitive> primitive;
    switch (type) {
        case PrimitiveType::Plane: primitive = std::make_unique<PrimitivePlane>(size); break;
        case PrimitiveType::Box: primitive = std::make_unique<PrimitiveBox>(size); break;
        case PrimitiveType::Sphere: primitive = std::make_unique<PrimitiveSphere>(size, segments); break;
        case PrimitiveType::Circle: primitive = std::make_unique<PrimitiveCircle>(size, segments); break;
        case PrimitiveType::Ring: primitive = std::make_unique<PrimitiveRing>(size * 0.5f, size, segments); break;
        case PrimitiveType::Cylinder: primitive = std::make_unique<PrimitiveCylinder>(size, size * 2.0f, segments); break;
        case PrimitiveType::Cone: primitive = std::make_unique<PrimitiveCone>(size, size * 2.0f, segments); break;
        case PrimitiveType::Torus: primitive = std::make_unique<PrimitiveTorus>(size, size * 0.3f, segments); break;
        case PrimitiveType::Triangle: primitive = std::make_unique<PrimitiveTriangle>(size); break;
        case PrimitiveType::Star: primitive = std::make_unique<PrimitiveStar>(size); break;
    }

    if (primitive) {
        primitive->Initialize(device_);
        primitiveRegistry_[key] = std::move(primitive);
        return primitiveRegistry_[key].get();
    }
    return nullptr;
}

Primitive* PrimitiveManager::GetRing(float innerRadius, float outerRadius, uint32_t segments, float startAngle, float endAngle, const Vector4& innerColor, const Vector4& outerColor, bool isRadialUV) {
    std::string key = std::format("Ring_R{:.2f}_r{:.2f}_S{}_A{:.2f}_a{:.2f}_C{:.2f}{:.2f}{:.2f}{:.2f}_c{:.2f}{:.2f}{:.2f}{:.2f}_{}",
        outerRadius, innerRadius, segments, startAngle, endAngle,
        innerColor.x, innerColor.y, innerColor.z, innerColor.w,
        outerColor.x, outerColor.y, outerColor.z, outerColor.w,
        isRadialUV ? 1 : 0);

    if (primitiveRegistry_.contains(key)) {
        return primitiveRegistry_[key].get();
    }

    auto primitive = std::make_unique<PrimitiveRing>(innerRadius, outerRadius, segments, startAngle, endAngle, innerColor, outerColor, isRadialUV);
    primitive->Initialize(device_);
    primitiveRegistry_[key] = std::move(primitive);

    return primitiveRegistry_[key].get();
}
