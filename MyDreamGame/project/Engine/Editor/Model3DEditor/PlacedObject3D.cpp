#include "PlacedObject3D.h"
#include "Resource/Model/ModelManager.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/TransformFunctions.h"
#include <cmath>
#include <algorithm>

#include <atomic>

namespace {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    std::atomic<uint64_t> sNextObjectId{ 1 };
}

PlacedObject3D::PlacedObject3D()
    : id_(sNextObjectId.fetch_add(1)) {
}

PlacedObject3D::PlacedObject3D(const std::string& name, const std::string& modelDirectory, const std::string& modelFileName)
    : id_(sNextObjectId.fetch_add(1)), name_(name), modelDirectory_(modelDirectory), modelFileName_(modelFileName) {
}

bool PlacedObject3D::Initialize(ID3D12Device* device) {
    if (modelDirectory_.empty() || modelFileName_.empty()) {
        return false;
    }

    model_ = ModelManager::GetInstance()->GetModel(modelDirectory_, modelFileName_);
    if (!model_) {
        return false;
    }

    object3D_ = std::make_unique<Object3D>();
    object3D_->Initialize(device, model_);
    object3D_->SetName(name_);
    object3D_->SetTranslation(translation_);
    object3D_->SetRotation(rotation_);
    object3D_->SetScale(scale_);
    object3D_->GetMaterial().color = color_;
    object3D_->SetIsDoubleSided(isDoubleSided_);

    if (!texturePath_.empty()) {
        uint32_t texIdx = TextureManager::GetInstance()->Load(texturePath_);
        object3D_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texIdx));
    }

    object3D_->Update();
    return true;
}

void PlacedObject3D::Update() {
    if (object3D_) {
        object3D_->SetName(name_);
        object3D_->SetTranslation(translation_);
        object3D_->SetRotation(rotation_);
        object3D_->SetScale(scale_);
        object3D_->GetMaterial().color = color_;
        object3D_->SetIsDoubleSided(isDoubleSided_);
        object3D_->Update();
    }
}

void PlacedObject3D::Draw() {
    if (object3D_) {
        object3D_->Draw();
    }
}

void PlacedObject3D::SetName(const std::string& name) {
    name_ = name;
    if (object3D_) {
        object3D_->SetName(name);
    }
}

void PlacedObject3D::SetModel(const std::string& modelDirectory, const std::string& modelFileName, ID3D12Device* device) {
    modelDirectory_ = modelDirectory;
    modelFileName_ = modelFileName;
    model_ = ModelManager::GetInstance()->GetModel(modelDirectory_, modelFileName_);
    if (model_ && device) {
        if (!object3D_) {
            object3D_ = std::make_unique<Object3D>();
        }
        object3D_->Initialize(device, model_);
        Update();
    }
}

void PlacedObject3D::SetTexture(const std::string& texturePath) {
    texturePath_ = texturePath;
    if (object3D_ && !texturePath_.empty()) {
        uint32_t texIdx = TextureManager::GetInstance()->Load(texturePath_);
        object3D_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texIdx));
    }
}

Vector3 PlacedObject3D::GetTranslation() const {
    return translation_;
}

Vector3 PlacedObject3D::GetRotation() const {
    return rotation_;
}

Vector3 PlacedObject3D::GetRotationDegrees() const {
    return { rotation_.x * kRadToDeg, rotation_.y * kRadToDeg, rotation_.z * kRadToDeg };
}

Vector3 PlacedObject3D::GetScale() const {
    return scale_;
}

void PlacedObject3D::SetTranslation(const Vector3& translation) {
    translation_ = translation;
    if (object3D_) {
        object3D_->SetTranslation(translation);
    }
}

void PlacedObject3D::SetRotation(const Vector3& rotation) {
    rotation_ = rotation;
    if (object3D_) {
        object3D_->SetRotation(rotation);
    }
}

void PlacedObject3D::SetRotationDegrees(const Vector3& rotationDeg) {
    rotation_ = { rotationDeg.x * kDegToRad, rotationDeg.y * kDegToRad, rotationDeg.z * kDegToRad };
    if (object3D_) {
        object3D_->SetRotation(rotation_);
    }
}

void PlacedObject3D::SetScale(const Vector3& scale) {
    scale_ = scale;
    if (object3D_) {
        object3D_->SetScale(scale);
    }
}

Vector4 PlacedObject3D::GetColor() const {
    return color_;
}

void PlacedObject3D::SetColor(const Vector4& color) {
    color_ = color;
    if (object3D_) {
        object3D_->GetMaterial().color = color;
    }
}

void PlacedObject3D::SetDoubleSided(bool doubleSided) {
    isDoubleSided_ = doubleSided;
    if (object3D_) {
        object3D_->SetIsDoubleSided(doubleSided);
    }
}

void PlacedObject3D::GetWorldBounds(Vector3& outMin, Vector3& outMax) const {
    Vector3 localMin = { -0.5f, -0.5f, -0.5f };
    Vector3 localMax = { 0.5f, 0.5f, 0.5f };

    if (model_) {
        const auto& vertices = model_->GetModelData().vertices;
        if (!vertices.empty()) {
            localMin = { 1e9f, 1e9f, 1e9f };
            localMax = { -1e9f, -1e9f, -1e9f };
            for (const auto& v : vertices) {
                localMin.x = (std::min)(localMin.x, v.position.x);
                localMin.y = (std::min)(localMin.y, v.position.y);
                localMin.z = (std::min)(localMin.z, v.position.z);
                localMax.x = (std::max)(localMax.x, v.position.x);
                localMax.y = (std::max)(localMax.y, v.position.y);
                localMax.z = (std::max)(localMax.z, v.position.z);
            }
        }
    }

    // Transform local bounding box 8 corners to world space
    Vector3 corners[8] = {
        { localMin.x, localMin.y, localMin.z },
        { localMax.x, localMin.y, localMin.z },
        { localMin.x, localMax.y, localMin.z },
        { localMax.x, localMax.y, localMin.z },
        { localMin.x, localMin.y, localMax.z },
        { localMax.x, localMin.y, localMax.z },
        { localMin.x, localMax.y, localMax.z },
        { localMax.x, localMax.y, localMax.z }
    };

    Matrix4x4 worldMat = TransformFunctions::MakeAffineMatrix(scale_, rotation_, translation_);

    outMin = { 1e9f, 1e9f, 1e9f };
    outMax = { -1e9f, -1e9f, -1e9f };

    for (int i = 0; i < 8; ++i) {
        Vector3 worldPt = TransformFunctions::EulerTransform(corners[i], worldMat);
        outMin.x = (std::min)(outMin.x, worldPt.x);
        outMin.y = (std::min)(outMin.y, worldPt.y);
        outMin.z = (std::min)(outMin.z, worldPt.z);
        outMax.x = (std::max)(outMax.x, worldPt.x);
        outMax.y = (std::max)(outMax.y, worldPt.y);
        outMax.z = (std::max)(outMax.z, worldPt.z);
    }
}

bool PlacedObject3D::IntersectRay(const Vector3& rayOrigin, const Vector3& rayDir, float& outT) const {
    Vector3 boxMin, boxMax;
    GetWorldBounds(boxMin, boxMax);

    // Padding slightly to make picking easier for thin objects
    const float pad = 0.1f;
    boxMin.x -= pad; boxMin.y -= pad; boxMin.z -= pad;
    boxMax.x += pad; boxMax.y += pad; boxMax.z += pad;

    float tmin = 0.0f;
    float tmax = 1e9f;

    for (int i = 0; i < 3; ++i) {
        float o = (i == 0 ? rayOrigin.x : (i == 1 ? rayOrigin.y : rayOrigin.z));
        float d = (i == 0 ? rayDir.x : (i == 1 ? rayDir.y : rayDir.z));
        float bMin = (i == 0 ? boxMin.x : (i == 1 ? boxMin.y : boxMin.z));
        float bMax = (i == 0 ? boxMax.x : (i == 1 ? boxMax.y : boxMax.z));

        if (std::abs(d) < 1e-6f) {
            if (o < bMin || o > bMax) return false;
        } else {
            float invD = 1.0f / d;
            float t1 = (bMin - o) * invD;
            float t2 = (bMax - o) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = (std::max)(tmin, t1);
            tmax = (std::min)(tmax, t2);
            if (tmin > tmax) return false;
        }
    }

    outT = tmin;
    return true;
}

nlohmann::json PlacedObject3D::ToJson() const {
    nlohmann::json j;
    j["name"] = name_;
    j["modelDirectory"] = modelDirectory_;
    j["modelFileName"] = modelFileName_;
    j["texturePath"] = texturePath_;
    j["isDoubleSided"] = isDoubleSided_;

    j["translation"] = { translation_.x, translation_.y, translation_.z };
    j["rotation"] = { rotation_.x, rotation_.y, rotation_.z };
    j["scale"] = { scale_.x, scale_.y, scale_.z };
    j["color"] = { color_.x, color_.y, color_.z, color_.w };
    return j;
}

bool PlacedObject3D::FromJson(const nlohmann::json& j, ID3D12Device* device) {
    if (j.contains("name")) name_ = j["name"].get<std::string>();
    if (j.contains("modelDirectory")) modelDirectory_ = j["modelDirectory"].get<std::string>();
    if (j.contains("modelFileName")) modelFileName_ = j["modelFileName"].get<std::string>();
    if (j.contains("texturePath")) texturePath_ = j["texturePath"].get<std::string>();
    if (j.contains("isDoubleSided")) isDoubleSided_ = j["isDoubleSided"].get<bool>();

    if (j.contains("translation") && j["translation"].is_array() && j["translation"].size() == 3) {
        translation_ = { j["translation"][0], j["translation"][1], j["translation"][2] };
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 3) {
        rotation_ = { j["rotation"][0], j["rotation"][1], j["rotation"][2] };
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3) {
        scale_ = { j["scale"][0], j["scale"][1], j["scale"][2] };
    }
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
        color_ = { j["color"][0], j["color"][1], j["color"][2], j["color"][3] };
    }

    return Initialize(device);
}
