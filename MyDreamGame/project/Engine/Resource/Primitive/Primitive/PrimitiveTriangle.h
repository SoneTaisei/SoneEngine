#pragma once
#include "Resource/Primitive/Primitive.h"

class PrimitiveTriangle : public Primitive {
public:
    PrimitiveTriangle(float size);
    void GenerateModelData() override;

private:
    float size_;
};
