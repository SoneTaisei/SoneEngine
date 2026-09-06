#include "AnimationPreviewScene.h"
#ifdef USE_IMGUI
#include "Graphics/TextureManager.h"
#include "Scene/SceneManager.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Component/TransformComponent.h"
#include "Renderer/Renderer.h"
#include "Core/Utility/Animation.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

void AnimationPreviewScene::Initialize() {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

    // 1. スカイボックスの生成（明るさを抑えてBlender風ダークグレー背景にする）
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device, skyboxTextureHandle_);
    skybox_->SetColor(Vector4{ 0.08f, 0.08f, 0.10f, 1.0f });

    // 2. 床グリッドオブジェクト（グレーの基準床）の生成
    Primitive* boxPrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box);
    if (boxPrim) {
        gridFloorObj_ = std::make_unique<PrimitiveObject>();
        gridFloorObj_->Initialize(device, boxPrim);
        gridFloorObj_->SetName("GridFloor");
        gridFloorObj_->SetTranslation({ 0.0f, -0.05f, 0.0f });
        gridFloorObj_->SetScale({ 40.0f, 0.1f, 40.0f });
        gridFloorObj_->SetIsDoubleSided(true);
        gridFloorObj_->GetMaterial().lightingType = 0; // 均一ライティングでどの角度からも一定の明るさを維持
        gridFloorObj_->GetMaterial().color = { 0.18f, 0.18f, 0.20f, 1.0f };
        gridFloorObj_->GetMaterial().enableBoxMapping = 2.0f; // プロシージャル3D床グリッドを有効化
        gridFloorObj_->Update();
    }

    // 3. アニメーション編集対象モデルのロード（JSONがあればJSONから復元、無ければデフォルト生成）
    if (!LoadHierarchyFromJson()) {
        playerObject_ = CreateDefaultPlayerObject();
        if (playerObject_) {
            gameObjects_.push_back(playerObject_);
            selectedGameObject_ = playerObject_;
            SaveHierarchyToJson();
        }
    }

    // 4. アニメーションエディター専用定数バッファの生成 (256バイトアライン)
    auto CreateCB = [&](size_t size) {
        return CreateBufferResource(device, (size + 255) & ~255u);
    };
    animDirLightResource_ = CreateCB(sizeof(DirectionalLight));
    animDirLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedAnimDirLight_));
    *mappedAnimDirLight_ = {};

    animPointLightResource_ = CreateCB(sizeof(PointLight));
    animPointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedAnimPointLight_));
    *mappedAnimPointLight_ = {};

    animSpotLightResource_ = CreateCB(sizeof(SpotLightGroup));
    animSpotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedAnimSpotLight_));
    *mappedAnimSpotLight_ = {};

    // 5. アニメーションエディター専用ライティング設定のロード
    lightingConfig_.LoadFromFile();
}

void AnimationPreviewScene::OnEnter(SceneManager *sceneManager) {
    if (gridFloorObj_) gridFloorObj_->Update();
    for (auto& obj : gameObjects_) {
        if (obj) obj->Update();
    }
}

void AnimationPreviewScene::OnExit(SceneManager *sceneManager) {
    // シーン離脱時に最新の配置・Transform・マテリアルを自動保存
    SaveHierarchyToJson();
}

void AnimationPreviewScene::Update(SceneManager *sceneManager) {
    if (skybox_) {
        skybox_->Update();
    }
    if (gridFloorObj_) {
        gridFloorObj_->Update();
    }
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void AnimationPreviewScene::UpdateEditor() {
    if (skybox_) {
        skybox_->Update();
    }
    if (gridFloorObj_) {
        gridFloorObj_->Update();
    }
    for (auto& obj : gameObjects_) {
        if (obj) {
            obj->Update();
        }
    }
}

void AnimationPreviewScene::ApplyAnimationLighting() {
    if (!modelCommon_) return;

    // ゲーム本番ライティングモードの場合、カスタムライトを解除してゲーム側のライトをそのまま使用
    if (lightingConfig_.useGameLighting) {
        modelCommon_->ClearCustomLighting();
        return;
    }

    // 水平角・仰角から方向ベクトルを再計算
    lightingConfig_.RecalculateDirection();

    // 明るさスケール（全体の明るさスライダーの反映）
    float scale = (std::max)(0.01f, lightingConfig_.brightness);

    // 1. 専用キーライト (DirectionalLight)
    if (mappedAnimDirLight_) {
        mappedAnimDirLight_->color = lightingConfig_.keyLightColor;
        mappedAnimDirLight_->direction = lightingConfig_.keyLightDirection;
        mappedAnimDirLight_->intensity = lightingConfig_.enableKeyLight ? (lightingConfig_.keyLightIntensity * scale) : 0.0f;
        mappedAnimDirLight_->enableFlatShading = 0; // スムースシェーディングで滑らかな立体感を表現
    }

    // 2. 専用リムライト / フィルライト (PointLight)
    if (mappedAnimPointLight_) {
        mappedAnimPointLight_->color = lightingConfig_.rimLightColor;
        mappedAnimPointLight_->position = lightingConfig_.rimLightPos;
        mappedAnimPointLight_->intensity = lightingConfig_.enableRimLight ? (lightingConfig_.rimLightIntensity * scale) : 0.0f;
        mappedAnimPointLight_->radius = lightingConfig_.rimLightRadius;
        mappedAnimPointLight_->decay = lightingConfig_.rimLightDecay;
    }

    // 3. 専用環境光 & スポットライト (SpotLightGroup)
    if (mappedAnimSpotLight_) {
        mappedAnimSpotLight_->ambientIntensity = lightingConfig_.ambientIntensity * scale;
        mappedAnimSpotLight_->spotLightCount = 0; // ゲーム内のスポットライトが干渉しないよう無効化
        for (uint32_t i = 0; i < kMaxSpotLights; ++i) {
            mappedAnimSpotLight_->spotLights[i].enable = 0;
            mappedAnimSpotLight_->spotLights[i].shadowMapIndex = -1;
        }
    }

    // ModelCommon に専用バッファのアドレスをオーバーライド登録
    D3D12_GPU_VIRTUAL_ADDRESS dirAddr = animDirLightResource_ ? animDirLightResource_->GetGPUVirtualAddress() : 0;
    D3D12_GPU_VIRTUAL_ADDRESS pointAddr = animPointLightResource_ ? animPointLightResource_->GetGPUVirtualAddress() : 0;
    D3D12_GPU_VIRTUAL_ADDRESS spotAddr = animSpotLightResource_ ? animSpotLightResource_->GetGPUVirtualAddress() : 0;
    modelCommon_->SetCustomLighting(dirAddr, pointAddr, spotAddr);
}

void AnimationPreviewScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    if (modelCommon_) {
        ApplyAnimationLighting();
        modelCommon_->PreDraw();
    }

    if (skybox_) {
        skybox_->Draw();
        
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }

    if (gridFloorObj_) {
        gridFloorObj_->Draw();
    }

    // 選択されたオブジェクトのみを描画
    if (selectedGameObject_) {
        selectedGameObject_->Draw();
    } else if (!gameObjects_.empty() && gameObjects_[0]) {
        gameObjects_[0]->Draw();
    }

    Renderer::GetInstance()->RenderComponents();

    // 描画コマンド発行完了後、ModelCommonのカスタムライトアドレスをクリア
    if (modelCommon_) {
        modelCommon_->ClearCustomLighting();
    }
}

void AnimationPreviewScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
}

std::vector<Object3D *> AnimationPreviewScene::GetObjects() {
    return {};
}

std::vector<PrimitiveObject *> AnimationPreviewScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;
    if (gridFloorObj_) {
        result.push_back(gridFloorObj_.get());
    }
    return result;
}

std::shared_ptr<GameObject> AnimationPreviewScene::AddGameObjectFromModel(const std::string& directoryPath, const std::string& fileName, const std::string& displayName) {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    Model* model = ModelManager::GetInstance()->GetModel(directoryPath, fileName);
    if (!model) return nullptr;

    // 重複を避けた一意な名前の決定
    std::string uniqueName = displayName.empty() ? "ModelObject" : displayName;
    int suffix = 1;
    bool nameTaken = true;
    while (nameTaken) {
        nameTaken = false;
        for (const auto& existing : gameObjects_) {
            if (existing && existing->GetName() == uniqueName) {
                nameTaken = true;
                uniqueName = displayName + "_" + std::to_string(suffix++);
                break;
            }
        }
    }

    auto newObj = std::make_shared<GameObject>(uniqueName);
    auto transform = newObj->AddComponent<TransformComponent>();
    transform->SetPosition({ 0.0f, 0.0f, 0.0f });
    transform->SetScale({ 1.0f, 1.0f, 1.0f });
    transform->SetRotation({ 0.0f, 0.0f, 0.0f });

    // テクスチャ設定
    const std::string& texPath = model->GetModelData().material.textureFilePath;
    if (!texPath.empty() && std::filesystem::exists(texPath)) {
        uint32_t texIdx = TextureManager::GetInstance()->Load(texPath);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(texIdx);
        model->SetTextureHandle(gpuHandle);
    }

    auto meshRenderer = newObj->AddComponent<MeshRendererComponent>();
    meshRenderer->Initialize(device, model);
    meshRenderer->SetModelInfo(directoryPath, fileName);
    if (!texPath.empty() && std::filesystem::exists(texPath)) {
        uint32_t texIdx = TextureManager::GetInstance()->Load(texPath);
        meshRenderer->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texIdx));
        meshRenderer->SetTexturePath(texPath);
    }
    meshRenderer->GetMaterial().color = { 0.85f, 0.85f, 0.88f, 1.0f };
    meshRenderer->GetMaterial().lightingType = 1;
    meshRenderer->GetMaterial().shininess = 40.0f;

    auto animator = newObj->AddComponent<AnimatorComponent>();
    animator->Initialize();
    animator->SetModelData(model->GetModelData());
    animator->ClearJointOverrides();

    gameObjects_.push_back(newObj);
    selectedGameObject_ = newObj;
    SaveHierarchyToJson(); // 追加時に自動保存
    return newObj;
}

bool AnimationPreviewScene::RemoveGameObject(const std::shared_ptr<GameObject>& obj) {
    if (!obj) return false;
    auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
    if (it != gameObjects_.end()) {
        if (*it == playerObject_) {
            playerObject_ = nullptr;
        }
        if (*it == selectedGameObject_) {
            selectedGameObject_ = nullptr;
        }
        gameObjects_.erase(it);
        if (!selectedGameObject_ && !gameObjects_.empty()) {
            selectedGameObject_ = gameObjects_[0];
        }
        SaveHierarchyToJson(); // 削除時に自動保存
        return true;
    }
    return false;
}

const char* AnimationPreviewScene::GetDefaultHierarchyJsonPath() {
    return "resources/json/local/animation_hierarchy.json";
}

std::shared_ptr<GameObject> AnimationPreviewScene::CreateDefaultPlayerObject() {
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    Model* playerModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/player", "Player.gltf");
    if (!playerModel) return nullptr;

    auto playerObj = std::make_shared<GameObject>("Player");
    auto playerTransform = playerObj->AddComponent<TransformComponent>();
    playerTransform->SetPosition({ 0.0f, 0.0f, 0.0f });
    playerTransform->SetScale({ 2.0f, 2.0f, 2.0f });
    playerTransform->SetRotation({ 0.0f, 3.14159265f, 0.0f });

    std::string texPath = "resources/Object/Original/player/white.png";
    uint32_t playerTexIndex = TextureManager::GetInstance()->Load(texPath);
    D3D12_GPU_DESCRIPTOR_HANDLE playerTH = TextureManager::GetInstance()->GetGpuHandle(playerTexIndex);

    auto playerRenderer = playerObj->AddComponent<MeshRendererComponent>();
    playerRenderer->Initialize(device, playerModel);
    playerRenderer->SetModelInfo("resources/Object/Original/player", "Player.gltf");
    playerRenderer->SetTexturePath(texPath);
    playerRenderer->SetTextureHandle(playerTH);
    playerModel->SetTextureHandle(playerTH);
    playerRenderer->GetMaterial().color = { 0.85f, 0.85f, 0.88f, 1.0f };
    playerRenderer->GetMaterial().lightingType = 1;
    playerRenderer->GetMaterial().shininess = 40.0f;

    auto animator = playerObj->AddComponent<AnimatorComponent>();
    animator->Initialize();
    animator->SetModelData(playerModel->GetModelData());
    animator->ClearJointOverrides();

    return playerObj;
}

bool AnimationPreviewScene::SaveHierarchyToJson(const std::string& filePath) {
    std::string path = filePath.empty() ? GetDefaultHierarchyJsonPath() : filePath;
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json root = nlohmann::json::array();
        for (const auto& obj : gameObjects_) {
            if (!obj) continue;
            nlohmann::json item;
            item["name"] = obj->GetName();
            item["isPlayer"] = (obj == playerObject_ || obj->GetName() == "Player");

            auto* renderer = obj->GetComponent<MeshRendererComponent>();
            if (renderer) {
                item["modelDirectory"] = renderer->GetModelDirectory();
                item["modelFileName"] = renderer->GetModelFileName();
                item["texturePath"] = renderer->GetTexturePath();

                const auto& mat = renderer->GetMaterial();
                item["material"]["color"] = { mat.color.x, mat.color.y, mat.color.z, mat.color.w };
                item["material"]["lightingType"] = mat.lightingType;
                item["material"]["shininess"] = mat.shininess;
                item["material"]["isDoubleSided"] = renderer->IsDoubleSided();
            } else {
                item["modelDirectory"] = "";
                item["modelFileName"] = "";
                item["texturePath"] = "";
            }

            auto* transform = obj->GetComponent<TransformComponent>();
            if (transform) {
                const auto& pos = transform->GetPosition();
                const auto& rot = transform->GetRotation();
                const auto& scale = transform->GetScale();
                item["translation"] = { pos.x, pos.y, pos.z };
                item["rotation"] = { rot.x, rot.y, rot.z };
                item["scale"] = { scale.x, scale.y, scale.z };
            } else {
                item["translation"] = { 0.0f, 0.0f, 0.0f };
                item["rotation"] = { 0.0f, 0.0f, 0.0f };
                item["scale"] = { 1.0f, 1.0f, 1.0f };
            }

            root.push_back(item);
        }

        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        ofs << root.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

bool AnimationPreviewScene::LoadHierarchyFromJson(const std::string& filePath) {
    std::string path = filePath.empty() ? GetDefaultHierarchyJsonPath() : filePath;
    if (!std::filesystem::exists(path)) return false;

    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;
        nlohmann::json root = nlohmann::json::parse(ifs);
        if (!root.is_array() || root.empty()) return false;

        ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
        if (!device) return false;

        gameObjects_.clear();
        playerObject_ = nullptr;
        selectedGameObject_ = nullptr;

        for (const auto& item : root) {
            std::string name = item.value("name", "ModelObject");
            std::string dir = item.value("modelDirectory", "");
            std::string file = item.value("modelFileName", "");
            std::string tex = item.value("texturePath", "");
            bool isPlayer = item.value("isPlayer", false);

            if (dir.empty() || file.empty()) {
                if (isPlayer) {
                    dir = "resources/Object/Original/player";
                    file = "Player.gltf";
                } else {
                    continue;
                }
            }

            Model* model = ModelManager::GetInstance()->GetModel(dir, file);
            if (!model) continue;

            auto newObj = std::make_shared<GameObject>(name);
            auto transform = newObj->AddComponent<TransformComponent>();

            if (item.contains("translation") && item["translation"].is_array() && item["translation"].size() == 3) {
                transform->SetPosition({ item["translation"][0], item["translation"][1], item["translation"][2] });
            }
            if (item.contains("rotation") && item["rotation"].is_array() && item["rotation"].size() == 3) {
                transform->SetRotation({ item["rotation"][0], item["rotation"][1], item["rotation"][2] });
            }
            if (item.contains("scale") && item["scale"].is_array() && item["scale"].size() == 3) {
                transform->SetScale({ item["scale"][0], item["scale"][1], item["scale"][2] });
            }

            // テクスチャ設定
            std::string effectiveTex = tex;
            if (effectiveTex.empty() || !std::filesystem::exists(effectiveTex)) {
                effectiveTex = model->GetModelData().material.textureFilePath;
            }
            if (!effectiveTex.empty() && std::filesystem::exists(effectiveTex)) {
                uint32_t texIdx = TextureManager::GetInstance()->Load(effectiveTex);
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(texIdx);
                model->SetTextureHandle(gpuHandle);
            }

            auto meshRenderer = newObj->AddComponent<MeshRendererComponent>();
            meshRenderer->Initialize(device, model);
            meshRenderer->SetModelInfo(dir, file);
            meshRenderer->SetTexturePath(effectiveTex);

            if (!effectiveTex.empty() && std::filesystem::exists(effectiveTex)) {
                uint32_t texIdx = TextureManager::GetInstance()->Load(effectiveTex);
                meshRenderer->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(texIdx));
            }

            // マテリアル復元
            if (item.contains("material")) {
                const auto& m = item["material"];
                if (m.contains("color") && m["color"].is_array() && m["color"].size() == 4) {
                    meshRenderer->GetMaterial().color = { m["color"][0], m["color"][1], m["color"][2], m["color"][3] };
                }
                if (m.contains("lightingType")) {
                    meshRenderer->GetMaterial().lightingType = m["lightingType"].get<int>();
                }
                if (m.contains("shininess")) {
                    meshRenderer->GetMaterial().shininess = m["shininess"].get<float>();
                }
                if (m.contains("isDoubleSided")) {
                    meshRenderer->SetIsDoubleSided(m["isDoubleSided"].get<bool>());
                }
            }

            auto animator = newObj->AddComponent<AnimatorComponent>();
            animator->Initialize();
            animator->SetModelData(model->GetModelData());
            animator->ClearJointOverrides();

            gameObjects_.push_back(newObj);
            if (isPlayer) {
                playerObject_ = newObj;
            }
        }

        if (!gameObjects_.empty()) {
            selectedGameObject_ = playerObject_ ? playerObject_ : gameObjects_[0];
        }
        return true;
    } catch (...) {
        return false;
    }
}

void AnimationPreviewScene::ResetHierarchyToDefault() {
    gameObjects_.clear();
    playerObject_ = CreateDefaultPlayerObject();
    if (playerObject_) {
        gameObjects_.push_back(playerObject_);
        selectedGameObject_ = playerObject_;
    }
    SaveHierarchyToJson();
}
#endif
