#pragma once
#include "Core/Utility/Structs.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Resource/Primitive/Primitive.h"
#include "Core/Utility/BlendMode.h"

class Renderer;

class PrimitiveObject {
public:
    void Initialize(ID3D12Device* device, Primitive* primitive);
    void Update();
    void Draw();
    void DisplayImGui(const std::string& label);

    const Vector3& GetTranslation() const { return transform_.translate; }
    void SetTranslation(const Vector3& translate) { transform_.translate = translate; }
    const Vector3& GetRotation() const { return transform_.rotate; }
    void SetRotation(const Vector3& rotate) { transform_.rotate = rotate; }
    const Vector3& GetScale() const { return transform_.scale; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // 直接WorldMatrixを設定するフラグと行列
    void SetOverrideMatrix(bool enable, const Matrix4x4& matrix = {}) { 
        overrideMatrix_ = enable; 
        overriddenWorldMatrix_ = matrix; 
    }

    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle_ = handle; }
    static void SetDefaultTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { sDefaultTextureHandle_ = handle; }
    const Material& GetMaterial() const { return material_; }
    void SetMaterial(const Material& material) { material_ = material; }
    void SetColor(const Vector4& color) { material_.color = color; }
    const Vector4& GetColor() const { return material_.color; }
    void SetLightingType(int32_t type) { material_.lightingType = type; }
    void SetDissolveThreshold(float threshold) { material_.dissolveThreshold = threshold; }
    void SetAlpha(float alpha) { material_.color.w = alpha; }
    void SetEnableEnvironmentMap(int32_t enable) { material_.enableEnvironmentMap = enable; }
    void SetEnableBoxMapping(float enable) { material_.enableBoxMapping = enable; }
    void SetUVTransform(const Matrix4x4 &uvTransform) { material_.uvTransform = uvTransform; }
    void SetAlphaReference(float alphaRef) { material_.alphaReference = alphaRef; }
    void SetShininess(float shininess) { material_.shininess = shininess; }

    // --- 親子関係 ---
    void SetParent(PrimitiveObject* parent) { parent_ = parent; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    void SetIsBillboard(bool isBillboard) { isBillboard_ = isBillboard; }
    void SetIsDoubleSided(bool isDoubleSided) { isDoubleSided_ = isDoubleSided; }
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    const BlendMode GetBlendMode() const { return blendMode_; }

    // --- 名前関連 ---
    const std::string &GetName() const { return name_; }
    void SetName(const std::string &name) { name_ = name; }

    // --- 描画・Renderer用公開Getter ---
    const Material *GetMappedMaterial() const { return mappedMaterial_; }
    const TransformMatrix *GetMappedTransform() const { return mappedTransform_; }
    ID3D12Resource *GetTransformResource() const { return transformResource_.Get(); }
    ID3D12Resource *GetMaterialResource() const { return materialResource_.Get(); }
    void SetWorldMatrix(const Matrix4x4 &worldMatrix) { worldMatrix_ = worldMatrix; }
    const Primitive *GetPrimitive() const { return primitive_; }
    const PrimitiveObject *GetParent() const { return parent_; }
    bool IsBillboard() const { return isBillboard_; }
    bool IsDoubleSided() const { return isDoubleSided_; }
    const D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle_; }
    static D3D12_GPU_DESCRIPTOR_HANDLE GetDefaultTextureHandle() { return sDefaultTextureHandle_; }
    static int GetMaxGhosts() { return kMaxGhosts; }
    const int GetCurrentGhostIndex() const { return currentGhostIndex_; }
    void IncrementGhostIndex() { currentGhostIndex_++; }
    const uint8_t *GetMappedGhostTransform() const { return mappedGhostTransform_; }
    const uint8_t *GetMappedGhostMaterial() const { return mappedGhostMaterial_; }
    ID3D12Resource *GetGhostTransformResource() const { return ghostTransformResource_.Get(); }
    ID3D12Resource *GetGhostMaterialResource() const { return ghostMaterialResource_.Get(); }

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
    EulerTransform transform_;
    Material material_;
    Matrix4x4 worldMatrix_;

    PrimitiveObject* parent_ = nullptr;
    bool isBillboard_ = false;
    bool isDoubleSided_ = false;
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;

    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};
    static D3D12_GPU_DESCRIPTOR_HANDLE sDefaultTextureHandle_;

    bool overrideMatrix_ = false;
    Matrix4x4 overriddenWorldMatrix_ = {};

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
    const bool GetShowTrail() const { return showTrail_; }
    void SetShowTrail(bool show) { showTrail_ = show; }

    const  EulerTransform &GetTransform() const { return transform_; }

    void ResetGhostIndex() { currentGhostIndex_ = 0; }
    void DrawGhost(const EulerTransform& transform, const Material& material);
};
