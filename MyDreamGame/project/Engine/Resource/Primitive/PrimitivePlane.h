#pragma once
#include "Primitive.h"

class PrimitivePlane : public Primitive {
public:
    PrimitivePlane(float size);
    void GenerateModelData() override;

private:
    float size_;
};
