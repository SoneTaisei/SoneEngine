#pragma once
#include "Core/Utility/Structs.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Resource/Model/Model.h"

class Object3D {
public:
    void Initialize(ID3D12Device *device, Model *model);
    void Update(const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix);
    void Draw(ID3D12GraphicsCommandList *commandList);
    void DisplayImGui(const std::string &label);

    // --- Transformのゲッター ---
    const Vector3 &GetTranslation() const { return transform_.translate; }
    const Vector3 &GetRotation() const { return transform_.rotate; }
    const Vector3 &GetScale() const { return transform_.scale; }

    // --- Transformのセッター ---
    void SetTranslation(const Vector3 &translate) { transform_.translate = translate; }
    void SetRotation(const Vector3 &rotate) { transform_.rotate = rotate; }
    void SetScale(const Vector3 &scale) { transform_.scale = scale; }
    
    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        if (model_) {
            model_->SetTextureHandle(handle);
        }
    }

private:
    // マテリアル
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material *mappedMaterial_ = nullptr;

    // 平行光源 (Directional Light)
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight *mappedLight_ = nullptr;

    // 座標変換 (World, WVP)
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformMatrix *mappedTransform_ = nullptr;

    // ポイントライト (Point Light)
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight *mappedPointLight_ = nullptr; // 名前を統一感あるものに変更

    Model *model_ = nullptr;

    // CPU側データ
    Transform transform_;
    Material material_;
    DirectionalLight light_;
    PointLight pointLight_; // CPU側でも値を保持しておくと便利

    // テクスチャ関連
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
    uint32_t textureSrvHandle_ = 0;

    ModelData *modelData_ = nullptr;
};