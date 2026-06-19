#pragma once
#include "Primitive.h"

class PrimitiveRing : public Primitive {
public:
    PrimitiveRing(float innerRadius, float outerRadius, uint32_t segments, float startAngle = 0.0f, float endAngle = 2.0f * std::numbers::pi_v<float>, const Vector4& innerColor = {1,1,1,1}, const Vector4& outerColor = {1,1,1,1}, bool isRadialUV = false);
    void GenerateModelData() override;

private:
    float innerRadius_;
    float outerRadius_;
    uint32_t segments_;
    float startAngle_;
    float endAngle_;
    Vector4 innerColor_;
    Vector4 outerColor_;
    bool isRadialUV_;
};
