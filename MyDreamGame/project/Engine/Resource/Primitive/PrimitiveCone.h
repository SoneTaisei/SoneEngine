#pragma once
#include "Primitive.h"

class PrimitiveCone : public Primitive {
public:
    PrimitiveCone(float radius, float height, uint32_t segments);
    void GenerateModelData() override;

private:
    float radius_;
    float height_;
    uint32_t segments_;
};
