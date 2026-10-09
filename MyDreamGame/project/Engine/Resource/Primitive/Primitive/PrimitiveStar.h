#pragma once
#include "Resource/Primitive/Primitive.h"

class PrimitiveStar : public Primitive {
public:
    PrimitiveStar(float size);
    void GenerateModelData() override;

private:
    float size_;
};
