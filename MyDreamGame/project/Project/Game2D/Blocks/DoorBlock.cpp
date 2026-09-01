#include "DoorBlock.h"
#include "SwitchBlock.h"
#include "Core/TimeManager.h"
#include "Game2D/MapChip2D.h"
#include <algorithm>

DoorBlock::DoorBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

void DoorBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    startWidth_ = width;
    startHeight_ = height;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("DoorBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    // ドアの色（鉄格子っぽい青灰色）
    renderer->GetMaterial().color = {0.5f, 0.6f, 0.7f, 1.0f};
    renderer->GetMaterial().lightingType = 1;

    SetupCollider();
}

void DoorBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("linkId") && properties["linkId"].is_number()) {
        linkId_ = properties["linkId"];
    }
    if (properties.contains("openSpeed") && properties["openSpeed"].is_number()) {
        openSpeed_ = properties["openSpeed"];
    }
}

void DoorBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_ || !map_) return;

    bool isAnySwitchPressed = false;

    // マップ上のすべてのブロックから、自分と同じlinkIdを持つSwitchBlockを探す
    for (const auto& blockPtr : map_->GetUpdateBlocks()) {
        if (!blockPtr || blockPtr->IsDestroyed()) continue;
        
        // dynamic_cast で SwitchBlock かどうか判定
        SwitchBlock* switchBlock = dynamic_cast<SwitchBlock*>(blockPtr.get());
        if (switchBlock) {
            if (switchBlock->GetLinkId() == linkId_ && switchBlock->IsPressed()) {
                isAnySwitchPressed = true;
                break; // 1つでも押されていれば開く
            }
        }
    }

    float dt = TimeManager::GetInstance().GetDeltaTime();

    if (isAnySwitchPressed) {
        // 開く
        openProgress_ += dt * openSpeed_;
        if (openProgress_ > 1.0f) openProgress_ = 1.0f;
    } else {
        // 閉まる
        openProgress_ -= dt * openSpeed_;
        if (openProgress_ < 0.0f) openProgress_ = 0.0f;
    }

    // 見た目と判定の更新（シャッターのように上に開く）
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (tc) {
        // openProgress_ が 1.0f の時は完全に開く（Yスケール0）
        // 完全に0にすると判定や見た目でおかしくなるかもしれないので、最小0.01fなどを残すか
        float currentScaleY = startHeight_ * (1.0f - openProgress_);
        if (currentScaleY < 0.001f) currentScaleY = 0.0f;
        
        // Yスケールが変わった分、位置を上にずらす（上が固定されているように見せる）
        // 中心座標を計算。上が固定されるということは、Y座標の基準は startY_ + startHeight_ * 0.5f。
        // 現在のY座標の中心 = (startY_ + startHeight_ * 0.5f) - currentScaleY * 0.5f
        float currentY = (startY_ + startHeight_ * 0.5f) - currentScaleY * 0.5f;

        tc->SetScale({startWidth_, currentScaleY, 1.0f});
        tc->SetPosition({startX_, currentY, 0.0f});
    }
}

void DoorBlock::Reset() {
    openProgress_ = 0.0f;
    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            tc->SetScale({startWidth_, startHeight_, 1.0f});
            tc->SetPosition({startX_, startY_, 0.0f});
        }
    }
}
