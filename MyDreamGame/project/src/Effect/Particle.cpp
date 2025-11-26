#include "Particle.h"
#include "Graphics/TextureManager.h"
#include "Utility/TransformFunctions.h"
#include <cassert>

Particle::~Particle() {
    if(particleCommon_) {
        particleCommon_->RemoveParticle(this);
    }
}

void Particle::Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex) {
    particleCommon_ = particleCommon;
    kParticleCount_ = count;
    ID3D12Device *device = particleCommon_->GetDevice();
    blendMode_ = blendMode;

    // 1. Instancingリソースの作成
    UINT size = kParticleCount_ * sizeof(TransformMatrix);
    instancingResource_ = CreateBufferResource(device, size);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void **>(&instancingData_));

    // 初期化 (単位行列)
    for(uint32_t i = 0; i < kParticleCount_; ++i) {
        instancingData_[i].World = TransformFunctions::MakeIdentity4x4();
        instancingData_[i].WVP = TransformFunctions::MakeIdentity4x4();
    }

    // 2. SRVの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = kParticleCount_;
    srvDesc.Buffer.StructureByteStride = sizeof(TransformMatrix);

    ID3D12DescriptorHeap *srvHeap = TextureManager::GetInstance()->GetSrvDescriptorHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ハンドルの計算 (指定されたindexの場所を使う)
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandleCPU.ptr += descriptorSize * srvIndex;
    srvHandleGPU.ptr += descriptorSize * srvIndex;
    instancingSrvHandleGPU_ = srvHandleGPU;

    device->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, srvHandleCPU);

    // 3. マテリアルの作成
    materialResource_ = CreateBufferResource(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->lightingType = 0;
    materialData_->uvTransform = TransformFunctions::MakeIdentity4x4();
    // materialResource_->Unmap(0, nullptr); // 書き続けるならUnmapしない

    // 4. テクスチャ読み込み
    // コマンドリストが必要なのでParticleCommonから取得するか、TextureManager::Loadのタイミングを考える必要があるが
    // ここでは初期化時にコマンドリストを渡していないため、事前にロード済みであることを前提とするか、
    // InitializeにCommandListを渡すように変更するのが良い。
    // 今回は簡易的にロード処理を呼ぶ (内部でロード済みならハンドルだけ返ってくる)
    textureIndex_ = TextureManager::GetInstance()->Load(textureFilePath, commandList);
}

void Particle::Update(const Matrix4x4 &viewProjection) {
    // 簡易的な更新処理 (例としてInitializeにあったものをここに実装)
    for(uint32_t i = 0; i < kParticleCount_; ++i) {
        Matrix4x4 worldMatrix = TransformFunctions::MakeTranslateMatrix({ 0.0f + 0.1f * i, 0.0f - 0.1f * i, 0.0f });
        Matrix4x4 wvpMatrix = TransformFunctions::Multiply(worldMatrix, viewProjection);

        instancingData_[i].World = worldMatrix;
        instancingData_[i].WVP = wvpMatrix;
    }
}

void Particle::Draw() {
    ID3D12GraphicsCommandList *commandList = particleCommon_->GetCommandList();
    particleCommon_->SetBlendMode(blendMode_);

    // 描画前にParticleCommonへモードを設定
    particleCommon_->SetBlendMode(blendMode_);

    // 頂点バッファセット (Commonが持っている板ポリゴン)
    commandList->IASetVertexBuffers(0, 1, &particleCommon_->GetVertexBufferView());

    // RootParam[0]: Material
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // RootParam[1]: Instancing Data (SRV DescriptorTable)
    commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

    // RootParam[2]: Texture (SRV DescriptorTable)
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGpuHandle(textureIndex_));

    // RootParam[3]: DirectionalLight (使わない場合はダミーを設定するか、シェーダーで無視する)
    // ※Particle.VS.hlslがDirectionalLightを要求する場合、Scene等から渡されたLightのリソースアドレスが必要になります。
    // 今回は簡易化のため省略するか、別途Setする関数を作る必要があります。

    // インスタンシング描画
    commandList->DrawInstanced(particleCommon_->GetVertexCount(), kParticleCount_, 0, 0);
}