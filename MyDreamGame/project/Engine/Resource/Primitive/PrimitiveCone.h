#pragma once
#include "Primitive.h"

class PrimitiveCone : public Primitive {
public:
    PrimitiveCone(float radius, float height, uint32_t segments);
    void GenerateModelData() override;

    float GetHeight() const { return height_; }
    float GetRadius() const { return radius_; }

private:
    float radius_;
    float height_;
    uint32_t segments_;
};
