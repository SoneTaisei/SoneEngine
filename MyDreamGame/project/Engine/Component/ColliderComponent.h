#pragma once
#include "IComponent.h"
#include "Core/Utility/Structs.h"

enum class ColliderType {
    kBox,
    kSphere
};

class ColliderComponent : public IComponent {
public:
    ColliderComponent() = default;
    ~ColliderComponent() override = default;

    void Initialize() override {}
    void Update() override;
    void DisplayImGui() override;

    // Type Setters
    void SetColliderType(ColliderType type) { type_ = type; }
    ColliderType GetColliderType() const { return type_; }

    // Box Collider
    void SetBoxSize(const Vector3& size) { boxSize_ = size; }
    void SetBoxOffset(const Vector3& offset) { boxOffset_ = offset; }
    Vector3 GetBoxSize() const { return boxSize_; }
    Vector3 GetBoxOffset() const { return boxOffset_; }

    // Sphere Collider
    void SetSphereRadius(float radius) { sphereRadius_ = radius; }
    void SetSphereOffset(const Vector3& offset) { sphereOffset_ = offset; }
    float GetSphereRadius() const { return sphereRadius_; }
    Vector3 GetSphereOffset() const { return sphereOffset_; }

    // OnCollision callback type
    using OnCollisionCallback = void(*)(ColliderComponent* other);
    void SetOnCollisionCallback(OnCollisionCallback callback) { onCollision_ = callback; }

private:
    ColliderType type_ = ColliderType::kBox;

    // Box params
    Vector3 boxSize_{ 1.0f, 1.0f, 1.0f };
    Vector3 boxOffset_{ 0.0f, 0.0f, 0.0f };

    // Sphere params
    float sphereRadius_ = 1.0f;
    Vector3 sphereOffset_{ 0.0f, 0.0f, 0.0f };

    OnCollisionCallback onCollision_ = nullptr;
};
