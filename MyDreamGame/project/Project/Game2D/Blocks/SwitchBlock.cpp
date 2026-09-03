#include "SwitchBlock.h"
#include "Core/TimeManager.h"

SwitchBlock::SwitchBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

void SwitchBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    startWidth_ = width;
    startHeight_ = height;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("SwitchBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    // スイッチらしく、少し薄くする
    transform->SetPosition({worldX, worldY - height * 0.25f, 0.0f});
    transform->SetScale({width * 0.8f, height * 0.5f, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    // スイッチの色（赤系）
    renderer->GetMaterial().color = {0.8f, 0.2f, 0.2f, 1.0f};
    renderer->GetMaterial().lightingType = 1;

    SetupCollider();
}

void SwitchBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("linkId") && properties["linkId"].is_number()) {
        linkId_ = properties["linkId"];
    }
}

void SwitchBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

    float dt = TimeManager::GetInstance().GetDeltaTime();
    
    // タイマーを減らす
    if (pressedTimer_ > 0.0f) {
        pressedTimer_ -= dt;
        isPressed_ = true;
    } else {
        isPressed_ = false;
    }

    // 見た目の更新
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>();
    if (tc && renderer) {
        if (isPressed_) {
            // 押されている時は沈み込み、色が明るくなる
            tc->SetScale({startWidth_ * 0.8f, startHeight_ * 0.1f, 1.0f});
            tc->SetPosition({startX_, startY_ - startHeight_ * 0.45f, 0.0f});
            renderer->GetMaterial().color = {1.0f, 0.5f, 0.5f, 1.0f};
        } else {
            // 元に戻る
            tc->SetScale({startWidth_ * 0.8f, startHeight_ * 0.5f, 1.0f});
            tc->SetPosition({startX_, startY_ - startHeight_ * 0.25f, 0.0f});
            renderer->GetMaterial().color = {0.8f, 0.2f, 0.2f, 1.0f};
        }
    }
}

void SwitchBlock::OnCollision(Player2D* player) {
    // プレイヤーが重なった（通過した）時、タイマーをリセット
    (void)player;
    pressedTimer_ = 0.1f;
}

void SwitchBlock::OnChainTouch(const Vector3& pos, float radius, float speed) {
    // 鎖の節や宝石が乗っている間はプレイヤーと同じく押され続ける（重さスイッチ）
    (void)pos; (void)radius; (void)speed;
    pressedTimer_ = 0.1f;
}

void SwitchBlock::Reset() {
    isPressed_ = false;
    pressedTimer_ = 0.0f;
}