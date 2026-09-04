#include "ChainItemBlock.h"
#include "../Player/Player2D.h"
#include <algorithm>

namespace {
    constexpr int kMaxPips = 8;
    constexpr float kPipSize = 0.09f;
    constexpr float kPipPitch = 0.13f;
    constexpr float kPipFrontZ = -0.3f;
}

void ChainItemBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    width_ = width;
    height_ = height;
    device_ = device;
    boxPrimitive_ = boxPrimitive;

    gameObject_ = std::make_unique<GameObject>("ChainItem");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();
    prc->Initialize(device, boxPrimitive);
    // 鎖っぽい色（銀色）
    prc->GetMaterial().color = { 0.8f, 0.8f, 0.8f, 1.0f };
    prc->GetMaterial().lightingType = 1;
    // 少し小さめに配置
    tc->SetScale({ width * 0.5f, height * 0.5f, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    // 当たり判定をセットアップ（IsSolid=falseなのですり抜ける判定だけになる）
    SetupCollider();

    SyncPips();
}

void ChainItemBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("units") && properties["units"].is_number()) {
        units_ = properties["units"];
    }
    units_ = std::clamp(units_, 1, kMaxPips);
    SyncPips();
}

void ChainItemBlock::SyncPips() {
    // インスペクタのプレビュー用ブロックは Initialize されないので、その時は何も作らない
    if (!device_ || !boxPrimitive_) return;
    // 1本なら点は出さない（今まで通りの見た目）。2本以上で本数分の点を上に並べる
    int wanted = (units_ >= 2) ? units_ : 0;
    while (static_cast<int>(pips_.size()) > wanted) {
        pips_.pop_back();
    }
    while (static_cast<int>(pips_.size()) < wanted) {
        auto pip = std::make_unique<GameObject>("ChainItemPip");
        auto* tc = pip->AddComponent<TransformComponent>();
        tc->SetScale({kPipSize, kPipSize, 0.06f});
        auto* r = pip->AddComponent<PrimitiveRendererComponent>();
        r->Initialize(device_, boxPrimitive_);
        r->GetMaterial().color = {0.95f, 0.85f, 0.35f, 1.0f};
        r->GetMaterial().lightingType = 1;
        pips_.push_back(std::move(pip));
    }
    int n = static_cast<int>(pips_.size());
    for (int i = 0; i < n; ++i) {
        float ox = (static_cast<float>(i) - (n - 1) * 0.5f) * kPipPitch;
        if (auto* tc = pips_[i]->GetComponent<TransformComponent>()) {
            tc->SetPosition({startX_ + ox, startY_ + height_ * 0.25f + 0.12f, kPipFrontZ});
        }
        pips_[i]->Update();
    }
}

void ChainItemBlock::Draw() {
    BaseBlock::Draw();
    if (isDestroyed_) return;
    for (auto& pip : pips_) {
        pip->Draw();
    }
}

void ChainItemBlock::OnCollision(Player2D* player) {
    if (!isDestroyed_ && player) {
        // プレイヤーの鎖を units_ 本増やす
        player->AddChainLength(units_);
        // 拾ったのでこのギミックをマップから消す
        Destroy();
    }
}
