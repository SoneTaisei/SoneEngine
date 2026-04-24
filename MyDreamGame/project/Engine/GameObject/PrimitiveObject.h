#pragma once
#include "Core/Utility/Structs.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Resource/Primitive/Primitive.h"

class PrimitiveObject {
public:
    void Initialize(ID3D12Device* device, Primitive* primitive);
    void Update();
    void Draw(ID3D12GraphicsCommandList* commandList);
    void DisplayImGui(const std::string& label);

    // --- Transformのゲッター/セッター ---
    const Vector3& GetTranslation() const { return transform_.translate; }
    void SetTranslation(const Vector3& translate) { transform_.translate = translate; }
    const Vector3& GetRotation() const { return transform_.rotate; }
    void SetRotation(const Vector3& rotate) { transform_.rotate = rotate; }
    const Vector3& GetScale() const { return transform_.scale; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle_ = handle; }
    Material& GetMaterial() { return material_; }

private:
    Primitive* primitive_ = nullptr;

    // マテリアル
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* mappedMaterial_ = nullptr;

    // 座標変換
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformMatrix* mappedTransform_ = nullptr;

    // CPU側データ
    Transform transform_;
    Material material_;

    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};
};
