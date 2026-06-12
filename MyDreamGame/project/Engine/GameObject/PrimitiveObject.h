#pragma once
#include "Core/Utility/Structs.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Resource/Primitive/Primitive.h"
#include "Core/Utility/BlendMode.h"
#include <deque>

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
    static void SetDefaultTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { sDefaultTextureHandle_ = handle; }
    Material& GetMaterial() { return material_; }

    // --- 親子関係 ---
    void SetParent(PrimitiveObject* parent) { parent_ = parent; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    void SetIsBillboard(bool isBillboard) { isBillboard_ = isBillboard; }
    void SetIsDoubleSided(bool isDoubleSided) { isDoubleSided_ = isDoubleSided; }
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    BlendMode GetBlendMode() const { return blendMode_; }

    // --- 名前関連 ---
    const std::string &GetName() const { return name_; }
    void SetName(const std::string &name) { name_ = name; }

private:
    std::string name_ = "PrimitiveObject";
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
    Matrix4x4 worldMatrix_;

    PrimitiveObject* parent_ = nullptr;
    bool isBillboard_ = false;
    bool isDoubleSided_ = false;
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;

    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};
    static D3D12_GPU_DESCRIPTOR_HANDLE sDefaultTextureHandle_;

    // --- 残像 (Trail) 設定 ---
    bool showTrail_ = false;

    // --- 外部からの残像(ゴースト)描画用 ---
    static const int kMaxGhosts = 10000;
    Microsoft::WRL::ComPtr<ID3D12Resource> ghostTransformResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> ghostMaterialResource_;
    uint8_t* mappedGhostTransform_ = nullptr;
    uint8_t* mappedGhostMaterial_ = nullptr;
    int currentGhostIndex_ = 0;

public:
    bool GetShowTrail() const { return showTrail_; }
    void SetShowTrail(bool show) { showTrail_ = show; }

    const Transform& GetTransform() const { return transform_; }

    void ResetGhostIndex() { currentGhostIndex_ = 0; }
    void DrawGhost(ID3D12GraphicsCommandList* commandList, const Transform& transform, const Material& material);
};
