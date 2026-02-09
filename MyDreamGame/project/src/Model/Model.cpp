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

    // Commonに自分を登録
    modelCommon_->AddModel(this);

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

    // --- 3. Transform Buffer (変形行列用) の作成 ---
    // ★重要：サイズを256バイトの倍数にする魔法の計算
    transformResource_ = CreateBufferResource(device, (sizeof(TransformMatrix) + 255) & ~255u);

    // バッファを開いて、書き込み用のポインタを取得する
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    // 安全のため、最初は単位行列を入れておく
    mappedTransform_->WVP = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->World = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->WorldInverseTranspose = TransformFunctions::MakeIdentity4x4();
}

void Model::Draw(const Matrix4x4 &viewProjectionMatrix) {
    ID3D12GraphicsCommandList *commandList = modelCommon_->GetCommandList();
    assert(commandList);

    // 1. オブジェクト自身の変形行列（Scale/Rotate/Translate）を作成
    Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    // 2. 資料に基づき、Assimpで読み込んだノード行列を適用する (★ここが重要！)
    // 最終的なワールド行列 = ルートノードの行列 * オブジェクトのワールド行列
    Matrix4x4 finalWorldMatrix = modelData_.rootNode.localMatrix * worldMatrix; //

    // 3. 書き込み用のバッファを更新
    if (mappedTransform_) {
        // WVP = 最終ワールド行列 * ビュープロジェクション行列
        mappedTransform_->WVP = TransformFunctions::Multiply(finalWorldMatrix, viewProjectionMatrix); //
        mappedTransform_->World = finalWorldMatrix;                                                   //

        // ライティング用の逆転置行列も、finalWorldMatrix を元に計算する
        DirectX::XMMATRIX worldX = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4 *>(&finalWorldMatrix));
        DirectX::XMMATRIX worldInv = DirectX::XMMatrixInverse(nullptr, worldX);
        DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4 *>(&mappedTransform_->WorldInverseTranspose), worldInv);
    }

    // --- 以下、描画コマンドの発行 ---
    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, textureHandle_);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(UINT(modelData_.indices.size()), 1, 0, 0, 0);
}