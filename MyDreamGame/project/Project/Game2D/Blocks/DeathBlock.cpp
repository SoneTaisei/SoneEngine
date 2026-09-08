#include "DeathBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Effect/GPUParticle/GPUParticleSystem.h"
#include "Editor/Replay/ReplayManager.h"
#include "Core/TimeManager.h"

DeathBlock::DeathBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

DeathBlock::~DeathBlock() = default;

void DeathBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("DeathBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();
    
    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 1.0f, 0.2f, 0.2f, 1.0f };
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1;
    SetupCollider();

    if (device) {
        thunderParticle_ = std::make_unique<GPUParticleSystem>();
        thunderParticle_->Initialize(device);
        thunderParticle_->LoadFromFile("resources/json/shared/Particle/Thunder.json");
        thunderParticle_->PlayAt({ worldX, worldY, 0.0f });
    }
}

void DeathBlock::Update() {
    BaseBlock::Update();

    if (thunderParticle_) {
        float dt = ReplayManager::GetInstance()->GetPlayDeltaTime();
        if (dt <= 0.0f) {
            dt = TimeManager::GetInstance().GetDeltaTime();
        }

        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                thunderParticle_->SetPosition(tc->GetPosition());
            }
        }

        if (!thunderParticle_->IsPlaying()) {
            thunderParticle_->Play();
        }
        thunderParticle_->Update(dt);
    }
}

void DeathBlock::DrawParticle(
    ID3D12GraphicsCommandList* commandList,
    const Matrix4x4& viewProjection,
    const Matrix4x4& cameraMatrix,
    ParticleCommon* particleCommon,
    ModelManager* modelManager) {
    if (thunderParticle_ && thunderParticle_->IsPlaying()) {
        thunderParticle_->Draw(commandList, viewProjection, cameraMatrix, particleCommon, modelManager);
    }
}

void DeathBlock::OnCollision(Player2D* player) {
    if (player) {
        player->Kill();
    }
}

void DeathBlock::OnPlayerStand(Player2D* player) {
    if (player) {
        player->Kill();
    }
}

void DeathBlock::OnPlayerTouch(Player2D* player) {
    if (player) {
        player->Kill();
    }
}

void DeathBlock::Reset() {
    if (thunderParticle_) {
        if (gameObject_) {
            if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
                thunderParticle_->SetPosition(tc->GetPosition());
            }
        }
        thunderParticle_->Restart();
        thunderParticle_->Play();
    }
}
