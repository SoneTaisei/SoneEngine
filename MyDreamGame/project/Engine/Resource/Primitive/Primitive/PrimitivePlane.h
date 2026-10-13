#pragma once
#include "Resource/Primitive/Primitive.h"

class PrimitivePlane : public Primitive {
public:
    PrimitivePlane(float size);
    void GenerateModelData() override;

private:
    float size_;
};
