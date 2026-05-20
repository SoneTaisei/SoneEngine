#pragma once
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Core/Utility/Structs.h"
#include <vector>
#include <memory>

/// <summary>
/// 2Dスクロールゲーム用マップクラス
/// 2D配列でマップチップを管理し、PrimitiveObject(Box)で描画する
/// </summary>
class MapChip2D {
public:
    // チップの種類
    enum class ChipType : int {
        kNone = 0,  // 空気（何もなし）
        kBlock = 1, // ブロック（地面・壁）
    };

    void Initialize(ID3D12GraphicsCommandList* commandList);
    void Update();
    void Draw(ID3D12GraphicsCommandList* commandList);

    // 指定座標がブロックかどうか判定
    bool IsBlock(int chipX, int chipY) const;

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

    // ヒエラルキー用
    std::vector<PrimitiveObject*> GetPrimitiveObjects();

private:
    void BuildMap();
    void CreateChipObjects(ID3D12GraphicsCommandList* commandList);

private:
    // マップデータ（左下が(0,0)）
    std::vector<std::vector<ChipType>> mapData_;
    int mapWidth_ = 0;
    int mapHeight_ = 0;
    float chipSize_ = 1.0f; // 1チップのサイズ（ワールド座標）

    // 描画用オブジェクト（ブロックのみ）
    std::vector<std::unique_ptr<PrimitiveObject>> chipObjects_;
};
