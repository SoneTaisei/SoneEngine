#pragma once
#include "GameObject/PrimitiveObject.h"
#include <memory>
#include <nlohmann/json.hpp>
#include "GameObject/Object3D.h"
#include "Core/Utility/Structs.h"
class Player2D;
class MapChip2D;
struct ID3D12Device;
class Primitive;

class BaseBlock {
public:
    BaseBlock(MapChip2D* map, int chipX, int chipY) 
        : map_(map), chipX_(chipX), chipY_(chipY) {}
    virtual ~BaseBlock() = default;

    virtual void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {}
    
    virtual void Update() {
        if (primitiveObj_) {
            primitiveObj_->Update();
        }
        if (object3D_) {
            object3D_->Update();
        }
    }
    
    virtual void Draw(ID3D12GraphicsCommandList* commandList) {
        if (object3D_) {
            object3D_->Draw(commandList);
        } else if (primitiveObj_) {
            primitiveObj_->Draw(commandList);
        }
    }

    // 当たり判定の性質
    virtual bool IsSolid() const { return false; }
    virtual bool IsOneWay() const { return false; }
    
    // リフトなどの動く足場用
    virtual bool IsMoving() const { return false; }
    virtual Vector3 GetVelocity() const { return {0.0f, 0.0f, 0.0f}; }

    // プレイヤーと接触した際の処理
    virtual void OnCollision(Player2D* player) {}
    
    // プレイヤーが上に乗った際の処理
    virtual void OnPlayerStand() {}

    // Jsonプロパティの受け取り
    virtual void SetProperties(const nlohmann::json& properties) {}

    PrimitiveObject* GetPrimitive() const { return primitiveObj_.get(); }
    Object3D* GetObject3D() const { return object3D_.get(); }
    void SetObject3D(std::unique_ptr<Object3D> obj) { object3D_ = std::move(obj); }
    
    // 消滅フラグ（コイン取得時など）
    bool IsDestroyed() const { return isDestroyed_; }
    void Destroy() { isDestroyed_ = true; }

    AABB2D GetAABB() const {
        Vector3 pos = {0.0f, 0.0f, 0.0f};
        Vector3 scale = {1.0f, 1.0f, 1.0f};
        if (object3D_) {
            pos = object3D_->GetTranslation();
            scale = object3D_->GetScale();
        } else if (primitiveObj_) {
            pos = primitiveObj_->GetTranslation();
            scale = primitiveObj_->GetScale();
        }
        return {
            pos.x - scale.x * 0.5f,
            pos.y + scale.y * 0.5f,
            pos.x + scale.x * 0.5f,
            pos.y - scale.y * 0.5f
        };
    }

protected:
    MapChip2D* map_ = nullptr;
    int chipX_ = 0;
    int chipY_ = 0;
    std::unique_ptr<PrimitiveObject> primitiveObj_;
    std::unique_ptr<Object3D> object3D_;
    bool isDestroyed_ = false;
};
