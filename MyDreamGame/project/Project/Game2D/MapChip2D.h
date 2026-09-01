#pragma once
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Core/Utility/Structs.h"
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "Blocks/BaseBlock.h"

/// <summary>
/// 2Dスクロールゲーム用マップクラス
/// 2D配列でマップチップを管理し、PrimitiveObject(Box)で描画する
/// </summary>

class MapChip2D {
public:
    enum class ChipType : int {
        kNone = 0,        // 空気（何もなし）
        kBlock = 1,       // ブロック（地面・壁）
        kDeathBlock = 2,   // デスブロック（触れたら死ぬ）
        kGoal = 3,        // ゴール
        kOneWayBlock = 5, // 一方向通行床
        kPlayerSpawn = 6, // プレイヤー初期位置
        kRoomRespawn = 10, // 部屋用リスポーン地点
        kChainItemBlock = 11, // 鎖アイテム
        kMovingBlock = 12, // 移動する床
        kFragileBlock = 13, // 鎖の重さで崩れる床
        kSwitchBlock = 14, // スイッチ
        kDoorBlock = 15 // シャッタードア
    };

    void Initialize(const std::string& mapFilePath);
    void Update();
    void Draw();

    // 指定座標のブロックを取得する
    BaseBlock* GetBlock(int chipX, int chipY) const;

    // 指定座標のチップの種類を取得
    ChipType GetChipType(int chipX, int chipY) const;

    // ワールド座標 → チップ座標 変換
    int WorldToChipX(float worldX) const;
    int WorldToChipY(float worldY) const;

    // チップ座標 → ワールド座標（チップの左下） 変換
    float ChipToWorldX(int chipX) const;
    float ChipToWorldY(int chipY) const;

    // プレイヤー初期位置 (kPlayerSpawn) の取得
    bool HasPlayerSpawn() const;
    bool GetPlayerSpawnChipPosition(int& outX, int& outY) const;
    Vector3 GetPlayerSpawnWorldPosition(const Vector3& defaultPos = {2.0f, 5.0f, 0.0f}) const;

    // マップの寸法
    int GetWidth() const { return mapWidth_; }
    int GetHeight() const { return mapHeight_; }
    float GetChipSize() const { return chipSize_; }

    // マップサイズを動的に変更
    void Resize(int newWidth, int newHeight);

    // ヒエラルキー用
    std::vector<PrimitiveObject*> GetPrimitiveObjects();

    // チップを設定
    void SetChip(int x, int y, ChipType type);

    // チップを取得
    ChipType GetChip(int x, int y) const;

    // バケツ塗り（フラッドフィル）
    void BucketFill(int startX, int startY, ChipType targetType, ChipType replacementType);

    // マップをすべてクリア
    void ClearMap();

    // マップを再構築（初期のBuildMapを呼び出し）
    void ResetMap();

    // マップデータの動的再構築
    void RebuildChipObjects();

    // シミュレーション時の再構築抑制用
    void SetRebuildEnabled(bool enabled) { 
        isRebuildEnabled_ = enabled; 
        if (enabled) RebuildChipObjects(); 
    }

    bool SaveToFile(const std::string& filepath);
    bool LoadFromFile(const std::string& filepath);
    bool LoadFromStageName(const std::string& stageName);

    // 文字列ベースのマップデータ取得＆設定（リプレイ用）
    std::string GetMapDataAsString() const;
    bool LoadFromString(const std::string& data);

    std::vector<StageRoom>& GetRooms() { return rooms_; }
    const std::vector<StageRoom>& GetRooms() const { return rooms_; }
    
    // デフォルトルームの生成（マップサイズに基づいて自動生成）
    void GenerateDefaultRooms();
    
    // ルームデータの保存と読込
    bool SaveRoomsToFile(const std::string& filepath);
    bool LoadRoomsFromFile(const std::string& filepath);

    // 描画および動的更新対象のブロックリストを取得（動的当たり判定用）
    const std::vector<std::shared_ptr<BaseBlock>>& GetUpdateBlocks() const { return updateBlocks_; }

    // 全ブロックのリセット（プレイヤー死亡・リトライ時）
    void ResetBlocks();

    struct CustomBlockDef {
        int id = 100;
        std::string name = "New Custom Block";
        std::string type = "JumpBlock";
        nlohmann::json properties = nlohmann::json::object();
        Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        Vector3 scale = {1.0f, 1.0f, 1.0f};
        std::string modelName = "";
        std::string textureName = "";
    };

    std::vector<CustomBlockDef>& GetCustomPalette() { return customPalette_; }
    const std::vector<CustomBlockDef>& GetCustomPalette() const { return customPalette_; }

    std::vector<CustomBlockDef>& GetTemplatePalette() { return templatePalette_; }
    const std::vector<CustomBlockDef>& GetTemplatePalette() const { return templatePalette_; }

    // テンプレートのグローバル保存/読込
    bool SaveTemplatesToFile(const std::string& filepath);
    bool LoadTemplatesFromFile(const std::string& filepath);

    // デフォルトマップの構築
    void BuildMap();

public:
    void SetDirty() { isDirty_ = true; }
private:
    std::shared_ptr<BaseBlock> InstantiateBlock(int x, int y, ChipType type, int spanWidth, int spanHeight, class Primitive* boxPrimitive);
    void CreateChipObjects();

private:
    // マップデータ（左下が(0,0)）
    std::vector<std::vector<ChipType>> mapData_;
    int mapWidth_ = 0;
    int mapHeight_ = 0;
    float chipSize_ = 1.0f; // 1チップのサイズ（ワールド座標）

    // ルームデータ（ワールド座標）
    std::vector<StageRoom> rooms_;

    std::unique_ptr<GameObject> boundaries_[4];
    void CreateBoundaries();

    // 実行時に生成される各種ブロックのインスタンス
    std::vector<std::vector<std::shared_ptr<BaseBlock>>> activeBlocks_;
    
    // 更新・描画用のユニークなブロックリスト
    std::vector<std::shared_ptr<BaseBlock>> updateBlocks_;

    // カスタムパレット（自作ブロック定義リスト）
    std::vector<CustomBlockDef> customPalette_;
    
    // テンプレートパレット（BasicToolsの設定用）
    std::vector<CustomBlockDef> templatePalette_;

    // 実行時の動的再構築用のキャッシュ
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};

    bool isRebuildEnabled_ = true;
    bool isDirty_ = false;
    std::string currentFilePath_ = "";
};
