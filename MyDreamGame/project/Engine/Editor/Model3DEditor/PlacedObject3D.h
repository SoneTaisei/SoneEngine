#pragma once
#include <string>
#include <memory>
#include <d3d12.h>
#include <nlohmann/json.hpp>
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Vector4.h"
#include "GameObject/Object3D.h"

class Model;

class PlacedObject3D {
public:
    PlacedObject3D();
    PlacedObject3D(const std::string& name, const std::string& modelDirectory, const std::string& modelFileName);
    ~PlacedObject3D() = default;

    bool Initialize(ID3D12Device* device);
    void Update();
    void Draw();

    // Getter / Setter
    uint64_t GetId() const { return id_; }
    void SetId(uint64_t id) { id_ = id; }

    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name);

    const std::string& GetModelDirectory() const { return modelDirectory_; }
    const std::string& GetModelFileName() const { return modelFileName_; }
    void SetModel(const std::string& modelDirectory, const std::string& modelFileName, ID3D12Device* device);

    const std::string& GetTexturePath() const { return texturePath_; }
    void SetTexture(const std::string& texturePath);

    Vector3 GetTranslation() const;
    Vector3 GetRotation() const; // in radians
    Vector3 GetRotationDegrees() const; // in degrees
    Vector3 GetScale() const;

    void SetTranslation(const Vector3& translation);
    void SetRotation(const Vector3& rotation); // in radians
    void SetRotationDegrees(const Vector3& rotationDeg); // in degrees
    void SetScale(const Vector3& scale);

    Vector4 GetColor() const;
    void SetColor(const Vector4& color);

    bool IsDoubleSided() const { return isDoubleSided_; }
    void SetDoubleSided(bool doubleSided);

    Object3D* GetObject3D() { return object3D_.get(); }
    const Object3D* GetObject3D() const { return object3D_.get(); }

    // Bounding Box (AABB) in world space for raycast picking
    void GetWorldBounds(Vector3& outMin, Vector3& outMax) const;

    // Raycast intersection test: returns true if ray intersects this object, and sets outT to distance
    bool IntersectRay(const Vector3& rayOrigin, const Vector3& rayDir, float& outT) const;

    // JSON serialization
    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& j, ID3D12Device* device);

private:
    uint64_t id_ = 0;
    std::string name_ = "PlacedModel";
    std::string modelDirectory_ = "";
    std::string modelFileName_ = "";
    std::string texturePath_ = "";
    bool isDoubleSided_ = false;

    Vector3 translation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // Radians
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    std::unique_ptr<Object3D> object3D_;
    Model* model_ = nullptr;
};
