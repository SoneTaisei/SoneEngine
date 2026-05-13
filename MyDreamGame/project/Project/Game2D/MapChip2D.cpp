#include "MapChip2D.h"
#include "Core/Utility/TransformFunctions.h"

#include "Graphics/TextureManager.h"

void MapChip2D::Initialize(ID3D12GraphicsCommandList* commandList) {
    // マップデータを構築
    BuildMap();

    // チップごとの描画オブジェクトを生成
    CreateChipObjects(commandList);
}

void MapChip2D::Update() {
    // マップチップは動かないので特に何もしない
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
    return mapData_[chipY][chipX] == ChipType::kBlock;
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

    // --- 穴を作る（x=8〜9は地面なし）---
    mapData_[0][8] = ChipType::kNone;
    mapData_[1][8] = ChipType::kNone;
    mapData_[0][9] = ChipType::kNone;
    mapData_[1][9] = ChipType::kNone;

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
}

void MapChip2D::CreateChipObjects(ID3D12GraphicsCommandList* commandList) {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);

    // デフォルトテクスチャのロード
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> comPtrCommandList(commandList);
    uint32_t texHandle = TextureManager::GetInstance()->Load("Object/School/human/white.png", comPtrCommandList);
    auto gpuHandle = TextureManager::GetInstance()->GetGpuHandle(texHandle);

    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (mapData_[y][x] == ChipType::kBlock) {
                auto obj = std::make_unique<PrimitiveObject>();
                obj->Initialize(device.Get(), boxPrimitive);
                
                // テクスチャの設定
                obj->SetTextureHandle(gpuHandle);

                // チップの中心座標を計算
                float worldX = x * chipSize_ + chipSize_ * 0.5f;
                float worldY = y * chipSize_ + chipSize_ * 0.5f;

                obj->SetTranslation({ worldX, worldY, 0.0f });
                obj->SetScale({ chipSize_, chipSize_, chipSize_ });

                // 地面と壁で色を変える
                if (y <= 1) {
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
