#pragma once
#include "Component/IComponent.h"
#include "Core/Utility/Structs.h"
#include <functional>

enum ColliderLayer : uint32_t {
    kLayerPlayer  = 1 << 0,
    kLayerBlock   = 1 << 1,
    kLayerItem    = 1 << 2,
    kLayerTrigger = 1 << 3,
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
    void DisplayImGui() override;

    // 形状種別の取得（仮想関数）
    virtual ColliderType GetColliderType() const { return type_; }
    void SetColliderType(ColliderType type) { type_ = type; }

    // 当たり判定AABBの取得（ポリモーフィズム用仮想関数）
    virtual AABB2D GetAABB() const;

    // --- Box Collider 仮想アクセサ（派生クラスでオーバーライド） ---
    virtual void SetBoxSize(const Vector3& size) { boxSize_ = size; }
    virtual void SetBoxOffset(const Vector3& offset) { boxOffset_ = offset; }
    virtual Vector3 GetBoxSize() const { return boxSize_; }
    virtual Vector3 GetBoxOffset() const { return boxOffset_; }

    // --- Sphere Collider 仮想アクセサ（派生クラスでオーバーライド） ---
    virtual void SetSphereRadius(float radius) { sphereRadius_ = radius; }
    virtual void SetSphereOffset(const Vector3& offset) { sphereOffset_ = offset; }
    virtual float GetSphereRadius() const { return sphereRadius_; }
    virtual Vector3 GetSphereOffset() const { return sphereOffset_; }

    // --- 共通の物理プロパティ ---
    void SetLayerMask(uint32_t layer) { layerMask_ = layer; }
    uint32_t GetLayerMask() const { return layerMask_; }

    void SetIsSolid(bool solid) { isSolid_ = solid; }
    bool IsSolid() const { return isSolid_; }

    void SetIsOneWay(bool oneWay) { isOneWay_ = oneWay; }
    bool IsOneWay() const { return isOneWay_; }

    void SetIsMoving(bool moving) { isMoving_ = moving; }
    bool IsMoving() const { return isMoving_; }

    void SetVelocity(const Vector3& vel) { velocity_ = vel; }
    Vector3 GetVelocity() const { return velocity_; }

    // Custom Fixed AABB
    void SetCustomAABB(const AABB2D& aabb) { customAABB_ = aabb; hasCustomAABB_ = true; }
    void ClearCustomAABB() { hasCustomAABB_ = false; }
    bool HasCustomAABB() const { return hasCustomAABB_; }
    const AABB2D& GetCustomAABB() const { return customAABB_; }

    void SetUserData(void* data) { userData_ = data; }
    void* GetUserData() const { return userData_; }

    // Callbacks
    using OnCollisionCallback = std::function<void(ColliderComponent*)>;
    void SetOnCollision(OnCollisionCallback callback) { onCollision_ = callback; }
    void SetOnPlayerStand(OnCollisionCallback callback) { onPlayerStand_ = callback; }

    void OnCollision(ColliderComponent* other) { if (onCollision_) onCollision_(other); }
    void OnPlayerStand(ColliderComponent* other) { if (onPlayerStand_) onPlayerStand_(other); }

protected:
    ColliderType type_ = ColliderType::kBox;

    Vector3 boxSize_{ 1.0f, 1.0f, 1.0f };
    Vector3 boxOffset_{ 0.0f, 0.0f, 0.0f };

    float sphereRadius_ = 1.0f;
    Vector3 sphereOffset_{ 0.0f, 0.0f, 0.0f };

    uint32_t layerMask_ = 0xFFFFFFFF;
    bool isSolid_ = true;
    bool isOneWay_ = false;
    bool isMoving_ = false;
    Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
    void* userData_ = nullptr;

    bool hasCustomAABB_ = false;
    AABB2D customAABB_{};

    OnCollisionCallback onCollision_ = nullptr;
    OnCollisionCallback onPlayerStand_ = nullptr;
};

#include "ColliderComponent/BoxColliderComponent.h"
#include "ColliderComponent/SphereColliderComponent.h"
