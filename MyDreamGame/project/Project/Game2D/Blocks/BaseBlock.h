#pragma once
#include "GameObject/PrimitiveObject.h"
#include <memory>
#include <vector>
#include <cstdint>
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
    virtual void OnPlayerStand(Player2D* player) { OnPlayerStand(); }
    
    // プレイヤーが横などから接触した際の処理
    virtual void OnPlayerTouch() {}
    virtual void OnPlayerTouch(Player2D* player) { OnPlayerTouch(); }

    // Jsonプロパティの受け取り
    virtual void SetProperties(const nlohmann::json& properties) {}

    // リセット処理（プレイヤー死亡時・リトライ時等）
    virtual void Reset() {}

    // ===== リプレイ対応 =====
    // リプレイに毎フレーム状態を記録する対象かどうか。
    // 位置・回転・スケール・色・破壊フラグの変化は MapChip2D 側が自動で検出して
    // 記録対象に加えるため、新しいブロックを追加しても基本的に何もしなくてよい。
    // 「見た目は変わらないが内部状態を持つ」ブロックだけ、これを true にする。
    virtual bool IsReplayTracked() const { return IsMoving(); }

    // 派生ブロック固有の内部状態（タイマー・開閉率など）を float 列に詰める。
    // 位置・回転・スケール・色・破壊フラグは共通で保存されるので、ここには含めなくてよい。
    virtual void CaptureReplayState(std::vector<float>& outCustom) const { outCustom.clear(); }

    // CaptureReplayState で詰めた内容から内部状態を復元する。
    virtual void RestoreReplayState(const std::vector<float>& custom) { (void)custom; }

    // リプレイ上でブロックを一意に識別するID（配置チップ座標から生成する）。
    // マップは録画時の状態から復元されるため、同じ座標のブロックは必ず同じIDになる。
    virtual uint64_t GetReplayObjectId() const {
        return (static_cast<uint64_t>(static_cast<uint32_t>(chipX_) & 0xFFFFu) << 16) |
               (static_cast<uint32_t>(chipY_) & 0xFFFFu);
    }

#ifdef USE_IMGUI
    // ImGuiによるブロックパラメータの調整やデバッグ操作用UI
    virtual void DrawImGui() {}
#endif

    GameObject* GetGameObject() const { return gameObject_.get(); }
    void SetGameObject(std::unique_ptr<GameObject> obj) { gameObject_ = std::move(obj); }
    
    // 消滅フラグ（コイン取得時など）
    bool IsDestroyed() const { return isDestroyed_; }
    void Destroy() { isDestroyed_ = true; }
    void SetDestroyed(bool destroyed) { isDestroyed_ = destroyed; }

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
