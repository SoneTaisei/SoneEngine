#include "MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/TextureManager.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <algorithm>

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
    for (auto& obj : chipObjects_) {
        obj->Update();
    }
}

void MapChip2D::Draw(ID3D12GraphicsCommandList* commandList) {
    for (auto& obj : chipObjects_) {
        obj->Draw(commandList);
    }
}

bool MapChip2D::IsBlock(int chipX, int chipY) const {
    // 範囲外はブロック扱い（壁として機能させる）
    if (chipX < 0 || chipX >= mapWidth_ || chipY < 0) {
        return true;
    }
    // 上方向に範囲外は空気
    if (chipY >= mapHeight_) {
        return false;
    }
    return mapData_[chipY][chipX] == ChipType::kBlock || mapData_[chipY][chipX] == ChipType::kDeathBlock;
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
    for (auto& obj : chipObjects_) {
        result.push_back(obj.get());
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
}

void MapChip2D::CreateChipObjects(ID3D12GraphicsCommandList* commandList) {
    RebuildChipObjects();
}

void MapChip2D::SetChip(int x, int y, ChipType type) {
    if (x < 0 || x >= mapWidth_ || y < 0 || y >= mapHeight_) return;
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
    if (!device_) return;

    chipObjects_.clear();

    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);

    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (mapData_[y][x] == ChipType::kBlock || mapData_[y][x] == ChipType::kDeathBlock) {
                auto obj = std::make_unique<PrimitiveObject>();
                obj->Initialize(device_.Get(), boxPrimitive);
                
                // テクスチャの設定
                obj->SetTextureHandle(gpuHandle_);

                // チップの中心座標を計算
                float worldX = x * chipSize_ + chipSize_ * 0.5f;
                float worldY = y * chipSize_ + chipSize_ * 0.5f;

                obj->SetTranslation({ worldX, worldY, 0.0f });
                obj->SetScale({ chipSize_, chipSize_, chipSize_ });

                // 地面と壁とデスブロックで色を変える
                if (mapData_[y][x] == ChipType::kDeathBlock) {
                    // デスブロック：赤色
                    obj->GetMaterial().color = { 1.0f, 0.2f, 0.2f, 1.0f };
                } else if (y <= 1) {
                    // 地面：茶色
                    obj->GetMaterial().color = { 0.55f, 0.35f, 0.17f, 1.0f };
                } else if (x == 0 || x == mapWidth_ - 1) {
                    // 壁：灰色
                    obj->GetMaterial().color = { 0.5f, 0.5f, 0.55f, 1.0f };
                } else {
                    // その他のブロック：緑
                    obj->GetMaterial().color = { 0.3f, 0.7f, 0.3f, 1.0f };
                }

                // ライティング無効（2Dなので）
                obj->GetMaterial().lightingType = 0;

                obj->SetName("MapChip_" + std::to_string(x) + "_" + std::to_string(y));
                obj->Update();

                chipObjects_.push_back(std::move(obj));
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

    int width = 0;
    int height = 0;
    if (!(ifs >> width >> height)) return false;

    mapWidth_ = width;
    mapHeight_ = height;

    mapData_.clear();
    mapData_.resize(mapHeight_, std::vector<ChipType>(mapWidth_, ChipType::kNone));

    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            int val = 0;
            if (ifs >> val) {
                mapData_[y][x] = static_cast<ChipType>(val);
            }
        }
    }
    ifs.close();

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


