#include "MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/TextureManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include "Blocks/NormalBlock.h"
#include "Blocks/DeathBlock.h"
#include "Blocks/GoalBlock.h"
#include "Blocks/CoinBlock.h"
#include "Blocks/OneWayBlock.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include "Resource/Primitive/PrimitiveManager.h"

void MapChip2D::Initialize(ID3D12GraphicsCommandList* commandList) {
    commandList->GetDevice(IID_PPV_ARGS(&device_));

    // デフォルトテクスチャのロード
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> comPtrCommandList(commandList);
    uint32_t texHandle = TextureManager::GetInstance()->Load("Object/School/human/white.png", comPtrCommandList);
    gpuHandle_ = TextureManager::GetInstance()->GetGpuHandle(texHandle);

    // 保存ファイルがあれば読込み、なければ初期構築して保存する
    if (!LoadFromFile("json/map_data.txt")) {
        BuildMap();
        SaveToFile("json/map_data.txt");
    }
}

void MapChip2D::Update() {
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (activeBlocks_[y][x]) {
                activeBlocks_[y][x]->Update();
                if (activeBlocks_[y][x]->IsDestroyed()) {
                    activeBlocks_[y][x].reset();
                    mapData_[y][x] = ChipType::kNone;
                }
            }
        }
    }
}

void MapChip2D::Draw(ID3D12GraphicsCommandList* commandList) {
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (activeBlocks_[y][x]) {
                activeBlocks_[y][x]->Draw(commandList);
            }
        }
    }
}

BaseBlock* MapChip2D::GetBlock(int chipX, int chipY) const {
    if (chipX < 0 || chipX >= mapWidth_ || chipY < 0 || chipY >= mapHeight_) {
        return nullptr;
    }
    return activeBlocks_[chipY][chipX].get();
}

MapChip2D::ChipType MapChip2D::GetChipType(int chipX, int chipY) const {
    return GetChip(chipX, chipY);
}

int MapChip2D::WorldToChipX(float worldX) const {
    return static_cast<int>(std::floor(worldX / chipSize_));
}

int MapChip2D::WorldToChipY(float worldY) const {
    return static_cast<int>(std::floor(worldY / chipSize_));
}

float MapChip2D::ChipToWorldX(int chipX) const {
    return static_cast<float>(chipX) * chipSize_;
}

float MapChip2D::ChipToWorldY(int chipY) const {
    return static_cast<float>(chipY) * chipSize_;
}

std::vector<PrimitiveObject*> MapChip2D::GetPrimitiveObjects() {
    std::vector<PrimitiveObject*> result;
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (activeBlocks_[y][x]) {
                auto* prim = activeBlocks_[y][x]->GetPrimitive();
                if (prim) {
                    result.push_back(prim);
                }
            }
        }
    }
    return result;
}

void MapChip2D::BuildMap() {
    // 横30 x 縦15 のシンプルなステージ
    // 下が y=0, 上が y=14
    // 1 = ブロック, 0 = 空気
    mapWidth_ = 40;
    mapHeight_ = 15;

    // まず全部空気で初期化
    mapData_.resize(mapHeight_, std::vector<ChipType>(mapWidth_, ChipType::kNone));

    // --- 地面（y=0）を敷く ---
    for (int x = 0; x < mapWidth_; ++x) {
        mapData_[0][x] = ChipType::kBlock;
    }

    // --- y=1にも地面（厚みを持たせる） ---
    for (int x = 0; x < mapWidth_; ++x) {
        mapData_[1][x] = ChipType::kBlock;
    }

    // --- 左壁 ---
    for (int y = 0; y < mapHeight_; ++y) {
        mapData_[y][0] = ChipType::kBlock;
    }

    // --- 右壁 ---
    for (int y = 0; y < mapHeight_; ++y) {
        mapData_[y][mapWidth_ - 1] = ChipType::kBlock;
    }

    // --- 穴を作る（代わりにデスブロックを設置）---
    mapData_[0][8] = ChipType::kDeathBlock;
    mapData_[1][8] = ChipType::kDeathBlock;
    mapData_[0][9] = ChipType::kDeathBlock;
    mapData_[1][9] = ChipType::kDeathBlock;

    // --- 段差（x=12〜14, y=2〜3）---
    for (int x = 12; x <= 14; ++x) {
        mapData_[2][x] = ChipType::kBlock;
        mapData_[3][x] = ChipType::kBlock;
    }

    // --- 浮島（x=18〜22, y=4）---
    for (int x = 18; x <= 22; ++x) {
        mapData_[4][x] = ChipType::kBlock;
    }

    // --- 階段（x=25〜29）---
    for (int step = 0; step < 5; ++step) {
        int x = 25 + step;
        for (int y = 2; y <= 2 + step; ++y) {
            if (x < mapWidth_) {
                mapData_[y][x] = ChipType::kBlock;
            }
        }
    }

    // --- 高い足場（x=32〜36, y=7）---
    for (int x = 32; x <= 36; ++x) {
        mapData_[7][x] = ChipType::kBlock;
    }

    // --- 中間足場（x=15〜17, y=6）---
    for (int x = 15; x <= 17; ++x) {
        mapData_[6][x] = ChipType::kBlock;
    }

    // --- 地面の上にデスブロックを1つ追加 ---
    mapData_[2][20] = ChipType::kDeathBlock;

    // --- ゴール配置 ---
    mapData_[8][35] = ChipType::kGoal;

    // --- コイン配置 ---
    mapData_[4][13] = ChipType::kCoin;
    mapData_[5][20] = ChipType::kCoin;
    mapData_[4][27] = ChipType::kCoin;
}

void MapChip2D::CreateChipObjects(ID3D12GraphicsCommandList* commandList) {
    RebuildChipObjects();
}

void MapChip2D::SetChip(int x, int y, ChipType type) {
    if (x < 0 || x >= mapWidth_ || y < 0 || y >= mapHeight_) return;

    // もしプレイヤー初期位置を置こうとしているなら、他の初期位置を消す
    if (type == ChipType::kPlayerSpawn) {
        for (int cy = 0; cy < mapHeight_; ++cy) {
            for (int cx = 0; cx < mapWidth_; ++cx) {
                if (mapData_[cy][cx] == ChipType::kPlayerSpawn) {
                    mapData_[cy][cx] = ChipType::kNone;
                }
            }
        }
    }

    if (mapData_[y][x] != type) {
        mapData_[y][x] = type;
        RebuildChipObjects();
    }
}

MapChip2D::ChipType MapChip2D::GetChip(int x, int y) const {
    if (x < 0 || x >= mapWidth_ || y < 0 || y >= mapHeight_) return ChipType::kNone;
    return mapData_[y][x];
}

void MapChip2D::ClearMap() {
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            mapData_[y][x] = ChipType::kNone;
        }
    }
    RebuildChipObjects();
}

void MapChip2D::ResetMap() {
    BuildMap();
    RebuildChipObjects();
}

void MapChip2D::RebuildChipObjects() {
    if (!isRebuildEnabled_) return;

    activeBlocks_.clear();
    activeBlocks_.resize(mapHeight_);

    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);

    for (int y = 0; y < mapHeight_; ++y) {
        activeBlocks_[y].resize(mapWidth_);
        for (int x = 0; x < mapWidth_; ++x) {
            float worldX = ChipToWorldX(x) + chipSize_ * 0.5f;
            float worldY = ChipToWorldY(y) + chipSize_ * 0.5f;

            ChipType type = mapData_[y][x];
            std::unique_ptr<BaseBlock> newBlock = nullptr;

            if (type == ChipType::kBlock) {
                newBlock = std::make_unique<NormalBlock>(this, x, y);
            } else if (type == ChipType::kDeathBlock) {
                newBlock = std::make_unique<DeathBlock>(this, x, y);
            } else if (type == ChipType::kGoal) {
                newBlock = std::make_unique<GoalBlock>(this, x, y);
            } else if (type == ChipType::kCoin) {
                newBlock = std::make_unique<CoinBlock>(this, x, y);
            } else if (type == ChipType::kOneWayBlock) {
                newBlock = std::make_unique<OneWayBlock>(this, x, y);
            }

            if (newBlock) {
                newBlock->Initialize(device_.Get(), boxPrimitive, worldX, worldY, chipSize_);
                // 実行時にオブジェクトに名前を付けたい場合など
                if (newBlock->GetPrimitive()) {
                    newBlock->GetPrimitive()->SetName("MapChip_" + std::to_string(x) + "_" + std::to_string(y));
                }
                activeBlocks_[y][x] = std::move(newBlock);
            }
        }
    }
}

bool MapChip2D::SaveToFile(const std::string& filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    // ヘッダー（幅 高さ）
    ofs << mapWidth_ << " " << mapHeight_ << "\n";

    // グリッドデータ
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            ofs << static_cast<int>(mapData_[y][x]);
            if (x < mapWidth_ - 1) ofs << " ";
        }
        ofs << "\n";
    }
    ofs.close();
    return true;
}

bool MapChip2D::LoadFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return LoadFromString(buffer.str());
}

std::string MapChip2D::GetMapDataAsString() const {
    std::stringstream ss;
    ss << mapWidth_ << " " << mapHeight_ << "\n";
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            ss << static_cast<int>(mapData_[y][x]);
            if (x < mapWidth_ - 1) ss << " ";
        }
        ss << "\n";
    }
    return ss.str();
}

bool MapChip2D::LoadFromString(const std::string& data) {
    if (data.empty()) return false;
    std::stringstream iss(data);

    int w, h;
    if (!(iss >> w >> h)) return false;
    if (w < 1 || h < 1) return false;

    mapWidth_ = w;
    mapHeight_ = h;
    mapData_.assign(mapHeight_, std::vector<ChipType>(mapWidth_, ChipType::kNone));

    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            int val;
            if (iss >> val) {
                mapData_[y][x] = static_cast<ChipType>(val);
            }
        }
    }
    RebuildChipObjects();
    return true;
}

void MapChip2D::Resize(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) return;

    // 現在のデータを退避させつつ新しいグリッドを生成する
    std::vector<std::vector<ChipType>> newMapData(newHeight, std::vector<ChipType>(newWidth, ChipType::kNone));

    // コピー可能な共通範囲を計算
    int copyHeight = (std::min)(mapHeight_, newHeight);
    int copyWidth = (std::min)(mapWidth_, newWidth);

    // 既存データをコピーする
    for (int y = 0; y < copyHeight; ++y) {
        for (int x = 0; x < copyWidth; ++x) {
            newMapData[y][x] = mapData_[y][x];
        }
    }

    mapData_ = std::move(newMapData);
    mapWidth_ = newWidth;
    mapHeight_ = newHeight;

    // 描画オブジェクトを再構築
    RebuildChipObjects();
}


