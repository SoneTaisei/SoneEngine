#pragma once
#include "IComponent.h"
#include "Resource/Primitive/Primitive.h"
#include "Core/Utility/Structs.h"
#include "Core/Utility/BlendMode.h"
#include <wrl/client.h>
#include <string>

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

    // Setters / Getters
    void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandle_ = handle; }
    Material& GetMaterial() { return material_; }
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    BlendMode GetBlendMode() const { return blendMode_; }
    
    // トレイル描画用
    bool GetShowTrail() const { return showTrail_; }
    void SetShowTrail(bool show) { showTrail_ = show; }
    void DrawGhost(ID3D12GraphicsCommandList* commandList, const EulerTransform& transform, const Material& material);

private:
    void UpdateGhost(const EulerTransform& currentTransform);

    Primitive* primitive_ = nullptr;
    Material material_;
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* mappedMaterial_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
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

    ID3D12Resource* GetTransformResource() const { return transformResource_.Get(); }
    TransformMatrix* GetMappedTransform() const { return mappedTransform_; }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle_; }
    Primitive* GetPrimitive() const { return primitive_; }

    int GetCurrentGhostIndex() const { return currentGhostIndex_; }
    ID3D12Resource* GetGhostTransformResource() const { return ghostTransformResource_.Get(); }
    ID3D12Resource* GetGhostMaterialResource() const { return ghostMaterialResource_.Get(); }
    uint8_t* GetMappedGhostTransform() const { return mappedGhostTransform_; }
    uint8_t* GetMappedGhostMaterial() const { return mappedGhostMaterial_; }
    void IncrementGhostIndex() { currentGhostIndex_++; }
};
