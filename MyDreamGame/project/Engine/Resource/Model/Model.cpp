#include "Model.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Graphics/TextureManager.h"
#include <cassert>
#include <filesystem>

// 必要に応じてextern宣言など
extern ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename);
extern void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, uint32_t latDiv, uint32_t lonDiv);

Model::~Model() {
    // ★ 破棄されるときにリストから自分を削除
    if(modelCommon_) {
        modelCommon_->RemoveModel(this);
    }
}

void Model::Initialize(ModelCommon *modelCommon, const std::string &directoryPath, const std::string &filename) {
    // 1. Commonをセット
    modelCommon_ = modelCommon;

    // 2. データ読み込み
    modelData_ = LoadModelFile(directoryPath, filename);

    // モデル自体のマテリアルにテクスチャファイルが存在する場合のみ TextureManager からロードして GPU デスクリプタをセット
    if (!modelData_.material.textureFilePath.empty() && std::filesystem::exists(modelData_.material.textureFilePath)) {
        uint32_t texIndex = TextureManager::GetInstance()->Load(modelData_.material.textureFilePath);
        textureHandle_ = TextureManager::GetInstance()->GetGpuHandle(texIndex);
    }

    // 3. バッファ生成
    CreateBuffers();
}

// ★球体用初期化
void Model::InitializeSphere(ModelCommon *modelCommon) {
    // 1. Commonをセット
    modelCommon_ = modelCommon;

    // Commonに自分を登録
    modelCommon_->AddModel(this);

    // 2. 球体メッシュ生成
    CreateSphereMesh(modelData_.vertices, modelData_.indices, 1.0f, 32, 32);

    // 3. バッファ生成
    CreateBuffers();
}

void Model::CreateBuffers() {
    if (!modelCommon_) return;
    ID3D12Device *device = modelCommon_->GetDevice();
    if (!device) return;

    if (modelData_.vertices.empty()) {
        // メッシュのないファイルでも安全にダミーバッファを作成
        VertexData dummy{};
        dummy.position = { 0.0f, 0.0f, 0.0f, 1.0f };
        dummy.normal = { 0.0f, 1.0f, 0.0f };
        dummy.texcoord = { 0.0f, 0.0f };
        dummy.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        modelData_.vertices.push_back(dummy);
    }

    HRESULT hr;

    // --- 1. Vertex Buffer (頂点バッファ) の作成 ---
    vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData *vertexData = nullptr;
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
    if (SUCCEEDED(hr) && vertexData) {
        std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
        vertexResource_->Unmap(0, nullptr);
    }

    // --- 2. Index Buffer (インデックスバッファ) の作成 ---
    size_t indexCount = modelData_.indices.empty() ? 1 : modelData_.indices.size();
    indexResource_ = CreateBufferResource(device, sizeof(uint32_t) * indexCount);
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indexCount);
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t *indexData = nullptr;
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
    if (SUCCEEDED(hr) && indexData) {
        if (!modelData_.indices.empty()) {
            std::memcpy(indexData, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
        } else {
            indexData[0] = 0;
        }
        indexResource_->Unmap(0, nullptr);
    }
}

void Model::Draw(const D3D12_VERTEX_BUFFER_VIEW* weightBufferView, D3D12_GPU_DESCRIPTOR_HANDLE overrideTexture) {
    if (modelData_.indices.empty()) return;

    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    if (!commandList) return;

    D3D12_GPU_DESCRIPTOR_HANDLE activeTexture = overrideTexture;
    if (activeTexture.ptr == 0) {
        activeTexture = textureHandle_;
    }
    if (activeTexture.ptr == 0) {
        uint32_t defaultWhite = TextureManager::GetInstance()->Load("white");
        activeTexture = TextureManager::GetInstance()->GetGpuHandle(defaultWhite);
    }

    commandList->SetGraphicsRootDescriptorTable(2, activeTexture);
    
    if (weightBufferView != nullptr) {
        D3D12_VERTEX_BUFFER_VIEW views[] = { vertexBufferView_, *weightBufferView };
        commandList->IASetVertexBuffers(0, 2, views);
    } else {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    }
    
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
}