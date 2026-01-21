#include "ModelCommon.h"
#include "Model.h"
#include <cassert>
#include "Graphics/TextureManager.h"

void ModelCommon::Initialize(ID3D12Device *device) {
    assert(device);
    device_ = device;

    // ヘルパー関数（CreateBufferResource）を使ってリソースを作成
    // 256バイトアライメントを忘れずに！
    directionalLightResource_ = CreateBufferResource(device_, (sizeof(DirectionalLight) + 255) & ~255u);
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedDirectionalLight_));

    pointLightResource_ = CreateBufferResource(device_, (sizeof(PointLight) + 255) & ~255u);
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedPointLight_));

    cameraResource_ = CreateBufferResource(device_, (sizeof(CameraForGPU) + 255) & ~255u);
    cameraResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedCamera_));

    // 初期値を入れておく
    *mappedDirectionalLight_ = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 1.0f};
    *mappedPointLight_ = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}, 1.0f, 10.0f, 1.0f};
    *mappedCamera_ = {{0.0f, 0.0f, -10.0f}};
}

void ModelCommon::PreDraw(ID3D12GraphicsCommandList *commandList) {
    assert(commandList);
    commandList_ = commandList;

    // デスクリプタヒープの設定などはそのまま
    ID3D12DescriptorHeap *descriptorHeaps[] = {TextureManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- 重要：ルートパラメータにバッファをセット ---
    // DirectXCommon.cpp で定義したスロット番号に合わせます

    // 3: Camera (register b3)
    commandList_->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());

    // 4: DirectionalLight (register b1)
    commandList_->SetGraphicsRootConstantBufferView(4, directionalLightResource_->GetGPUVirtualAddress());

    // 5: PointLight (register b2)
    commandList_->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());

    //// ★重要修正: テクスチャを使用するための「場所（ヒープ）」をGPUに教える
    //// これがないと SetGraphicsRootDescriptorTable で必ずクラッシュします
    //ID3D12DescriptorHeap *descriptorHeaps[] = { TextureManager::GetInstance()->GetSrvDescriptorHeap() };
    //commandList_->SetDescriptorHeaps(1, descriptorHeaps);

    //// ★推奨: 描画形状（三角形リスト）を指定しておく
    //// これを忘れると、前の描画設定（線など）が残っていた場合に表示がおかしくなります
    //commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ModelCommon::AddModel(Model *model) {
    models_.push_back(model);
}

void ModelCommon::RemoveModel(Model *model) {
    models_.remove(model);
}

void ModelCommon::DrawAll(const Matrix4x4 &viewProjectionMatrix) {
    for(Model *model : models_) {
        // モデル自身のDrawを呼ぶ（引数にはVP行列だけ渡す）
        model->Draw(viewProjectionMatrix);
    }
}
