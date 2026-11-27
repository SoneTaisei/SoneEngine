#pragma once
#include "Component/IComponent.h"
#include "Core/Utility/Structs.h"
#include <functional>

class LineRenderer;

enum ColliderLayer : uint32_t {
    kLayerPlayer = 1 << 0,
    kLayerBlock = 1 << 1,
    kLayerItem = 1 << 2,
    kLayerTrigger = 1 << 3,
    kLayerPlayerHead = 1 << 4,
};

enum class ColliderType {
    kAABB,
    kOBB,
    kSphere,
    kBox = kAABB // 後方互換用エイリアス
};

/// <summary>
/// コライダーコンポーネント基底クラス（ポリモーフィズム用）
/// </summary>
class ColliderComponent : public IComponent {
public:
    ColliderComponent();
    ~ColliderComponent() override; // 仮想デストラクタ

    void Initialize() override;
    void Update() override;
    void DisplayImGui() override = 0;

    // 形状種別の取得（純粋仮想関数）
    virtual ColliderType GetColliderType() const = 0;
    // 当たり判定AABBの取得（純粋仮想関数）
    virtual AABB2D GetAABB() const = 0;

    // --- 共通の物理プロパティ ---
    void SetLayerMask(uint32_t layer) { layerMask_ = layer; }
    uint32_t GetLayerMask() const { return layerMask_; }

    void SetIsSolid(bool solid) { isSolid_ = solid; }
    bool IsSolid() const { return isSolid_; }

    void SetIsOneWay(bool oneWay) { isOneWay_ = oneWay; }
    bool IsOneWay() const { return isOneWay_; }

    void SetIsMoving(bool moving) { isMoving_ = moving; }
    bool IsMoving() const { return isMoving_; }

    void SetVelocity(const Vector3 &vel) { velocity_ = vel; }
    Vector3 GetVelocity() const { return velocity_; }

    // Custom Fixed AABB
    void SetCustomAABB(const AABB2D &aabb) {
        customAABB_ = aabb;
        hasCustomAABB_ = true;
    }
    void ClearCustomAABB() { hasCustomAABB_ = false; }
    bool HasCustomAABB() const { return hasCustomAABB_; }
    const AABB2D &GetCustomAABB() const { return customAABB_; }

    void SetUserData(void *data) { userData_ = data; }
    void *GetUserData() const { return userData_; }

    virtual void DrawDebug(LineRenderer *lineRenderer, const Vector4 &color) const = 0;

private:
    uint32_t layerMask_ = 0xFFFFFFFF;
    bool isSolid_ = true;
    bool isOneWay_ = false;
    bool isMoving_ = false;
    Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
    void *userData_ = nullptr;

    bool hasCustomAABB_ = false;
    AABB2D customAABB_{};
};

#include "ColliderComponent/BoxColliderComponent.h"
#include "ColliderComponent/SphereColliderComponent.h"
