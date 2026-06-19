#pragma once
#include "Primitive.h"

class PrimitiveCircle : public Primitive {
public:
    PrimitiveCircle(float size, uint32_t segments);
    void GenerateModelData() override;

private:
    float size_;
    uint32_t segments_;
};
