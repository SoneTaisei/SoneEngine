#pragma once
#include "IComponent.h"
#include "Resource/Model/Model.h"
#include "Core/Utility/Structs.h"
#include "Core/Utility/BlendMode.h"
#include <wrl/client.h>
#include <string>

// GameObjectにアタッチして3Dモデル（Model）を描画するためのコンポーネント
class MeshRendererComponent : public IComponent {
public:
    MeshRendererComponent();
    ~MeshRendererComponent() override;

    void Initialize(ID3D12Device* device, Model* model);
    
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

    Model* GetModel() const { return model_; }
    void SetModel(Model* model) { model_ = model; }

private:
    Model* model_ = nullptr;
    Material material_;
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* mappedMaterial_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformMatrix* mappedTransform_ = nullptr;
    
    // 描画関連フラグ
    bool isDoubleSided_ = false;

public:
    void SetIsDoubleSided(bool d) { isDoubleSided_ = d; }
    bool IsDoubleSided() const { return isDoubleSided_; }
    
    ID3D12Resource* GetTransformResource() const { return transformResource_.Get(); }
    TransformMatrix* GetMappedTransform() const { return mappedTransform_; }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle_; }
};
