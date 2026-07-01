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
#include "Blocks/LiftBlock.h"
#include "Blocks/RailBlock.h"
#include "Blocks/JumpBlock.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelManager.h"
#include "Graphics/CameraManager.h"

void MapChip2D::Initialize(ID3D12GraphicsCommandList* commandList, const std::string& mapFilePath) {
    commandList->GetDevice(IID_PPV_ARGS(&device_));

    // デフォルトテクスチャのロード
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> comPtrCommandList(commandList);
    uint32_t texHandle = TextureManager::GetInstance()->Load("resources/Object/School/human/white.png", comPtrCommandList);
    gpuHandle_ = TextureManager::GetInstance()->GetGpuHandle(texHandle);

    // 保存ファイルがあれば読込み、なければ初期構築して保存する
    if (!LoadFromFile(mapFilePath)) {
        BuildMap();
        GenerateDefaultBoundaries();
        SaveToFile(mapFilePath);
    }

    // テンプレートの読み込み（なければデフォルト生成して保存）
    if (!LoadTemplatesFromFile("resources/json/templates_config.json") || templatePalette_.empty()) {
        templatePalette_.clear();
        auto addTemplate = [&](int id, const std::string& name, const std::string& type, Vector4 color, nlohmann::json props) {
            CustomBlockDef def;
            def.id = id;
            def.name = name;
            def.type = type;
            def.color = color;
            def.properties = props;
            templatePalette_.push_back(def);
        };
        addTemplate(1, "Block", "NormalBlock", {0.3f, 0.7f, 0.3f, 1.0f}, nlohmann::json::object());
        addTemplate(2, "Death", "DeathBlock", {1.0f, 0.2f, 0.2f, 1.0f}, nlohmann::json::object());
        addTemplate(3, "Goal", "GoalBlock", {0.8f, 0.2f, 0.8f, 1.0f}, nlohmann::json::object());
        addTemplate(4, "Coin", "CoinBlock", {1.0f, 0.8f, 0.0f, 1.0f}, nlohmann::json::object());
        addTemplate(5, "OneWay", "OneWayBlock", {0.4f, 0.8f, 0.8f, 1.0f}, nlohmann::json::object());
        
        nlohmann::json liftProps;
        liftProps["speed"] = 2.0f;
        liftProps["direction"] = "horizontal";
        liftProps["range"] = 10.0f;
        addTemplate(7, "Lift", "LiftBlock", {0.9f, 0.6f, 0.1f, 1.0f}, liftProps);
        
        addTemplate(8, "Rail", "RailBlock", {0.7f, 0.7f, 0.7f, 1.0f}, nlohmann::json::object());
        
        nlohmann::json jumpProps;
        jumpProps["jumpVelocity"] = 15.0f;
        addTemplate(9, "Jump", "JumpBlock", {1.0f, 0.5f, 0.0f, 1.0f}, jumpProps);

        SaveTemplatesToFile("resources/json/templates_config.json");
    }

    // 古いファイル等で CustomPalette が空の場合、テンプレートのブロックを CustomBlocks として登録する
    if (customPalette_.empty()) {
        for (const auto& def : templatePalette_) {
            CustomBlockDef newDef = def;
            newDef.id = 100 + static_cast<int>(customPalette_.size());
            newDef.name = "Custom " + def.name;
            customPalette_.push_back(newDef);
        }
    }
}

void MapChip2D::Update() {
    if (isDirty_) {
        RebuildChipObjects();
        isDirty_ = false;
    }

    for (auto it = updateBlocks_.begin(); it != updateBlocks_.end();) {
        if (*it) {
            (*it)->Update();
            if ((*it)->IsDestroyed()) {
                // Remove from mapData_ and activeBlocks_
                for (int y = 0; y < mapHeight_; ++y) {
                    for (int x = 0; x < mapWidth_; ++x) {
                        if (activeBlocks_[y][x] == *it) {
                            activeBlocks_[y][x].reset();
                            mapData_[y][x] = ChipType::kNone;
                        }
                    }
                }
                it = updateBlocks_.erase(it);
            } else {
                ++it;
            }
        } else {
            it = updateBlocks_.erase(it);
        }
    }
}

void MapChip2D::Draw(ID3D12GraphicsCommandList* commandList) {
    auto cameraMgr = CameraManager::GetInstance();
    Matrix4x4 vp = TransformFunctions::Multiply(cameraMgr->GetViewMatrix(), cameraMgr->GetProjectionMatrix());
    std::array<Vector4, 6> planes;
    TransformFunctions::ExtractFrustumPlanes(vp, planes);

    for (const auto& block : updateBlocks_) {
        if (block) {
            Vector3 center = {0, 0, 0};
            float radius = 0.0f;
            bool shouldCheck = false;

            if (block->GetObject3D()) {
                center = block->GetObject3D()->GetTranslation();
                Vector3 scale = block->GetObject3D()->GetScale();
                radius = (std::max)({scale.x, scale.y, scale.z}) * 2.5f; // Safe radius
                shouldCheck = true;
            } else if (block->GetPrimitive()) {
                center = block->GetPrimitive()->GetTransform().translate;
                Vector3 scale = block->GetPrimitive()->GetTransform().scale;
                radius = (std::max)({scale.x, scale.y, scale.z}) * 2.0f; // Safe radius
                shouldCheck = true;
            }

            if (shouldCheck) {
                if (!TransformFunctions::IsSphereInFrustum(center, radius, planes)) {
                    continue; // 視錐台の外なので描画スキップ
                }
            }

            block->Draw(commandList);
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
    for (const auto& block : updateBlocks_) {
        if (block) {
            auto* prim = block->GetPrimitive();
            if (prim) {
                result.push_back(prim);
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
        isDirty_ = true;
    }
}

MapChip2D::ChipType MapChip2D::GetChip(int x, int y) const {
    if (x < 0 || x >= mapWidth_ || y < 0 || y >= mapHeight_) return ChipType::kNone;
    return mapData_[y][x];
}

void MapChip2D::BucketFill(int startX, int startY, ChipType targetType, ChipType replacementType) {
    if (startX < 0 || startX >= mapWidth_ || startY < 0 || startY >= mapHeight_) return;
    if (targetType == replacementType) return;
    if (mapData_[startY][startX] != targetType) return;

    std::vector<std::pair<int, int>> queue;
    queue.push_back({startX, startY});

    while (!queue.empty()) {
        auto [x, y] = queue.back();
        queue.pop_back();

        if (x < 0 || x >= mapWidth_ || y < 0 || y >= mapHeight_) continue;
        if (mapData_[y][x] == targetType) {
            SetChip(x, y, replacementType);
            queue.push_back({x + 1, y});
            queue.push_back({x - 1, y});
            queue.push_back({x, y + 1});
            queue.push_back({x, y - 1});
        }
    }
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
    activeBlocks_.resize(mapHeight_, std::vector<std::shared_ptr<BaseBlock>>(mapWidth_, nullptr));
    updateBlocks_.clear();

    std::vector<std::vector<bool>> visited(mapHeight_, std::vector<bool>(mapWidth_, false));

    Primitive* boxPrimitive = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f);

    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (visited[y][x]) continue;

            ChipType type = mapData_[y][x];
            if (type == ChipType::kNone || type == ChipType::kPlayerSpawn) {
                visited[y][x] = true;
                continue;
            }

            int spanWidth = 1;
            int spanHeight = 1;
            int typeId = static_cast<int>(type);
            
            bool canMerge = false;
            if (typeId < 100) {
                canMerge = (type == ChipType::kBlock || type == ChipType::kDeathBlock || type == ChipType::kOneWayBlock);
            } else {
                // カスタムブロックの場合、ベースの型がマージ可能であればマージする
                const CustomBlockDef* def = nullptr;
                for (const auto& d : customPalette_) {
                    if (d.id == typeId) { def = &d; break; }
                }
                if (def) {
                    canMerge = (def->type == "NormalBlock" || def->type == "DeathBlock" || def->type == "OneWayBlock");
                    // モデルが設定されている場合は、引き伸ばされないようにマージを無効化する
                    if (!def->modelName.empty()) {
                        canMerge = false;
                    }
                }
            }

            if (canMerge) {
                // 水平方向のスパンを探索
                while (x + spanWidth < mapWidth_ && mapData_[y][x + spanWidth] == type && !visited[y][x + spanWidth]) {
                    spanWidth++;
                }

                // 垂直方向のスパンを探索
                bool canExpandUp = true;
                while (y + spanHeight < mapHeight_ && canExpandUp) {
                    // 追加する行が全て同じ種類で未訪問かチェック
                    for (int w = 0; w < spanWidth; ++w) {
                        if (mapData_[y + spanHeight][x + w] != type || visited[y + spanHeight][x + w]) {
                            canExpandUp = false;
                            break;
                        }
                    }
                    if (canExpandUp) {
                        spanHeight++;
                    }
                }
            }

            std::shared_ptr<BaseBlock> newBlock = InstantiateBlock(x, y, type, spanWidth, spanHeight, boxPrimitive);
            if (newBlock) {
                updateBlocks_.push_back(newBlock);

                for (int h = 0; h < spanHeight; ++h) {
                    for (int w = 0; w < spanWidth; ++w) {
                        activeBlocks_[y + h][x + w] = newBlock;
                        visited[y + h][x + w] = true;
                    }
                }
            } else {
                visited[y][x] = true;
            }

            x += spanWidth - 1;
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

    // JSON文字列として保存
    std::string data = GetMapDataAsString();
    ofs << data;
    ofs.close();

    // 境界線メタデータの保存
    std::string boundsPath = filepath;
    size_t lastDot = boundsPath.find_last_of(".");
    if (lastDot != std::string::npos) {
        boundsPath = boundsPath.substr(0, lastDot) + "_bounds.txt";
    } else {
        boundsPath += "_bounds.txt";
    }
    SaveBoundariesToFile(boundsPath);

    return true;
}

bool MapChip2D::LoadFromStageName(const std::string& stageName) {
    std::string name = stageName;
    bool hasExt = false;
    if (name.length() >= 4) {
        std::string ext = name.substr(name.length() - 4);
        if (ext == ".txt" || ext == ".TXT") hasExt = true;
    }
    if (!hasExt) name += ".txt";
    std::string filepath = "resources/json/MapData/" + name;
    return LoadFromFile(filepath);
}

bool MapChip2D::LoadFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    bool result = LoadFromString(buffer.str());

    if (result) {
        // 境界線メタデータの読み込み
        std::string boundsPath = filepath;
        size_t lastDot = boundsPath.find_last_of(".");
        if (lastDot != std::string::npos) {
            boundsPath = boundsPath.substr(0, lastDot) + "_bounds.txt";
        } else {
            boundsPath += "_bounds.txt";
        }
        
        if (!LoadBoundariesFromFile(boundsPath)) {
            // ファイルがなければ自動生成
            GenerateDefaultBoundaries();
        }
    }
    
    return result;
}

std::string MapChip2D::GetMapDataAsString() const {
    nlohmann::json j;
    j["mapWidth"] = mapWidth_;
    j["mapHeight"] = mapHeight_;
    
    std::vector<std::vector<int>> terrain(mapHeight_, std::vector<int>(mapWidth_, 0));
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            terrain[y][x] = static_cast<int>(mapData_[y][x]);
        }
    }
    j["terrain"] = terrain;
    
    nlohmann::json paletteArray = nlohmann::json::array();
    for (const auto& def : customPalette_) {
        nlohmann::json p;
        p["id"] = def.id;
        p["name"] = def.name;
        p["type"] = def.type;
        p["color"] = {{"r", def.color.x}, {"g", def.color.y}, {"b", def.color.z}, {"a", def.color.w}};
        p["scale"] = {{"x", def.scale.x}, {"y", def.scale.y}, {"z", def.scale.z}};
        p["modelName"] = def.modelName;
        p["properties"] = def.properties;
        paletteArray.push_back(p);
    }
    j["customPalette"] = paletteArray;
    
    return j.dump();
}

bool MapChip2D::LoadFromString(const std::string& data) {
    if (data.empty()) return false;
    
    try {
        nlohmann::json j = nlohmann::json::parse(data);
        if (j.contains("mapWidth") && j.contains("mapHeight")) {
            mapWidth_ = j["mapWidth"];
            mapHeight_ = j["mapHeight"];
        }
        
        mapData_.assign(mapHeight_, std::vector<ChipType>(mapWidth_, ChipType::kNone));
        if (j.contains("terrain")) {
            auto terrain = j["terrain"];
            for (int y = 0; y < mapHeight_ && y < terrain.size(); ++y) {
                auto row = terrain[y];
                for (int x = 0; x < mapWidth_ && x < row.size(); ++x) {
                    mapData_[y][x] = static_cast<ChipType>(row[x].get<int>());
                }
            }
        }
        
        // customPalette_.clear(); // クリアせず統合する
        if (j.contains("customPalette")) {
            auto paletteArray = j["customPalette"];
            for (const auto& p : paletteArray) {
                CustomBlockDef def;
                if (p.contains("id")) def.id = p["id"];
                if (p.contains("name")) def.name = p["name"];
                if (p.contains("type")) def.type = p["type"];
                if (p.contains("color")) {
                    def.color.x = p["color"]["r"];
                    def.color.y = p["color"]["g"];
                    def.color.z = p["color"]["b"];
                    def.color.w = p["color"]["a"];
                }
                if (p.contains("scale")) {
                    def.scale.x = p["scale"]["x"];
                    def.scale.y = p["scale"]["y"];
                    def.scale.z = p["scale"]["z"];
                }
                if (p.contains("modelName")) def.modelName = p["modelName"];
                if (p.contains("properties")) def.properties = p["properties"];
                
                bool found = false;
                for (auto& existing : customPalette_) {
                    if (existing.id == def.id) {
                        existing = def;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    customPalette_.push_back(def);
                }
            }
        }
        
        RebuildChipObjects();
        return true;
    } catch (const nlohmann::json::parse_error&) {
        // フォールバック（古いテキスト形式の読み込み）
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
        // customPalette_.clear(); // 古い形式のテキスト読み込みでもクリアしない
        RebuildChipObjects();
        return true;
    }
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

void MapChip2D::GenerateDefaultBoundaries() {
    boundaryX_.clear();
    boundaryY_.clear();

    float roomWidth = 20.0f;
    float roomHeight = 11.25f;

    for (float rx = 0.0f; rx <= static_cast<float>(mapWidth_); rx += roomWidth) {
        boundaryX_.push_back(rx);
    }
    for (float ry = 0.0f; ry <= static_cast<float>(mapHeight_); ry += roomHeight) {
        boundaryY_.push_back(ry);
    }
}

bool MapChip2D::SaveBoundariesToFile(const std::string& filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << boundaryX_.size() << "\n";
    for (size_t i = 0; i < boundaryX_.size(); ++i) {
        ofs << boundaryX_[i] << (i < boundaryX_.size() - 1 ? " " : "");
    }
    ofs << "\n";

    ofs << boundaryY_.size() << "\n";
    for (size_t i = 0; i < boundaryY_.size(); ++i) {
        ofs << boundaryY_[i] << (i < boundaryY_.size() - 1 ? " " : "");
    }
    ofs << "\n";

    ofs.close();
    return true;
}

bool MapChip2D::LoadBoundariesFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    size_t xSize = 0, ySize = 0;
    if (!(ifs >> xSize)) return false;

    boundaryX_.clear();
    for (size_t i = 0; i < xSize; ++i) {
        float val;
        if (ifs >> val) boundaryX_.push_back(val);
    }

    if (!(ifs >> ySize)) return false;

    boundaryY_.clear();
    for (size_t i = 0; i < ySize; ++i) {
        float val;
        if (ifs >> val) boundaryY_.push_back(val);
    }

    ifs.close();
    return true;
}

bool MapChip2D::SaveTemplatesToFile(const std::string& filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    
    nlohmann::json j;
    auto templatesArray = nlohmann::json::array();
    for (const auto& def : templatePalette_) {
        nlohmann::json p;
        p["id"] = def.id;
        p["name"] = def.name;
        p["type"] = def.type;
        p["color"] = {{"r", def.color.x}, {"g", def.color.y}, {"b", def.color.z}, {"a", def.color.w}};
        p["scale"] = {{"x", def.scale.x}, {"y", def.scale.y}, {"z", def.scale.z}};
        p["modelName"] = def.modelName;
        p["properties"] = def.properties;
        templatesArray.push_back(p);
    }
    j["templates"] = templatesArray;

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;
    ofs << j.dump(4);
    ofs.close();
    return true;
}

bool MapChip2D::LoadTemplatesFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    try {
        nlohmann::json j;
        ifs >> j;
        
        templatePalette_.clear();
        if (j.contains("templates")) {
            auto paletteArray = j["templates"];
            for (const auto& p : paletteArray) {
                CustomBlockDef def;
                if (p.contains("id")) def.id = p["id"];
                if (p.contains("name")) def.name = p["name"];
                if (p.contains("type")) def.type = p["type"];
                if (p.contains("color")) {
                    def.color.x = p["color"]["r"];
                    def.color.y = p["color"]["g"];
                    def.color.z = p["color"]["b"];
                    def.color.w = p["color"]["a"];
                }
                if (p.contains("scale")) {
                    def.scale.x = p["scale"]["x"];
                    def.scale.y = p["scale"]["y"];
                    def.scale.z = p["scale"]["z"];
                }
                if (p.contains("modelName")) def.modelName = p["modelName"];
                if (p.contains("properties")) def.properties = p["properties"];
                templatePalette_.push_back(def);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}


std::shared_ptr<BaseBlock> MapChip2D::InstantiateBlock(int x, int y, ChipType type, int spanWidth, int spanHeight, Primitive* boxPrimitive) {
    float worldX = ChipToWorldX(x) + (spanWidth * chipSize_) * 0.5f;
    float worldY = ChipToWorldY(y) + (spanHeight * chipSize_) * 0.5f;

    std::shared_ptr<BaseBlock> newBlock = nullptr;
    int typeId = static_cast<int>(type);

    if (type == ChipType::kBlock) {
        newBlock = std::make_shared<NormalBlock>(this, x, y);
    } else if (type == ChipType::kDeathBlock) {
        newBlock = std::make_shared<DeathBlock>(this, x, y);
    } else if (type == ChipType::kGoal) {
        newBlock = std::make_shared<GoalBlock>(this, x, y);
    } else if (type == ChipType::kCoin) {
        newBlock = std::make_shared<CoinBlock>(this, x, y);
    } else if (type == ChipType::kOneWayBlock) {
        newBlock = std::make_shared<OneWayBlock>(this, x, y);
    } else if (type == ChipType::kLift) {
        newBlock = std::make_shared<LiftBlock>(this, x, y);
    } else if (type == ChipType::kRail) {
        newBlock = std::make_shared<RailBlock>(this, x, y);
    } else if (type == ChipType::kJumpBlock) {
        newBlock = std::make_shared<JumpBlock>(this, x, y);
    } else if (typeId >= 100) {
        const CustomBlockDef* def = nullptr;
        for (const auto& d : customPalette_) {
            if (d.id == typeId) { def = &d; break; }
        }
        if (def) {
            if (def->type == "NormalBlock") newBlock = std::make_shared<NormalBlock>(this, x, y);
            else if (def->type == "DeathBlock") newBlock = std::make_shared<DeathBlock>(this, x, y);
            else if (def->type == "GoalBlock") newBlock = std::make_shared<GoalBlock>(this, x, y);
            else if (def->type == "CoinBlock") newBlock = std::make_shared<CoinBlock>(this, x, y);
            else if (def->type == "OneWayBlock") newBlock = std::make_shared<OneWayBlock>(this, x, y);
            else if (def->type == "LiftBlock") newBlock = std::make_shared<LiftBlock>(this, x, y);
            else if (def->type == "RailBlock") newBlock = std::make_shared<RailBlock>(this, x, y);
            else if (def->type == "JumpBlock") newBlock = std::make_shared<JumpBlock>(this, x, y);
        }
    }

    if (newBlock) {
        newBlock->Initialize(device_.Get(), boxPrimitive, worldX, worldY, spanWidth * chipSize_, spanHeight * chipSize_);
        
        const CustomBlockDef* def = nullptr;
        if (typeId >= 100) {
            for (const auto& d : customPalette_) {
                if (d.id == typeId) { def = &d; break; }
            }
        } else if (typeId >= 1 && typeId <= 9) {
            for (const auto& d : templatePalette_) {
                if (d.id == typeId) { def = &d; break; }
            }
        }
        
        if (def) {
            newBlock->SetProperties(def->properties);
            if (newBlock->GetPrimitive()) {
                newBlock->GetPrimitive()->GetMaterial().color = def->color;
                
                const Transform& t = newBlock->GetPrimitive()->GetTransform();
                newBlock->GetPrimitive()->SetScale({ 
                    t.scale.x * def->scale.x, 
                    t.scale.y * def->scale.y, 
                    t.scale.z * def->scale.z 
                });
            }
            
            if (!def->modelName.empty()) {
                Model* model = nullptr;
                if (def->modelName.length() >= 4 && def->modelName.substr(def->modelName.length() - 4) == ".obj") {
                    std::string fullPath = "resources/" + def->modelName;
                    std::filesystem::path p(fullPath);
                    std::string dirPath = p.parent_path().string();
                    std::string fileName = p.filename().string();
                    std::replace(dirPath.begin(), dirPath.end(), '\\', '/');
                    model = ModelManager::GetInstance()->GetModel(dirPath, fileName);
                } else {
                    model = ModelManager::GetInstance()->GetModel("resources/Object/School/" + def->modelName, def->modelName + ".obj");
                    if (!model) {
                        model = ModelManager::GetInstance()->GetModel("resources/models", def->modelName + ".obj");
                    }
                }
                
                if (model) {
                    auto obj = std::make_unique<Object3D>();
                    obj->Initialize(device_.Get(), model);
                    obj->SetTranslation({ worldX, worldY, 0.0f });
                    obj->SetScale(def->scale);
                    obj->SetTextureHandle(gpuHandle_);
                    obj->GetMaterial().color = def->color;
                    newBlock->SetObject3D(std::move(obj));
                }
            }
        }

        if (newBlock->GetPrimitive()) {
            newBlock->GetPrimitive()->SetName("MapChip_" + std::to_string(x) + "_" + std::to_string(y));
            if (spanWidth > 1 || spanHeight > 1) {
                Matrix4x4 uvTrans = TransformFunctions::MakeScaleMatrix({(float)spanWidth, (float)spanHeight, 1.0f});
                newBlock->GetPrimitive()->GetMaterial().uvTransform = uvTrans;
            }
        }
    }
    return newBlock;
}
