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
    *mappedDirectionalLight_ = {};

    pointLightResource_ = CreateCB(sizeof(PointLight));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedPointLight_));
    *mappedPointLight_ = {};

    cameraResource_ = CreateCB(sizeof(CameraForGPU));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedCamera_));
    mappedCamera_->worldPosition = {0.0f, 0.0f, -10.0f};

    // 💡 1. スポットライト群用のリソースを生成
    spotLightResource_ = CreateCB(sizeof(SpotLightGroup));

    // 💡 2. CPUから書き込めるように Map する
    spotLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedSpotLightGroup_));
    *mappedSpotLightGroup_ = {};

    // --- 2. 初期値の設定 ---

    // モデルを正常に表示させるための初期値設定
    mappedMaterial_->color = {1.0f, 1.0f, 1.0f, 1.0f}; // 白・不透明
    mappedMaterial_->lightingType = 1;                 // ライティング有効
    mappedMaterial_->uvTransform = TransformFunctions::MakeIdentity4x4();
    mappedMaterial_->shininess = 50.0f;

    // ライトの初期設定をJSONからロード
    LoadLightingConfig();
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
    std::string path = "resources/json/shared/Lighting/lighting_config.json";
    if (!std::filesystem::exists(path)) {
        path = "resources/json/shared/lighting_config.json";
    }
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;

    try {
        nlohmann::json j;
        ifs >> j;
        
        bool enableDirectional = false;
        bool enablePoint = false;
        bool enableFlatShading = false;
        float dIntensity = 1.0f;
        float pIntensity = 1.0f;

        if (j.contains("enableDirectional")) enableDirectional = j["enableDirectional"];
        if (j.contains("enablePoint")) enablePoint = j["enablePoint"];
        if (j.contains("enableFlatShading")) enableFlatShading = j["enableFlatShading"];
        if (j.contains("dIntensity")) dIntensity = j["dIntensity"];
        if (j.contains("pIntensity")) pIntensity = j["pIntensity"];

        float ambientIntensity = 1.0f;
        if (j.contains("ambientIntensity")) ambientIntensity = j["ambientIntensity"];
        if (mappedSpotLightGroup_) {
            mappedSpotLightGroup_->ambientIntensity = ambientIntensity;
        }

        auto d = GetDirectionalLight();
        if (d) {
            if (j.contains("dLight")) {
                if (j["dLight"].contains("color")) {
                    d->color = {j["dLight"]["color"][0], j["dLight"]["color"][1], j["dLight"]["color"][2], j["dLight"]["color"][3]};
                }
                if (j["dLight"].contains("direction")) {
                    d->direction = TransformFunctions::Normalize(Vector3{j["dLight"]["direction"][0], j["dLight"]["direction"][1], j["dLight"]["direction"][2]});
                }
            }
            d->intensity = enableDirectional ? dIntensity : 0.0f;
            d->enableFlatShading = enableFlatShading ? 1 : 0;
        }
        
        auto p = GetPointLight();
        if (p) {
            if (j.contains("pLight")) {
                if (j["pLight"].contains("color")) p->color = {j["pLight"]["color"][0], j["pLight"]["color"][1], j["pLight"]["color"][2], j["pLight"]["color"][3]};
                if (j["pLight"].contains("position")) p->position = {j["pLight"]["position"][0], j["pLight"]["position"][1], j["pLight"]["position"][2]};
                if (j["pLight"].contains("radius")) p->radius = j["pLight"]["radius"];
                if (j["pLight"].contains("decay")) p->decay = j["pLight"]["decay"];
            }
            p->intensity = enablePoint ? pIntensity : 0.0f;
        }

        if (mappedSpotLightGroup_) {
            for (uint32_t i = 0; i < kMaxSpotLights; ++i) {
                mappedSpotLightGroup_->spotLights[i].enable = 0;
            }
            if (j.contains("spotLights") && j["spotLights"].is_array()) {
                const auto& slArray = j["spotLights"];
                mappedSpotLightGroup_->spotLightCount = static_cast<int32_t>((std::min)(slArray.size(), static_cast<size_t>(kMaxSpotLights)));
                for (int32_t i = 0; i < mappedSpotLightGroup_->spotLightCount; ++i) {
                    const auto& slItem = slArray[i];
                    auto& sl = mappedSpotLightGroup_->spotLights[i];
                    if (slItem.contains("color")) sl.color = {slItem["color"][0], slItem["color"][1], slItem["color"][2], slItem["color"][3]};
                    if (slItem.contains("position")) sl.position = {slItem["position"][0], slItem["position"][1], slItem["position"][2]};
                    if (slItem.contains("direction")) {
                        Vector3 dir = {slItem["direction"][0], slItem["direction"][1], slItem["direction"][2]};
                        sl.direction = TransformFunctions::Normalize(dir);
                    }
                    if (slItem.contains("intensity")) sl.intensity = slItem["intensity"];
                    if (slItem.contains("distance")) sl.distance = slItem["distance"];
                    if (slItem.contains("decay")) sl.decay = slItem["decay"];
                    float angleDeg = slItem.value("angleDeg", 30.0f);
                    float falloffDeg = slItem.value("falloffDeg", 20.0f);
                    sl.cosAngle = std::cos(angleDeg * static_cast<float>(std::numbers::pi) / 180.0f);
                    sl.cosFalloffStart = std::cos(falloffDeg * static_cast<float>(std::numbers::pi) / 180.0f);
                    sl.enable = slItem.value("enabled", true) ? 1 : 0;
                }
            }
        }
    } catch (...) {}
}
