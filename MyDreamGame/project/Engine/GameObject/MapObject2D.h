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

    // Getters
    const std::string& GetName() const { return name_; }
    const std::string& GetType() const { return type_; }
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetScale() const { return scale_; }
    const nlohmann::json& GetProperties() const { return properties_; }
    std::shared_ptr<BaseBlock> GetBlockLogic() const { return blockLogic_; }

    // Setters
    void SetName(const std::string& name) { name_ = name; }
    void SetType(const std::string& type) { type_ = type; }
    void SetPosition(const Vector3& position) { position_ = position; }
    void SetScale(const Vector3& scale) { scale_ = scale; }
    void SetProperties(const nlohmann::json& properties) { properties_ = properties; }

    void DisplayImGui();
    
    // typeに応じてpropertiesのデフォルト値を入れる
    void SetupDefaultProperties();

    // ブロックロジックの生成・初期化
    void InitializeLogic(MapChip2D* map, ID3D12Device* device, Primitive* boxPrimitive);
    void Update();
    void Draw();

    // jsonからロード / jsonへセーブ
    void LoadFromJson(const nlohmann::json& j);
    nlohmann::json SaveToJson() const;

private:
    std::string name_ = "New Object";
    std::string type_ = "JumpBlock";
    
    // 自由配置用のトランスフォーム
    Vector3 position_ = {0.0f, 0.0f, 0.0f};
    Vector3 scale_ = {1.0f, 1.0f, 1.0f};
    
    // JSONでのプロパティ
    nlohmann::json properties_ = nlohmann::json::object();

    // 内部に持つ実際のブロックロジック
    std::shared_ptr<BaseBlock> blockLogic_;
};
