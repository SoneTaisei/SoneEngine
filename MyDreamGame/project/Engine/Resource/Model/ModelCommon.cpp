#include "ModelCommon.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Model.h"
#include <cassert>
#include "Graphics/TextureManager.h"
#include "Renderer/SrvManager.h"
#include <numbers> // 数学定数用 (C++20以上)
#include <cmath>   // std::cos, std::sin用
#include <fstream>
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <format>

void ModelCommon::Initialize(ID3D12Device *device) {
    assert(device);
    device_ = device;

    // 定数バッファ作成の共通処理（256バイトアライメント）
    auto CreateCB = [&](size_t size) {
        return CreateBufferResource(device_, (size + 255) & ~255u);
    };

    // ▼ マテリアルリソースの作成と初期化
    materialResource_ = CreateCB(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));

    // ★ ここに初期値をセット！ (materialData を mappedMaterial_ に変更)
    mappedMaterial_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    mappedMaterial_->lightingType = 1;
    mappedMaterial_->uvTransform = TransformFunctions::MakeIdentity4x4();
    mappedMaterial_->shininess = 50.0f;

    // ▼ 平行光源（DirectionalLight）のリソースの作成と初期化
    directionalLightResource_ = CreateCB(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedDirectionalLight_));

    // ★ ここに初期値をセット！ (directionalLightData_ を mappedDirectionalLight_ に変更)
    mappedDirectionalLight_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    mappedDirectionalLight_->direction = {0.0f, -1.0f, 0.0f};
    mappedDirectionalLight_->intensity = 1.0f;

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
    mappedCamera_->worldPosition = {0.0f, 0.0f, -10.0f};

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

void ModelCommon::PreDraw() {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    assert(commandList);
    commandList_ = commandList;

    // ★ Skybox等の後でも安全に3Dモデルが描画できるよう、RootSignatureを明示的にセット
    auto* rootSig = DirectXCommon::GetInstance()->GetRootSignature();
    commandList_->SetGraphicsRootSignature(rootSig);

    // ヒープとトポロジの設定
    ID3D12DescriptorHeap *descriptorHeaps[] = {SrvManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- ルートパラメータのセット (GPUAddressが0でない安全な場合のみセット) ---

    // 0: Material (register b0)
    if (materialResource_) {
        D3D12_GPU_VIRTUAL_ADDRESS addr = materialResource_->GetGPUVirtualAddress();
        if (addr != 0) {
            commandList_->SetGraphicsRootConstantBufferView(0, addr);
        }
    }

    // 3: Camera (register b3)
    if (cameraResource_) {
        D3D12_GPU_VIRTUAL_ADDRESS addr = cameraResource_->GetGPUVirtualAddress();
        if (addr != 0) {
            commandList_->SetGraphicsRootConstantBufferView(3, addr);
        }
    }

    // 4: DirectionalLight (register b1)
    if (directionalLightResource_) {
        D3D12_GPU_VIRTUAL_ADDRESS addr = directionalLightResource_->GetGPUVirtualAddress();
        if (addr != 0) {
            commandList_->SetGraphicsRootConstantBufferView(4, addr);
        }
    }

    // 5: PointLight (register b2)
    if (pointLightResource_) {
        D3D12_GPU_VIRTUAL_ADDRESS addr = pointLightResource_->GetGPUVirtualAddress();
        if (addr != 0) {
            commandList_->SetGraphicsRootConstantBufferView(5, addr);
        }
    }

    // 6: SpotLight (register b3)
    if (spotLightResource_) {
        D3D12_GPU_VIRTUAL_ADDRESS addr = spotLightResource_->GetGPUVirtualAddress();
        if (addr != 0) {
            commandList_->SetGraphicsRootConstantBufferView(6, addr);
        }
    }
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
        model->Draw();
    }
}

void ModelCommon::LoadLightingConfig() {
    std::ifstream ifs("resources/json/shared/lighting_config.json");
    if (!ifs.is_open()) return;

    try {
        nlohmann::json j;
        ifs >> j;
        
        int activeLightType = 2; // デフォルト：スポットライト
        bool enableFog = false;
        bool enableFlatShading = false;
        float dIntensity = 1.0f;
        float pIntensity = 1.0f;
        float sIntensity = 4.0f;
        float spotAngleDeg = 30.0f;
        float spotFalloffDeg = 20.0f;

        if (j.contains("activeLightType")) activeLightType = j["activeLightType"];
        if (j.contains("enableFog")) enableFog = j["enableFog"];
        if (j.contains("enableFlatShading")) enableFlatShading = j["enableFlatShading"];
        if (j.contains("dIntensity")) dIntensity = j["dIntensity"];
        if (j.contains("pIntensity")) pIntensity = j["pIntensity"];
        if (j.contains("sIntensity")) sIntensity = j["sIntensity"];
        if (j.contains("spotAngleDeg")) spotAngleDeg = j["spotAngleDeg"];
        if (j.contains("spotFalloffDeg")) spotFalloffDeg = j["spotFalloffDeg"];

        auto d = GetDirectionalLight();
        if (d && j.contains("dLight")) {
            if (j["dLight"].contains("color")) {
                d->color = {j["dLight"]["color"][0], j["dLight"]["color"][1], j["dLight"]["color"][2], j["dLight"]["color"][3]};
            }
            if (j["dLight"].contains("direction")) {
                d->direction = {j["dLight"]["direction"][0], j["dLight"]["direction"][1], j["dLight"]["direction"][2]};
            }
        }
        if (d) d->enableFlatShading = enableFlatShading ? 1 : 0;
        
        auto p = GetPointLight();
        if (p && j.contains("pLight")) {
            if (j["pLight"].contains("color")) p->color = {j["pLight"]["color"][0], j["pLight"]["color"][1], j["pLight"]["color"][2], j["pLight"]["color"][3]};
            if (j["pLight"].contains("position")) p->position = {j["pLight"]["position"][0], j["pLight"]["position"][1], j["pLight"]["position"][2]};
            if (j["pLight"].contains("radius")) p->radius = j["pLight"]["radius"];
            if (j["pLight"].contains("decay")) p->decay = j["pLight"]["decay"];
        }

        auto s = GetSpotLight();
        if (s && j.contains("sLight")) {
            if (j["sLight"].contains("color")) s->color = {j["sLight"]["color"][0], j["sLight"]["color"][1], j["sLight"]["color"][2], j["sLight"]["color"][3]};
            if (j["sLight"].contains("position")) s->position = {j["sLight"]["position"][0], j["sLight"]["position"][1], j["sLight"]["position"][2]};
            if (j["sLight"].contains("direction")) s->direction = {j["sLight"]["direction"][0], j["sLight"]["direction"][1], j["sLight"]["direction"][2]};
            if (j["sLight"].contains("distance")) s->distance = j["sLight"]["distance"];
            if (j["sLight"].contains("decay")) s->decay = j["sLight"]["decay"];
        }
        
        // intensity の反映
        if (activeLightType == 0) {
            if (d) d->intensity = dIntensity;
            if (p) p->intensity = 0.0f;
            if (s) s->intensity = 0.0f;
        } else if (activeLightType == 1) {
            if (d) d->intensity = 0.0f;
            if (p) p->intensity = pIntensity;
            if (s) s->intensity = 0.0f;
        } else if (activeLightType == 2) {
            if (d) d->intensity = 0.0f;
            if (p) p->intensity = 0.0f;
            if (s) {
                s->intensity = sIntensity;
                s->cosAngle = std::cos(spotAngleDeg * static_cast<float>(std::numbers::pi) / 180.0f);
                s->cosFalloffStart = std::cos(spotFalloffDeg * static_cast<float>(std::numbers::pi) / 180.0f);
            }
        }
    } catch (...) {}
}
