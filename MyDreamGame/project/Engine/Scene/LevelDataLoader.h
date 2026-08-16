#pragma once
#include <string>
#include <vector>
#include <memory>
#include <d3d12.h>
#include <nlohmann/json.hpp>
#include "Core/Utility/Vector3.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelCommon.h"

// --- レベルデータ構造体 ---
namespace LevelDataStructs {

struct TransformData {
    Vector3 translation = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // 度数法 (Degrees)
    Vector3 scaling = { 1.0f, 1.0f, 1.0f };
};

struct ColliderData {
    std::string type = "BOX";
    Vector3 center = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
};

struct ObjectData {
    std::string type;     // "MESH", "LIGHT", "CAMERA" など
    std::string name;     // オブジェクト名
    TransformData transform;
    std::string fileName; // モデルのファイル名 (カスタムプロパティ file_name)
    
    bool disabled = false; // 有効無効フラグ (カスタムプロパティ 無効オプション)

    bool hasCollider = false;
    ColliderData collider;

    std::vector<ObjectData> children; // 子要素 (ツリー構造)
};

struct PlayerSpawnData {
    Vector3 translation = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
};

struct LevelData {
    std::string name = "scene";
    std::vector<ObjectData> objects;
    std::vector<PlayerSpawnData> players; // 自キャラ出現データ配列
};

} // namespace LevelDataStructs

// --- レベルデータローダークラス ---
class LevelDataLoader {
public:
    LevelDataLoader() = default;
    ~LevelDataLoader() = default;

    /// <summary>
    /// レベルデータファイル (.json または .scene) をロードする
    /// </summary>
    /// <param name="filePath">ファイルパス</param>
    /// <returns>読み込み成功時 true</returns>
    bool LoadFile(const std::string& filePath);

    /// <summary>
    /// JSON形式のレベルデータをロードする
    /// </summary>
    bool LoadJSON(const std::string& filePath);

    /// <summary>
    /// テキスト形式 (.scene) のレベルデータをロードする (資料 01_11 / 01_12 形式)
    /// </summary>
    bool LoadSceneText(const std::string& filePath);

    /// <summary>
    /// 読み込んだLevelDataからObject3Dインスタンス群を生成する
    /// </summary>
    /// <param name="device">D3D12デバイス</param>
    /// <param name="modelCommon">ModelCommonポインタ</param>
    /// <param name="baseDirectoryPath">モデルが格納されているベースディレクトリ</param>
    /// <returns>生成されたObject3Dのunique_ptrリスト</returns>
    std::vector<std::unique_ptr<Object3D>> CreateObjects(
        ID3D12Device* device,
        ModelCommon* modelCommon,
        const std::string& baseDirectoryPath = "resources/");

    /// <summary>
    /// 読み込まれたLevelDataを取得
    /// </summary>
    const LevelDataStructs::LevelData& GetLevelData() const { return levelData_; }

#ifdef USE_IMGUI
    /// <summary>
    /// ImGuiデバッグ表示
    /// </summary>
    void DisplayImGui(ID3D12Device* device, ModelCommon* modelCommon, std::vector<std::unique_ptr<Object3D>>& outObjects);
#endif

private:
    /// <summary>
    /// JSONオブジェクトを再帰的にパースする
    /// </summary>
    void ParseObjectRecursive(const nlohmann::json& objectJson, LevelDataStructs::ObjectData& outObjectData);

    /// <summary>
    /// ObjectDataを再帰的に走査してObject3Dを生成する
    /// </summary>
    void CreateObjectRecursive(
        const LevelDataStructs::ObjectData& objectData,
        ID3D12Device* device,
        ModelCommon* modelCommon,
        const std::string& baseDirectoryPath,
        std::vector<std::unique_ptr<Object3D>>& outObjects);

private:
    LevelDataStructs::LevelData levelData_;
    std::string loadedFilePath_;
    bool isLoaded_ = false;
};
