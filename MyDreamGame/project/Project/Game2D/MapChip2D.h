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
        kNone = 0,  // 空気（何もなし）
        kBlock = 1, // ブロック（地面・壁）
        kDeathBlock = 2, // デスブロック（触れたら死ぬ）
        kGoal = 3,  // ゴール
        kCoin = 4,  // コイン
        kOneWayBlock = 5, // 一方向通行床
        kPlayerSpawn = 6, // プレイヤー初期位置
        kLift = 7,  // 動く足場（リフト）
        kRail = 8,  // リフトの移動レール
        kJumpBlock = 9, // ジャンプ台
    };

    void Initialize(ID3D12GraphicsCommandList* commandList, const std::string& mapFilePath);
    void Update();
    void Draw(ID3D12GraphicsCommandList* commandList);

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

    // 文字列ベースのマップデータ取得＆設定（リプレイ用）
    std::string GetMapDataAsString() const;
    bool LoadFromString(const std::string& data);

    // 境界線（ルームトリガー）データの管理
    std::vector<float>& GetBoundaryX() { return boundaryX_; }
    std::vector<float>& GetBoundaryY() { return boundaryY_; }
    const std::vector<float>& GetBoundaryX() const { return boundaryX_; }
    const std::vector<float>& GetBoundaryY() const { return boundaryY_; }
    
    // デフォルト境界線の生成（マップサイズに基づいて自動生成）
    void GenerateDefaultBoundaries();
    
    // 境界線メタデータの保存と読込
    bool SaveBoundariesToFile(const std::string& filepath);
    bool LoadBoundariesFromFile(const std::string& filepath);

    // 描画および動的更新対象のブロックリストを取得（動的当たり判定用）
    const std::vector<std::shared_ptr<BaseBlock>>& GetUpdateBlocks() const { return updateBlocks_; }

    struct CustomBlockDef {
        int id = 100;
        std::string name = "New Custom Block";
        std::string type = "JumpBlock";
        nlohmann::json properties = nlohmann::json::object();
        Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        Vector3 scale = {1.0f, 1.0f, 1.0f};
        std::string modelName = "";
    };

    std::vector<CustomBlockDef>& GetCustomPalette() { return customPalette_; }
    const std::vector<CustomBlockDef>& GetCustomPalette() const { return customPalette_; }

    std::vector<CustomBlockDef>& GetTemplatePalette() { return templatePalette_; }
    const std::vector<CustomBlockDef>& GetTemplatePalette() const { return templatePalette_; }

    // テンプレートのグローバル保存/読込
    bool SaveTemplatesToFile(const std::string& filepath);
    bool LoadTemplatesFromFile(const std::string& filepath);

private:
    void BuildMap();
    void CreateChipObjects(ID3D12GraphicsCommandList* commandList);

private:
    // マップデータ（左下が(0,0)）
    std::vector<std::vector<ChipType>> mapData_;
    int mapWidth_ = 0;
    int mapHeight_ = 0;
    float chipSize_ = 1.0f; // 1チップのサイズ（ワールド座標）

    // ルーム境界線データ（ワールド座標）
    std::vector<float> boundaryX_;
    std::vector<float> boundaryY_;

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
};
