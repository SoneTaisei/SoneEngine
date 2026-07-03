#pragma once
#include "IComponent.h"
#include "Core/Utility/Structs.h"

class TransformComponent : public IComponent {
public:
    TransformComponent() = default;
    ~TransformComponent() override = default;

    void Initialize() override;
    void Update() override;
    void DisplayImGui() override;

    void UpdateMatrix();

    // Setters
    void SetPosition(const Vector3& pos) { transform_.translate = pos; isDirty_ = true; }
    void SetRotation(const Vector3& rot) { transform_.rotate = rot; isDirty_ = true; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; isDirty_ = true; }
    void SetParent(TransformComponent* parent) { parent_ = parent; isDirty_ = true; }

    // Getters
    const Vector3& GetPosition() const { return transform_.translate; }
    const Vector3& GetRotation() const { return transform_.rotate; }
    const Vector3& GetScale() const { return transform_.scale; }
    const Transform& GetTransform() const { return transform_; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

private:
    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Matrix4x4 worldMatrix_{};
    TransformComponent* parent_ = nullptr;
    bool isDirty_ = true;
};
