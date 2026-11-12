#include "DustBlock.h"
#include "Game2D/Player/Player2D.h"

void DustBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    SetGameObject(std::make_unique<GameObject>("DustBlock"));
    auto *tc = GetGameObject()->AddComponent<TransformComponent>();
    auto *prc = GetGameObject()->AddComponent<PrimitiveRendererComponent>();
    
    prc->Initialize(device, boxPrimitive);
    
    // Core dark miasma color
    prc->SetColor({ 0.05f, 0.02f, 0.06f, 0.999f });
    // lightingType 3 = Dust Miasma Shading Mode
    prc->SetLightingType(3);
    // Pixelation resolution (e.g. 32x32 pixels per block)
    prc->SetShininess(32.0f);
    // Organic crawl speed
    prc->SetEnvironmentCoefficient(1.6f);
    prc->SetEnableEnvironmentMap(0);
    prc->SetTimeScale(1.4f);
    prc->SetEnableAutoTime(true);
    prc->SetBlendMode(BlendMode::kBlendModeNormal);
    prc->SetIsDoubleSided(false);

    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });

    SetupCollider();
}

void DustBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill();
    }
}
