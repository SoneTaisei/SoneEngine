#include "DoorBlock.h"
#include "LinkColor.h"
#include "SwitchBlock.h"
#include "Editor/Replay/ReplayManager.h"
#include "Game2D/MapChip2D.h"
#include "Resource/Audio/AudioManager.h"
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
    // ドアの色は連動番号ごと（同じ番号のスイッチと同じ色になる）。開き具合で明るさを変えるので Update でも入れ直す
    renderer->GetMaterial().color = LinkColor::Dark(linkId_);
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
    if (properties.contains("closeSpeed") && properties["closeSpeed"].is_number()) {
        closeSpeed_ = properties["closeSpeed"];
    }
    if (properties.contains("openDirection") && properties["openDirection"].is_string()) {
        std::string dir = properties["openDirection"];
        if (dir == "Up" || dir == "Down" || dir == "Left" || dir == "Right") {
            openDirection_ = dir;
        }
    }
    if (properties.contains("openDistance") && properties["openDistance"].is_number()) {
        openDistance_ = properties["openDistance"];
    }
    if (properties.contains("latch") && properties["latch"].is_boolean()) {
        latch_ = properties["latch"];
    }
    if (properties.contains("crushKills") && properties["crushKills"].is_boolean()) {
        crushKills_ = properties["crushKills"];
    }
    openSpeed_ = (std::max)(openSpeed_, 0.0f);
    closeSpeed_ = (std::max)(closeSpeed_, 0.0f);
    ApplyTransform();
}

bool DoorBlock::OnChainTouch(const Vector3& pos, float radius, const Vector3& velocity, bool isWeight) {
    (void)pos; (void)radius; (void)velocity; (void)isWeight;
    blockedThisFrame_ = true;
    return false; // 当たりは消費しない（鎖の勢いは弱めない）
}

void DoorBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_ || !map_) return;

    bool isAnySwitchPressed = false;

    // マップ上のすべてのブロックから、自分と同じlinkIdを持つSwitchBlockを探す
    for (const auto& blockPtr : map_->GetUpdateBlocks()) {
        if (!blockPtr || blockPtr->IsDestroyed()) continue;
        SwitchBlock* switchBlock = dynamic_cast<SwitchBlock*>(blockPtr.get());
        if (switchBlock && switchBlock->GetLinkId() == linkId_ && switchBlock->IsPressed()) {
            isAnySwitchPressed = true;
            break; // 1つでも押されていれば開く
        }
    }

    // 一度全部開いたら開いたまま
    if (latch_ && openProgress_ >= 1.0f) {
        latched_ = true;
    }
    bool wantOpen = isAnySwitchPressed || latched_;

    // リプレイ再生・シーク時も録画時と同じだけ時間が進むよう、共有クロックの差分を使う
    float dt = ReplayManager::GetInstance()->GetPlayDeltaTime();

    if (wantOpen) {
        if (openProgress_ <= 0.0f) {
            AudioManager::Play("resources/Sound/10Dyas/SE/OpenDoor.mp3", 0.75f);
        }
        openProgress_ += dt * openSpeed_;
        if (openProgress_ > 1.0f) openProgress_ = 1.0f;
    } else {
        // 挟まれミス無しの設定なら、通路に鎖がある間は閉まらずに待つ
        bool blocked = (!crushKills_ && blockedThisFrame_);
        if (!blocked) {
            openProgress_ -= dt * closeSpeed_;
            if (openProgress_ < 0.0f) openProgress_ = 0.0f;
        }
    }
    blockedThisFrame_ = false;

    // 連動番号ごとの色。開くほど明るくして、動いているのが分かるようにする
    Vector4 closed = LinkColor::Dark(linkId_);
    Vector4 opened = LinkColor::Bright(linkId_);
    float t = openProgress_;
    Vector4 currentColor = { closed.x + (opened.x - closed.x) * t,
                             closed.y + (opened.y - closed.y) * t,
                             closed.z + (opened.z - closed.z) * t,
                             closed.w };

    if (auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
        renderer->GetMaterial().color = currentColor;
    }

    ApplyTransform();
}

void DoorBlock::ApplyTransform() {
    if (!gameObject_) return;
    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    bool vertical = (openDirection_ == "Up" || openDirection_ == "Down");
    float full = vertical ? startHeight_ : startWidth_;
    float retract = (openDistance_ > 0.0f) ? (std::min)(openDistance_, full) : full;
    float remain = full - retract * openProgress_;
    if (remain < 0.001f) remain = 0.0f;

    float w = startWidth_;
    float h = startHeight_;
    float cx = startX_;
    float cy = startY_;
    if (openDirection_ == "Up") {
        // 上が固定で、下から引っ込む
        h = remain;
        cy = (startY_ + startHeight_ * 0.5f) - h * 0.5f;
    } else if (openDirection_ == "Down") {
        h = remain;
        cy = (startY_ - startHeight_ * 0.5f) + h * 0.5f;
    } else if (openDirection_ == "Left") {
        w = remain;
        cx = (startX_ - startWidth_ * 0.5f) + w * 0.5f;
    } else { // Right
        w = remain;
        cx = (startX_ + startWidth_ * 0.5f) - w * 0.5f;
    }
    float sz = tc->GetScale().z;
    tc->SetScale({w, h, sz});
    tc->SetPosition({cx, cy, 0.0f});
}

AABB2D DoorBlock::GetClosedAABB() const {
    return {
        startX_ - startWidth_ * 0.5f,
        startY_ + startHeight_ * 0.5f,
        startX_ + startWidth_ * 0.5f,
        startY_ - startHeight_ * 0.5f
    };
}

void DoorBlock::Reset() {
    openProgress_ = 0.0f;
    latched_ = false;
    blockedThisFrame_ = false;
    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            float sz = tc->GetScale().z;
            tc->SetScale({startWidth_, startHeight_, sz});
            tc->SetPosition({startX_, startY_, 0.0f});
        }
    }
}

void DoorBlock::CaptureReplayState(std::vector<float>& outCustom) const {
    outCustom.clear();
    outCustom.push_back(openProgress_);
    outCustom.push_back(latched_ ? 1.0f : 0.0f);
}

void DoorBlock::RestoreReplayState(const std::vector<float>& custom) {
    if (custom.empty()) return;
    openProgress_ = custom[0];
    latched_ = (custom.size() >= 2) ? (custom[1] != 0.0f) : false;
    ApplyTransform();
}
