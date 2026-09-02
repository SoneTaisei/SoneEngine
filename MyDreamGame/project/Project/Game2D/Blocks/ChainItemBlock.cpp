#include "ChainItemBlock.h"
#include "../Player/Player2D.h"

void ChainItemBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
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
}

void ChainItemBlock::OnCollision(Player2D* player) {
    if (!isDestroyed_ && player) {
        // プレイヤーの鎖の長さを1増やす
        player->AddChainLength(1);
        
        // 拾ったのでこのギミックをマップから消す
        Destroy();
    }
}
