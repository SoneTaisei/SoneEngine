#include "CollectibleBlock.h"
#include "Game2D/CollectibleTracker.h"
#include "Game2D/Player/Player2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include "Core/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
    // お宝と同じ宝石モデルを小さく・青くして使う
    constexpr const char* kModelDir = "resources/Object/Original/jewelry";
    constexpr const char* kModelFile = "jewelry.obj";
    constexpr float kGemScale = 0.14f;
    constexpr Vector4 kGemColor = { 0.45f, 0.8f, 1.0f, 1.0f };
    constexpr Vector4 kFlashColor = { 1.0f, 1.0f, 0.9f, 1.0f };
    // 以前のクリアで取った宝石は薄く
    constexpr float kGhostAlpha = 0.35f;
    // ゆらゆら（上下の幅と速さ）と自転の速さ
    constexpr float kBobAmp = 0.08f;
    constexpr float kBobSpeed = 2.4f;
    constexpr float kSpinSpeed = 1.6f;
    // 取った後の演出：秒数、上がる高さ、大きくなる倍率
    constexpr float kCollectAnim = 0.35f;
    constexpr float kCollectRise = 0.6f;
    constexpr float kCollectGrow = 0.9f;
    // 当たり判定の大きさ（マス）。見た目より少し大きくして取りやすく
    constexpr float kHitBox = 0.7f;
    constexpr float kGemZ = -0.25f;
}

void CollectibleBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    (void)width;
    (void)height;
    device_ = device;
    centerX_ = worldX;
    centerY_ = worldY;

    // 当たり判定用の箱（Draw では描かない。パレットの色や形は関係ない）
    gameObject_ = std::make_unique<GameObject>("Collectible");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();
    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 0.45f, 0.8f, 1.0f, 0.0f };
    prc->GetMaterial().lightingType = 0;
    tc->SetScale({ kHitBox, kHitBox, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    SetupCollider();

    collectedBefore_ = CollectibleTracker::Get().WasCollectedBefore(chipX_, chipY_);
    time_ = static_cast<float>((chipX_ * 7 + chipY_ * 13) % 17) * 0.37f; // 並んでいても揺れがそろわないように位相をずらす
    EnsureModel();
}

void CollectibleBlock::EnsureModel() {
    if (gem_ || !device_) return;
    Model* model = ModelManager::GetInstance()->GetModel(kModelDir, kModelFile);
    if (!model) return;
    gem_ = std::make_unique<Object3D>();
    gem_->Initialize(device_, model);
    gem_->SetName("CollectibleGem");
    Material& mat = gem_->GetMaterial();
    mat.color = kGemColor;
    mat.lightingType = 2;              // お宝と同じクリスタル表示
    mat.enableEnvironmentMap = 1;
    mat.shininess = 64.0f;
    mat.environmentCoefficient = 0.8f;
    gem_->SetScale({ kGemScale, kGemScale, kGemScale });
    gem_->SetTranslation({ centerX_, centerY_, kGemZ });
    gem_->Update();
}

void CollectibleBlock::Update() {
    BaseBlock::Update();
    // 巻き戻し等で破壊が解除されたら、演出も「まだ取っていない」に戻す
    if (!isDestroyed_ && collectTimer_ >= kCollectAnim) {
        collectTimer_ = -1.0f;
    }
    if (isDestroyed_) return;

    float dt = TimeManager::GetInstance().GetDeltaTime();
    time_ += dt;
    if (collectTimer_ >= 0.0f) {
        collectTimer_ += dt;
        if (collectTimer_ >= kCollectAnim) {
            Destroy();
            return;
        }
    }
    if (!gem_) return;

    // 取った後：上がりながら大きくなって白く光り、消える
    float t = (collectTimer_ >= 0.0f) ? std::clamp(collectTimer_ / kCollectAnim, 0.0f, 1.0f) : 0.0f;
    float bob = std::sin(time_ * kBobSpeed) * kBobAmp;
    float scale = kGemScale * (1.0f + kCollectGrow * t);
    gem_->SetTranslation({ centerX_, centerY_ + bob + kCollectRise * t, kGemZ });
    gem_->SetRotation({ 0.0f, time_ * kSpinSpeed + t * 6.0f, 0.0f });
    gem_->SetScale({ scale, scale, scale });

    Vector4 c = kGemColor;
    float alpha = collectedBefore_ ? kGhostAlpha : 1.0f;
    if (t > 0.0f) {
        c = { kGemColor.x + (kFlashColor.x - kGemColor.x) * t,
              kGemColor.y + (kFlashColor.y - kGemColor.y) * t,
              kGemColor.z + (kFlashColor.z - kGemColor.z) * t, 1.0f };
        alpha = (1.0f - t);
    }
    c.w = alpha;
    gem_->GetMaterial().color = c;
    gem_->Update();
}

void CollectibleBlock::Draw() {
    // 当たり判定の箱は描かない。宝石モデルだけ
    if (isDestroyed_ || !gem_) return;
    gem_->Draw();
}

void CollectibleBlock::OnCollision(Player2D* player) {
    if (isDestroyed_ || collectTimer_ >= 0.0f || !player) return;
    collectTimer_ = 0.0f;
    CollectibleTracker::Get().OnCollected(chipX_, chipY_);
}

void CollectibleBlock::Reset() {
    SetDestroyed(false);
    collectTimer_ = -1.0f;
}
