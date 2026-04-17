#include "Skybox.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Graphics/TextureManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h" // GetInstance()を使うために必要！

void Skybox::Initialize(ID3D12Device *device, uint32_t textureHandle) {
    textureHandle_ = textureHandle;

    // 1. 頂点データとインデックスデータの生成
    std::vector<SkyboxVertexData> vertices;
    std::vector<uint32_t> indices;
    CreateBoxMesh(vertices, indices); // UtilityFunctions等に実装済みの箱生成関数
    indexCount_ = static_cast<uint32_t>(indices.size());

    // 2. 頂点バッファの作成とデータ転送
    vertexBuffer_ = CreateBufferResource(device, sizeof(SkyboxVertexData) * vertices.size());
    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = static_cast<UINT>(sizeof(SkyboxVertexData) * vertices.size());
    vbView_.StrideInBytes = sizeof(SkyboxVertexData);

    SkyboxVertexData *vertexData = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
    std::memcpy(vertexData, vertices.data(), sizeof(SkyboxVertexData) * vertices.size());
    vertexBuffer_->Unmap(0, nullptr);

    // 3. インデックスバッファの作成とデータ転送
    indexBuffer_ = CreateBufferResource(device, sizeof(uint32_t) * indices.size());
    ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    ibView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t *indexData = nullptr;
    indexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
    std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
    indexBuffer_->Unmap(0, nullptr);

    // 4. 定数バッファの作成 (256バイトアライメントを適用！)
    transformBuffer_ = CreateBufferResource(device, (sizeof(TransformationMatrix) + 255) & ~255u);
    transformBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));
    // 初期値として単位行列を入れておく
    mappedTransform_->WVP = TransformFunctions::MakeIdentity4x4();
    mappedTransform_->World = TransformFunctions::MakeIdentity4x4();

    materialBuffer_ = CreateBufferResource(device, (sizeof(Material) + 255) & ~255u);
    materialBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
    // 色は白（テクスチャの色をそのまま出す）
    mappedMaterial_->color = {1.0f, 1.0f, 1.0f, 1.0f};
}

void Skybox::Update(const Vector3 &cameraPos, const Matrix4x4 &viewMatrix, const Matrix4x4 &projectionMatrix) {
    // ワールド行列の位置を、常にカメラの位置に追従させる
    Matrix4x4 worldMatrix = TransformFunctions::MakeTranslateMatrix(cameraPos);

    // WVP行列の計算
    Matrix4x4 wvpMatrix = TransformFunctions::Multiply(worldMatrix, TransformFunctions::Multiply(viewMatrix, projectionMatrix));

    // バッファに書き込み
    mappedTransform_->World = worldMatrix;
    mappedTransform_->WVP = wvpMatrix;
}

void Skybox::Draw(ID3D12GraphicsCommandList *commandList) {
    // 1. DirectXCommonから専用のルール（PSO・RootSignature）を取得してセット
    DirectXCommon *dxCommon = DirectXCommon::GetInstance();
    commandList->SetGraphicsRootSignature(dxCommon->GetSkyboxRootSignature());
    commandList->SetPipelineState(dxCommon->GetSkyboxPipelineState());

    // 2. 頂点とインデックスをセット
    commandList->IASetVertexBuffers(0, 1, &vbView_);
    commandList->IASetIndexBuffer(&ibView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 3. 定数バッファをセット（シェーダーの register(b0), register(b1) に対応）
    commandList->SetGraphicsRootConstantBufferView(0, transformBuffer_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, materialBuffer_->GetGPUVirtualAddress());

    // 4. テクスチャ(CubeMap)のSRVをセット（t0 に対応）
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(2, srvHandle);

    // 5. 描画！（インデックス描画）
    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}