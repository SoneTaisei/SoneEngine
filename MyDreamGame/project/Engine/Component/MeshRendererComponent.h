#pragma once
#include "IComponent.h"
#include "Resource/Model/Model.h"
#include "Core/Utility/Structs.h"
#include "Core/Utility/BlendMode.h"
#include "Renderer/ConstantBufferPool.h"
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

    // モデル・テクスチャ情報 (シリアライズ用)
    void SetModelInfo(const std::string& directoryPath, const std::string& fileName) {
        modelDirectory_ = directoryPath;
        modelFileName_ = fileName;
    }
    const std::string& GetModelDirectory() const { return modelDirectory_; }
    const std::string& GetModelFileName() const { return modelFileName_; }
    void SetTexturePath(const std::string& path) { texturePath_ = path; }
    const std::string& GetTexturePath() const { return texturePath_; }

private:
    Model* model_ = nullptr;
    std::string modelDirectory_;
    std::string modelFileName_;
    std::string texturePath_;
    Material material_;
    BlendMode blendMode_ = BlendMode::kBlendModeNormal;
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_{};

    // 定数バッファは ConstantBufferPool から切り出して使う
    // （ブロック 1 個ごとにバッファを作ると、マップを開くたびの生成回数が跳ね上がるため）
    ConstantBufferPool::Allocation materialCB_;
    Material* mappedMaterial_ = nullptr;

    ConstantBufferPool::Allocation transformCB_;
    TransformMatrix* mappedTransform_ = nullptr;
    
    // 描画関連フラグ
    bool isDoubleSided_ = false;

public:
    void SetIsDoubleSided(bool d) { isDoubleSided_ = d; }
    bool IsDoubleSided() const { return isDoubleSided_; }
    
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformGPUAddress() const { return transformCB_.gpuAddress; }
    TransformMatrix* GetMappedTransform() const { return mappedTransform_; }
    D3D12_GPU_VIRTUAL_ADDRESS GetMaterialGPUAddress() const { return materialCB_.gpuAddress; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle_; }
};
