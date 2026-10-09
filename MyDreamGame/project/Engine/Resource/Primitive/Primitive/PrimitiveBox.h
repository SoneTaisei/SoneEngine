#pragma once
#include "Resource/Primitive/Primitive.h"

class PrimitiveBox : public Primitive {
public:
    PrimitiveBox(float size);
    void GenerateModelData() override;

private:
    float size_;
};
