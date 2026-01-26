#include "ModelCommon.h"
#include "Model.h"
#include <cassert>
#include "Graphics/TextureManager.h"
#include <numbers> // 数学定数用 (C++20以上)
#include <cmath>   // std::cos, std::sin用

void ModelCommon::Initialize(ID3D12Device *device) {
    assert(device);
    device_ = device;

    // 定数バッファ作成の共通処理（256バイトアライメント）
    auto CreateCB = [&](size_t size) {
        return CreateBufferResource(device_, (size + 255) & ~255u);
    };

    // --- 1. リソースの生成とMap ---

    // マテリアルリソースの作成（これが抜けていました）
    materialResource_ = CreateCB(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));

    // ライトとカメラのリソース（既存の処理）
    directionalLightResource_ = CreateCB(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedDirectionalLight_));

    pointLightResource_ = CreateCB(sizeof(PointLight));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedPointLight_));

    cameraResource_ = CreateCB(sizeof(CameraForGPU));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedCamera_));

    // 💡 1. スポットライト用のリソースを生成
    spotLightResource_ = CreateCB(sizeof(SpotLight));

    // 💡 2. CPUから書き込めるように Map する（これで mappedSpotLight_ が Null じゃなくなります）
    spotLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedSpotLight_));

    // --- 2. 初期値の設定 ---

    // モデルを正常に表示させるための初期値設定
    mappedMaterial_->color = {1.0f, 1.0f, 1.0f, 1.0f}; // 白・不透明
    mappedMaterial_->lightingType = 1;                 // ライティング有効
    mappedMaterial_->uvTransform = TransformFunctions::MakeIdentity4x4();
    mappedMaterial_->shininess = 50.0f;

    // ライトとカメラの初期値
    *mappedDirectionalLight_ = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 0.0f};
    *mappedPointLight_ = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}, 1.0f, 10.0f, 1.0f};
    *mappedCamera_ = {{0.0f, 0.0f, -10.0f}};

    // 💡 資料通りの設定値に更新
    mappedSpotLight_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    mappedSpotLight_->position = {2.0f, 1.25f, 0.0f};
    mappedSpotLight_->distance = 7.0f;

    // 💡 向きは正規化(Normalize)を忘れずに！
    Vector3 rawDir = {-1.0f, -1.0f, 0.0f};
    mappedSpotLight_->direction = TransformFunctions::Normalize(rawDir);

    mappedSpotLight_->intensity = 4.0f;
    mappedSpotLight_->decay = 2.0f;

    // 💡 π/3 (60度) のコサインを設定
    mappedSpotLight_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
}

void ModelCommon::PreDraw(ID3D12GraphicsCommandList *commandList) {
    assert(commandList);
    commandList_ = commandList;

    // ヒープとトポロジの設定
    ID3D12DescriptorHeap *descriptorHeaps[] = {TextureManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- ルートパラメータのセット (DirectXCommon.cppの定義に準拠) ---

    // 0: Material (register b0) ★ここが抜けていたので追加！
    commandList_->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // 3: Camera (register b3)
    commandList_->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());

    // 4: DirectionalLight (register b1)
    commandList_->SetGraphicsRootConstantBufferView(4, directionalLightResource_->GetGPUVirtualAddress());

    // 5: PointLight (register b2)
    commandList_->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());

    commandList_->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
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
