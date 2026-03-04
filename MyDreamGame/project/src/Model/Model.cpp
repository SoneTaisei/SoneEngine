#include "Model.h"
#include <cassert>

// 必要に応じてextern宣言など
extern ModelData LoadModelFile(const std::string &directoryPath, const std::string &filename);
extern void CreateSphereMesh(std::vector<VertexData> &vertices, std::vector<uint32_t> &indices, float radius, uint32_t latDiv, uint32_t lonDiv);

Model::~Model() {
    // ★ 破棄されるときにリストから自分を削除
    if(modelCommon_) {
        modelCommon_->RemoveModel(this);
    }
}

// ★OBJ読み込み用初期化
void Model::Initialize(ModelCommon *modelCommon, const std::string &directoryPath, const std::string &filename) {
    // 1. Commonをセット
    modelCommon_ = modelCommon;

    // 2. データ読み込み
    modelData_ = LoadModelFile(directoryPath, filename);

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
    // ModelCommonからDeviceを取得
    ID3D12Device *device = modelCommon_->GetDevice();
    assert(device); // デバイスが正しく取得できているかチェック

    HRESULT hr;

    // --- 1. Vertex Buffer (頂点バッファ) の作成 ---
    // リソースを作成
    vertexResource_ = CreateBufferResource(device, sizeof(VertexData) * modelData_.vertices.size());

    // ビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // データを書き込む
    VertexData *vertexData = nullptr;
    hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
    assert(SUCCEEDED(hr));
    std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
    vertexResource_->Unmap(0, nullptr);


    // --- 2. Index Buffer (インデックスバッファ) の作成 ---
    // リソースを作成
    indexResource_ = CreateBufferResource(device, sizeof(uint32_t) * modelData_.indices.size());

    // ビューの設定
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * modelData_.indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // データを書き込む
    uint32_t *indexData = nullptr;
    hr = indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
    assert(SUCCEEDED(hr));
    std::memcpy(indexData, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
    indexResource_->Unmap(0, nullptr);
}

void Model::Draw() {
    ID3D12GraphicsCommandList *commandList = modelCommon_->GetCommandList();

    // 行列のセット（SetGraphicsRootConstantBufferView）は、
    // Object3D側で呼ぶようにするか、引数でアドレスを受け取る形にします。

    // テクスチャと頂点データのセット
    commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // 描画実行
    commandList->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
}