#include "WorldGif.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/TransformFunctions.h"
#include <fstream>
#include <filesystem>
#include <algorithm>

// ============================================================================
// WorldGifObject 実装
// ============================================================================
WorldGifObject::WorldGifObject() {
}

bool WorldGifObject::Initialize(ID3D12Device* device, const WorldGifConfig& config) {
    if (!device) return false;

    config_ = config;
    if (config_.totalFrames <= 1 && (config_.columns * config_.rows) > 1) {
        config_.totalFrames = config_.columns * config_.rows;
    }
    currentFrame_ = 0;
    animTimer_ = 0.0f;
    isPlaying_ = true;

    // 板ポリ（Plane）プリミティブを取得
    auto* planePrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Plane);
    if (!planePrim) return false;

    primitiveObj_ = std::make_unique<PrimitiveObject>();
    primitiveObj_->Initialize(device, planePrim);
    primitiveObj_->SetName(config_.name);
    primitiveObj_->SetIsDoubleSided(config_.isDoubleSided);
    primitiveObj_->GetMaterial().lightingType = 0; // アニメーション画像本来の色を出すためUnlit設定

    // テクスチャ読み込み
    if (!config_.texturePath.empty()) {
        textureHandle_ = TextureManager::GetInstance()->Load(config_.texturePath);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(textureHandle_);
        primitiveObj_->SetTextureHandle(gpuHandle);
    }

    UpdateMaterialAndTransform();
    return true;
}

void WorldGifObject::SetConfig(const WorldGifConfig& config, ID3D12Device* device) {
    bool textureChanged = (config_.texturePath != config.texturePath);
    config_ = config;
    if (config_.totalFrames <= 1 && (config_.columns * config_.rows) > 1) {
        config_.totalFrames = config_.columns * config_.rows;
    }

    if (textureChanged && primitiveObj_) {
        textureHandle_ = TextureManager::GetInstance()->Load(config_.texturePath);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = TextureManager::GetInstance()->GetGpuHandle(textureHandle_);
        primitiveObj_->SetTextureHandle(gpuHandle);
    }

    if (currentFrame_ >= config_.totalFrames) {
        currentFrame_ = 0;
    }

    UpdateMaterialAndTransform();
}

void WorldGifObject::Update(float deltaTime) {
    int effectiveTotalFrames = config_.totalFrames;
    if (effectiveTotalFrames <= 1 && (config_.columns * config_.rows) > 1) {
        effectiveTotalFrames = config_.columns * config_.rows;
    }

    if (isPlaying_ && effectiveTotalFrames > 1 && config_.fps > 0.0f) {
        float frameDuration = 1.0f / config_.fps;
        animTimer_ += deltaTime;

        while (animTimer_ >= frameDuration) {
            animTimer_ -= frameDuration;
            int next = currentFrame_ + 1;
            if (next >= effectiveTotalFrames) {
                if (config_.isLoop) {
                    next = 0;
                } else {
                    next = effectiveTotalFrames - 1;
                    isPlaying_ = false;
                    break;
                }
            }
            currentFrame_ = next;
        }
    }

    UpdateMaterialAndTransform();
}

void WorldGifObject::UpdateMaterialAndTransform() {
    if (!primitiveObj_) return;

    int cols = (config_.columns > 0) ? config_.columns : 1;
    int rows = (config_.rows > 0) ? config_.rows : 1;

    float frameW = 1.0f / static_cast<float>(cols);
    float frameH = 1.0f / static_cast<float>(rows);

    int col = currentFrame_ % cols;
    int row = currentFrame_ / cols;

    float u = static_cast<float>(col) * frameW;
    float v = static_cast<float>(row) * frameH;

    // UV切り抜き行列を設定
    primitiveObj_->GetMaterial().uvTransform = TransformFunctions::MakeAffineMatrix(
        { frameW, frameH, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { u, v, 0.0f }
    );

    primitiveObj_->SetTranslation(config_.translation);
    primitiveObj_->SetRotation(config_.rotation);

    Vector3 actualScale = config_.scale;
    // PrimitivePlaneはローカルXZ平面(Y=0)のため、Xが横幅、Zが縦幅(起立時Y軸)となる。
    // ユーザーが設定したscale.y(縦幅)をscale.zにも連動反映させ、直感的なサイズ変化を保証
    if (actualScale.z == 1.0f && actualScale.y != 1.0f) {
        actualScale.z = actualScale.y;
    }
    primitiveObj_->SetScale(actualScale);
    primitiveObj_->SetIsBillboard(config_.isBillboard);
    primitiveObj_->SetIsDoubleSided(config_.isDoubleSided);
    primitiveObj_->GetMaterial().color = config_.color;

    primitiveObj_->Update();
}

void WorldGifObject::Draw() {
    if (primitiveObj_) {
        primitiveObj_->Draw();
    }
}

nlohmann::json WorldGifObject::ToJson() const {
    nlohmann::json j;
    j["name"] = config_.name;
    j["texturePath"] = config_.texturePath;
    j["columns"] = config_.columns;
    j["rows"] = config_.rows;
    j["totalFrames"] = config_.totalFrames;
    j["fps"] = config_.fps;
    j["isLoop"] = config_.isLoop;
    j["translation"] = { config_.translation.x, config_.translation.y, config_.translation.z };
    j["rotation"] = { config_.rotation.x, config_.rotation.y, config_.rotation.z };
    j["scale"] = { config_.scale.x, config_.scale.y, config_.scale.z };
    j["color"] = { config_.color.x, config_.color.y, config_.color.z, config_.color.w };
    j["isBillboard"] = config_.isBillboard;
    j["isDoubleSided"] = config_.isDoubleSided;
    j["showInAllStages"] = config_.showInAllStages;
    return j;
}

bool WorldGifObject::FromJson(const nlohmann::json& j, ID3D12Device* device) {
    WorldGifConfig cfg;
    cfg.name = j.value("name", "GifPlane");
    cfg.texturePath = j.value("texturePath", "resources/Sprite/Original/gif/test_gif.png");
    cfg.columns = j.value("columns", 4);
    cfg.rows = j.value("rows", 4);
    cfg.totalFrames = j.value("totalFrames", 16);
    if (cfg.totalFrames <= 1 && (cfg.columns * cfg.rows) > 1) {
        cfg.totalFrames = cfg.columns * cfg.rows;
    }
    cfg.fps = j.value("fps", 15.0f);
    cfg.isLoop = j.value("isLoop", true);
    cfg.isBillboard = j.value("isBillboard", false);
    cfg.isDoubleSided = j.value("isDoubleSided", true);
    cfg.showInAllStages = j.value("showInAllStages", false);

    if (j.contains("translation") && j["translation"].is_array() && j["translation"].size() >= 3) {
        cfg.translation = { j["translation"][0].get<float>(), j["translation"][1].get<float>(), j["translation"][2].get<float>() };
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
        cfg.rotation = { j["rotation"][0].get<float>(), j["rotation"][1].get<float>(), j["rotation"][2].get<float>() };
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        cfg.scale = { j["scale"][0].get<float>(), j["scale"][1].get<float>(), j["scale"][2].get<float>() };
    }
    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
        cfg.color = { j["color"][0].get<float>(), j["color"][1].get<float>(), j["color"][2].get<float>(), j["color"][3].get<float>() };
    }

    return Initialize(device, cfg);
}

// ============================================================================
// StageGifManager 実装
// ============================================================================
StageGifManager* StageGifManager::GetInstance() {
    static StageGifManager instance;
    return &instance;
}

void StageGifManager::Initialize(ID3D12Device* device) {
    device_ = device;
}

void StageGifManager::Update(float deltaTime) {
    for (auto& obj : objects_) {
        if (obj) {
            obj->Update(deltaTime);
        }
    }
}

void StageGifManager::Draw() {
    for (auto& obj : objects_) {
        if (obj) {
            obj->Draw();
        }
    }
}

std::string StageGifManager::ResolveStageName(const std::string& stagePathOrName) const {
    if (stagePathOrName.empty()) {
        return currentStageName_.empty() ? "tutorial" : currentStageName_;
    }
    std::filesystem::path p(stagePathOrName);
    std::string stem = p.stem().string();
    // temp_play_map はエディタのプレイ中の一時マップなので除外して現在有効なステージ名を維持
    if (stem == "temp_play_map") {
        return currentStageName_.empty() ? "tutorial" : currentStageName_;
    }
    // もし _gifs が末尾についていたら除去
    const std::string suffix = "_gifs";
    if (stem.size() > suffix.size() && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0) {
        stem = stem.substr(0, stem.size() - suffix.size());
    }
    return stem;
}

std::string StageGifManager::GetStageJsonPath(const std::string& stageName) const {
    std::string sName = ResolveStageName(stageName);
    return "resources/json/shared/StageGifs/" + sName + "_gifs.json";
}

std::vector<std::string> StageGifManager::ScanAvailableStages() const {
    std::vector<std::string> stages;

    // MapData フォルダ内のマップファイルをスキャン
    std::filesystem::path mapDir("resources/json/shared/MapData");
    if (std::filesystem::exists(mapDir) && std::filesystem::is_directory(mapDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(mapDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".txt" || ext == ".json") {
                    std::string stem = entry.path().stem().string();
                    // _bounds や _config 等のメタファイル、temp_play_map は除外
                    if (stem.find("_bounds") == std::string::npos && stem.find("_config") == std::string::npos && stem != "temp_play_map") {
                        stages.push_back(stem);
                    }
                }
            }
        }
    }

    // StageGifs フォルダ内の既存ファイルもスキャン
    std::filesystem::path gifDir("resources/json/shared/StageGifs");
    if (std::filesystem::exists(gifDir) && std::filesystem::is_directory(gifDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(gifDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string sName = ResolveStageName(entry.path().string());
                if (sName != "temp_play_map" && std::find(stages.begin(), stages.end(), sName) == stages.end()) {
                    stages.push_back(sName);
                }
            }
        }
    }

    if (stages.empty()) {
        stages.push_back("tutorial");
    }

    std::sort(stages.begin(), stages.end());
    stages.erase(std::unique(stages.begin(), stages.end()), stages.end());
    return stages;
}

bool StageGifManager::LoadForStage(const std::string& stagePathOrName) {
    std::string sName = ResolveStageName(stagePathOrName);
    if (sName.empty() || sName == "temp_play_map") {
        return false;
    }
    currentStageName_ = sName;
    std::string jsonPath = GetStageJsonPath(currentStageName_);

    Clear();

    if (!std::filesystem::exists(jsonPath)) {
        // 自ステージの設定ファイルがない場合、showAllStages_が有効なら他の存在するステージから自動インポート
        if (showAllStages_) {
            for (const auto& otherStage : ScanAvailableStages()) {
                if (otherStage != currentStageName_ && std::filesystem::exists(GetStageJsonPath(otherStage))) {
                    ImportFromStage(otherStage);
                    break;
                }
            }
        }
        return false;
    }

    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        ifs >> root;
    } catch (...) {
        return false;
    }

    if (!root.is_array()) {
        return false;
    }

    for (const auto& item : root) {
        auto obj = std::make_unique<WorldGifObject>();
        if (obj->FromJson(item, device_)) {
            objects_.push_back(std::move(obj));
        }
    }

    // 他ステージで「showInAllStages == true」に設定されているGIFがあれば合流させる
    for (const auto& otherStage : ScanAvailableStages()) {
        if (otherStage == currentStageName_) continue;
        std::string otherJson = GetStageJsonPath(otherStage);
        if (!std::filesystem::exists(otherJson)) continue;
        std::ifstream oifs(otherJson);
        if (!oifs.is_open()) continue;
        try {
            nlohmann::json oroot;
            oifs >> oroot;
            if (oroot.is_array()) {
                for (const auto& item : oroot) {
                    if (item.value("showInAllStages", false)) {
                        std::string gName = item.value("name", "");
                        bool exists = false;
                        for (const auto& curObj : objects_) {
                            if (curObj && curObj->GetConfig().name == gName) { exists = true; break; }
                        }
                        if (!exists) {
                            auto obj = std::make_unique<WorldGifObject>();
                            if (obj->FromJson(item, device_)) {
                                objects_.push_back(std::move(obj));
                            }
                        }
                    }
                }
            }
        } catch (...) {}
    }

    // 自ステージの配置が空で、showAllStages_が有効な場合も他ステージから読み込み
    if (objects_.empty() && showAllStages_) {
        for (const auto& otherStage : ScanAvailableStages()) {
            if (otherStage != currentStageName_ && std::filesystem::exists(GetStageJsonPath(otherStage))) {
                ImportFromStage(otherStage);
                break;
            }
        }
    }

    return true;
}

bool StageGifManager::ImportFromStage(const std::string& sourceStageName) {
    if (!device_) return false;
    std::string srcJson = GetStageJsonPath(sourceStageName);
    if (!std::filesystem::exists(srcJson)) return false;

    std::ifstream ifs(srcJson);
    if (!ifs.is_open()) return false;

    nlohmann::json root;
    try {
        ifs >> root;
    } catch (...) {
        return false;
    }
    if (!root.is_array()) return false;

    for (const auto& item : root) {
        auto obj = std::make_unique<WorldGifObject>();
        if (obj->FromJson(item, device_)) {
            objects_.push_back(std::move(obj));
        }
    }
    return true;
}

bool StageGifManager::SaveForStage(const std::string& stagePathOrName) {
    std::string sName = ResolveStageName(stagePathOrName);
    currentStageName_ = sName;
    std::string jsonPath = GetStageJsonPath(sName);

    std::filesystem::path p(jsonPath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    nlohmann::json root = nlohmann::json::array();
    for (const auto& obj : objects_) {
        if (obj) {
            root.push_back(obj->ToJson());
        }
    }

    std::ofstream ofs(jsonPath);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << root.dump(4);
    return true;
}

WorldGifObject* StageGifManager::AddGifObject(const WorldGifConfig& config) {
    if (!device_) return nullptr;

    auto obj = std::make_unique<WorldGifObject>();
    if (!obj->Initialize(device_, config)) {
        return nullptr;
    }

    WorldGifObject* ptr = obj.get();
    objects_.push_back(std::move(obj));
    return ptr;
}

void StageGifManager::RemoveGifObject(WorldGifObject* target) {
    if (!target) return;
    auto it = std::remove_if(objects_.begin(), objects_.end(), [target](const std::unique_ptr<WorldGifObject>& o) {
        return o.get() == target;
    });
    objects_.erase(it, objects_.end());
}

WorldGifObject* StageGifManager::DuplicateGifObject(WorldGifObject* target) {
    if (!target || !device_) return nullptr;

    WorldGifConfig cfg = target->GetConfig();
    cfg.name += "_Copy";
    cfg.translation.x += 1.0f; // 少しずらして配置

    return AddGifObject(cfg);
}

void StageGifManager::Clear() {
    objects_.clear();
}
