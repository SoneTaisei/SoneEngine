#pragma once
#include "GameObject/PrimitiveObject.h"
#include <memory>
#include <nlohmann/json.hpp>
#include "GameObject/Object3D.h"
#include "Core/Utility/Structs.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#include "Component/PrimitiveRendererComponent.h"
#include "Component/MeshRendererComponent.h"
#include "Component/ColliderComponent.h"
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
        if (!isDestroyed_ && gameObject_) {
            gameObject_->Update();
        }
    }
    
    virtual void Draw() {
        if (!isDestroyed_ && gameObject_) {
            gameObject_->Draw(); // Rendererへの登録が行われる
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
    
    virtual void OnPlayerStand() {}
    
    // プレイヤーが横などから接触した際の処理
    virtual void OnPlayerTouch() {}

    // Jsonプロパティの受け取り
    virtual void SetProperties(const nlohmann::json& properties) {}

    // リセット処理（プレイヤー死亡時・リトライ時等）
    virtual void Reset() {}

#ifdef USE_IMGUI
    // ImGuiによるブロックパラメータの調整やデバッグ操作用UI
    virtual void DrawImGui() {}
#endif

    GameObject* GetGameObject() const { return gameObject_.get(); }
    void SetGameObject(std::unique_ptr<GameObject> obj) { gameObject_ = std::move(obj); }
    
    // 消滅フラグ（コイン取得時など）
    bool IsDestroyed() const { return isDestroyed_; }
    void Destroy() { isDestroyed_ = true; }

    void SetupCollider() {
        if (!gameObject_) return;
        auto* cc = gameObject_->AddComponent<ColliderComponent>();
        cc->SetLayerMask(kLayerBlock);
        cc->SetIsSolid(IsSolid());
        cc->SetIsOneWay(IsOneWay());
        cc->SetIsMoving(IsMoving());
        cc->SetVelocity(GetVelocity());
        cc->SetUserData(this);
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            cc->SetBoxSize({tc->GetScale().x, tc->GetScale().y, tc->GetScale().z});
            cc->SetBoxSize({1.0f, 1.0f, 1.0f}); // TransformComponentのスケールが反映されるので1.0でOK
        }
    }

    AABB2D GetAABB() const {
        Vector3 pos = {0.0f, 0.0f, 0.0f};
        Vector3 scale = {1.0f, 1.0f, 1.0f};
        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                pos = tc->GetPosition();
                scale = tc->GetScale();
            }
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
    std::unique_ptr<GameObject> gameObject_;
    bool isDestroyed_ = false;
};
