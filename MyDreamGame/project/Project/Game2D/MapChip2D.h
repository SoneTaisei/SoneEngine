#pragma once
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Core/Utility/Structs.h"
#include <vector>
#include <memory>
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
    };

    void Initialize(ID3D12GraphicsCommandList* commandList);
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

private:
    void BuildMap();
    void CreateChipObjects(ID3D12GraphicsCommandList* commandList);

private:
    // マップデータ（左下が(0,0)）
    std::vector<std::vector<ChipType>> mapData_;
    int mapWidth_ = 0;
    int mapHeight_ = 0;
    float chipSize_ = 1.0f; // 1チップのサイズ（ワールド座標）

    // 実行時に生成される各種ブロックのインスタンス
    std::vector<std::vector<std::unique_ptr<BaseBlock>>> activeBlocks_;

    // 実行時の動的再構築用のキャッシュ
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};

    bool isRebuildEnabled_ = true;
};
