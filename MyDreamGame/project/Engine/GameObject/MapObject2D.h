#pragma once
#include <string>
#include "Core/Utility/Structs.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <d3d12.h>

class BaseBlock;
class MapChip2D;
class Primitive;

class MapObject2D {
public:
    MapObject2D() = default;
    ~MapObject2D() = default;

    std::string name = "New Object";
    std::string type = "JumpBlock";
    
    // 自由配置用のトランスフォーム
    Vector3 position = {0.0f, 0.0f, 0.0f};
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    
    // JSONでのプロパティ
    nlohmann::json properties = nlohmann::json::object();

    // 内部に持つ実際のブロックロジック
    std::shared_ptr<BaseBlock> blockLogic;

    void DisplayImGui();
    
    // typeに応じてpropertiesのデフォルト値を入れる
    void SetupDefaultProperties();

    // ブロックロジックの生成・初期化
    void InitializeLogic(MapChip2D* map, ID3D12Device* device, Primitive* boxPrimitive);
    void Update();
    void Draw(ID3D12GraphicsCommandList* commandList);

    // jsonからロード / jsonへセーブ
    void LoadFromJson(const nlohmann::json& j);
    nlohmann::json SaveToJson() const;
};
