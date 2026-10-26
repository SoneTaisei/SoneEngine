#pragma once
#include "IComponent.h"
#include "Resource/Primitive/Primitive.h"
#include "Core/Utility/Structs.h"
#include "Core/Utility/BlendMode.h"
#include "Renderer/ConstantBufferPool.h"
#include <wrl/client.h>

// GameObjectにアタッチして基本図形（Primitive）を描画するためのコンポーネント
class PrimitiveRendererComponent : public IComponent {
public:
    PrimitiveRendererComponent();
    ~PrimitiveRendererComponent() override;

    void Initialize(ID3D12Device* device, Primitive* primitive);
    
    // IComponent overrides
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DisplayImGui() override;

    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle_ = handle; }
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
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    const BlendMode GetBlendMode() const { return blendMode_; }
    
    // トレイル描画用
    const bool GetShowTrail() const { return showTrail_; }
    void SetShowTrail(bool show) { showTrail_ = show; }
    void DrawGhost(ID3D12GraphicsCommandList* commandList, const EulerTransform& transform, const Material& material);

private:
    void UpdateGhost(const EulerTransform& currentTransform);
    // 残像用バッファ（1コンポーネントで数MB）を必要になった時にだけ確保する
    void EnsureGhostResources();

    ID3D12Device* device_ = nullptr;

    Primitive* primitive_ = nullptr;
    Material material_;
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    // 定数バッファは ConstantBufferPool から切り出して使う
    // （ブロック 1 個ごとにバッファを作ると、マップを開くたびの生成回数が跳ね上がるため）
    ConstantBufferPool::Allocation materialCB_;
    Material* mappedMaterial_ = nullptr;

    ConstantBufferPool::Allocation transformCB_;
    TransformMatrix* mappedTransform_ = nullptr;

    // トレイル（残像）用データ
    bool showTrail_ = false;
    static const int kMaxGhosts = 10000;
    Microsoft::WRL::ComPtr<ID3D12Resource> ghostTransformResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> ghostMaterialResource_;
    uint8_t* mappedGhostTransform_ = nullptr;
    uint8_t* mappedGhostMaterial_ = nullptr;
    int currentGhostIndex_ = 0;

    bool isBillboard_ = false;
    bool isDoubleSided_ = false;

public:
    void SetIsBillboard(bool b) { isBillboard_ = b; }
    bool IsBillboard() const { return isBillboard_; }
    void SetIsDoubleSided(bool d) { isDoubleSided_ = d; }
    bool IsDoubleSided() const { return isDoubleSided_; }

    const D3D12_GPU_VIRTUAL_ADDRESS GetTransformGPUAddress() const { return transformCB_.gpuAddress; }
    const TransformMatrix* GetMappedTransform() const { return mappedTransform_; }
    const D3D12_GPU_VIRTUAL_ADDRESS GetMaterialGPUAddress() const { return materialCB_.gpuAddress; }
    const D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle_; }
    const Primitive* GetPrimitive() const { return primitive_; }

    const int GetCurrentGhostIndex() const { return currentGhostIndex_; }
    const ID3D12Resource* GetGhostTransformResource() const { return ghostTransformResource_.Get(); }
    const ID3D12Resource* GetGhostMaterialResource() const { return ghostMaterialResource_.Get(); }
    const uint8_t* GetMappedGhostTransform() const { return mappedGhostTransform_; }
    const uint8_t* GetMappedGhostMaterial() const { return mappedGhostMaterial_; }
    void IncrementGhostIndex() { currentGhostIndex_++; }
};
