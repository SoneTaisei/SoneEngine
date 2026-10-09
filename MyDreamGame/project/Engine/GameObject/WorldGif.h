#pragma once
#include <string>
#include <vector>
#include <memory>
#include <d3d12.h>
#include <nlohmann/json.hpp>
#include "Core/Utility/Structs.h"
#include "GameObject/PrimitiveObject.h"

// ワールド配置GIF（板ポリ）の設定構造体
struct WorldGifConfig {
    std::string name = "GifPlane";
    std::string texturePath = "resources/Sprite/Original/gif/test_gif.png";
    int columns = 4;
    int rows = 4;
    int totalFrames = 16;
    float fps = 15.0f;
    bool isLoop = true;

    Vector3 translation = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { -1.5707963f, 0.0f, 0.0f }; // 2.5Dゲーム視点向けに垂直（X-Y平面）初期化
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    bool isBillboard = false;
    bool isDoubleSided = true;
    bool showInAllStages = false; // 全ステージ共通で表示するフラグ
};

// 3Dワールド空間上に配置される板ポリGIFオブジェクト
class WorldGifObject {
public:
    WorldGifObject();
    ~WorldGifObject() = default;

    bool Initialize(ID3D12Device* device, const WorldGifConfig& config);
    void Update(float deltaTime);
    void Draw();

    WorldGifConfig& GetConfig() { return config_; }
    const WorldGifConfig& GetConfig() const { return config_; }
    void SetConfig(const WorldGifConfig& config, ID3D12Device* device = nullptr);

    const Vector3& GetTranslation() const { return config_.translation; }
    void SetTranslation(const Vector3& translation) { config_.translation = translation; UpdateMaterialAndTransform(); }

    const Vector3& GetRotation() const { return config_.rotation; }
    void SetRotation(const Vector3& rotation) { config_.rotation = rotation; UpdateMaterialAndTransform(); }

    const Vector3& GetScale() const { return config_.scale; }
    void SetScale(const Vector3& scale) { config_.scale = scale; UpdateMaterialAndTransform(); }

    const Vector4& GetColor() const { return config_.color; }
    void SetColor(const Vector4& color) { config_.color = color; UpdateMaterialAndTransform(); }

    bool IsBillboard() const { return config_.isBillboard; }
    void SetBillboard(bool isBillboard) { config_.isBillboard = isBillboard; UpdateMaterialAndTransform(); }

    bool IsDoubleSided() const { return config_.isDoubleSided; }
    void SetDoubleSided(bool isDoubleSided) { config_.isDoubleSided = isDoubleSided; UpdateMaterialAndTransform(); }

    void ForceUpdate() { UpdateMaterialAndTransform(); }
    void UpdateMaterialAndTransform();

    PrimitiveObject* GetPrimitiveObject() { return primitiveObj_.get(); }
    const PrimitiveObject* GetPrimitiveObject() const { return primitiveObj_.get(); }

    void Play() { isPlaying_ = true; }
    void Pause() { isPlaying_ = false; }
    void Stop() { isPlaying_ = false; currentFrame_ = 0; animTimer_ = 0.0f; }
    bool IsPlaying() const { return isPlaying_; }

    nlohmann::json ToJson() const;
    bool FromJson(const nlohmann::json& j, ID3D12Device* device);

private:

private:
    WorldGifConfig config_;
    std::unique_ptr<PrimitiveObject> primitiveObj_;
    uint32_t textureHandle_ = 0;

    bool isPlaying_ = true;
    float animTimer_ = 0.0f;
    int currentFrame_ = 0;
};

// ステージ別のワールドGIF配置管理クラス
class StageGifManager {
public:
    static StageGifManager* GetInstance();

    void Initialize(ID3D12Device* device);
    void Update(float deltaTime);
    void Draw();

    // ステージごとのJSON保存・読み込み
    bool LoadForStage(const std::string& stagePathOrName);
    bool SaveForStage(const std::string& stagePathOrName);

    const std::string& GetCurrentStageName() const { return currentStageName_; }
    void SetCurrentStageName(const std::string& name) { currentStageName_ = name; }

    // オブジェクト管理
    WorldGifObject* AddGifObject(const WorldGifConfig& config);
    void RemoveGifObject(WorldGifObject* target);
    WorldGifObject* DuplicateGifObject(WorldGifObject* target);
    void Clear();

    std::vector<std::unique_ptr<WorldGifObject>>& GetObjects() { return objects_; }
    const std::vector<std::unique_ptr<WorldGifObject>>& GetObjects() const { return objects_; }

    std::string ResolveStageName(const std::string& stagePathOrName) const;
    std::string GetStageJsonPath(const std::string& stageName) const;
    std::vector<std::string> ScanAvailableStages() const;

    bool ImportFromStage(const std::string& sourceStageName);
    bool GetShowAllStages() const { return showAllStages_; }
    void SetShowAllStages(bool enable) { showAllStages_ = enable; }

private:
    StageGifManager() = default;
    ~StageGifManager() = default;
    StageGifManager(const StageGifManager&) = delete;
    StageGifManager& operator=(const StageGifManager&) = delete;

private:
    ID3D12Device* device_ = nullptr;
    std::vector<std::unique_ptr<WorldGifObject>> objects_;
    std::string currentStageName_ = "tutorial";
    bool showAllStages_ = false; // ステージに関わらず全GIFを表示するフラグ
};
