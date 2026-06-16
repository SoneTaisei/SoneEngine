#pragma once
#include "GameObject/PrimitiveObject.h"
#include <memory>

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
    }
    
    virtual void Draw(ID3D12GraphicsCommandList* commandList) {
        if (primitiveObj_) {
            primitiveObj_->Draw(commandList);
        }
    }

    // 当たり判定の性質
    virtual bool IsSolid() const { return false; }
    virtual bool IsOneWay() const { return false; }

    // プレイヤーと接触した際の処理
    virtual void OnCollision(Player2D* player) {}

    PrimitiveObject* GetPrimitive() const { return primitiveObj_.get(); }
    
    // 消滅フラグ（コイン取得時など）
    bool IsDestroyed() const { return isDestroyed_; }
    void Destroy() { isDestroyed_ = true; }

protected:
    MapChip2D* map_ = nullptr;
    int chipX_ = 0;
    int chipY_ = 0;
    std::unique_ptr<PrimitiveObject> primitiveObj_;
    bool isDestroyed_ = false;
};
