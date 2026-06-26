#pragma once
#include "Primitive.h"

class PrimitiveSphere : public Primitive {
public:
    PrimitiveSphere(float size, uint32_t segments);
    void GenerateModelData() override;

private:
    float size_;
    uint32_t segments_;
};
