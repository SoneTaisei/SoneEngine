#pragma once
#include "../BaseBlock.h"

class BaseMovingBlock : public BaseMapObject {
public:
    using BaseMapObject::BaseMapObject;
    ~BaseMovingBlock() override = default;

    virtual Vector3 GetVelocity() const = 0;
    virtual float GetInertiaTime() const { return 0.5f; }

protected:
    void SetupMovingCollider();
};