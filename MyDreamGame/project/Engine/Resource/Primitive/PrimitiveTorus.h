#pragma once
#include "Primitive.h"

class PrimitiveTorus : public Primitive {
public:
    PrimitiveTorus(float ringRadius, float tubeRadius, uint32_t segments);
    void GenerateModelData() override;

private:
    float ringRadius_;
    float tubeRadius_;
    uint32_t segments_;
};
