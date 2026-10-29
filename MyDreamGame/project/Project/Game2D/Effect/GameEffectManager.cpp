#include "GameEffectManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Graphics/TextureManager.h"
#include "Graphics/CameraManager.h"
#include "Graphics/Camera/GameCamera.h"
#include "Core/Utility/TransformFunctions.h"
#include "Resource/Model/ModelManager.h"
#include "Effect/ParticleCommon.h"
#include "GameObject/PrimitiveObject.h"

// エフェクト各種
#include "Effect/BaseEffect.h"
#include "Effect/BaseEffect/CoinEffect.h"
#include "Effect/BaseEffect/RingEffect.h"
#include "Effect/BaseEffect/CylinderEffect.h"
#include "Effect/GPUParticle/GPUParticleSystem.h"
#include "Editor/Replay/ReplayManager.h"

#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

GameEffectManager::GameEffectManager() = default;
GameEffectManager::~GameEffectManager() = default;

void GameEffectManager::Initialize() {
    auto device = DirectXCommon::GetInstance()->GetDevice();

    // 1. 各種BaseEffectの生成と初期化
    effects_.clear();

    auto coinEffect = std::make_unique<CoinEffect>();
    coinEffect->Initialize(device);
    coinEffect_ = coinEffect.get();
    effects_.push_back(std::move(coinEffect));

    uint32_t gradationHandle = TextureManager::GetInstance()->Load("resources/Sprite/School/gradationLine.png");

    auto ringEffect = std::make_unique<RingEffect>();
    ringEffect->Initialize(device, gradationHandle);
    effects_.push_back(std::move(ringEffect));

    auto cylinderEffect = std::make_unique<CylinderEffect>();
    cylinderEffect->Initialize(device, gradationHandle);
    effects_.push_back(std::move(cylinderEffect));

    // 2. GPUパーティクル (snow) の初期化
    snowParticle_ = std::make_unique<GPUParticleSystem>();
    snowParticle_->Initialize(device);
    if (snowParticle_->LoadFromFile("resources/json/shared/Particle/snow.json")) {
        snowParticle_->Play();
    }
}

void GameEffectManager::Update(float deltaTime, bool isGameActive, GameCamera* camera) {
    // ゲームプレイ中のみBaseEffectを更新
    if (isGameActive) {
        for (auto& effect : effects_) {
            if (effect) {
                effect->Update(1.0f / 60.0f);
            }
        }
    }

    // 降雪パーティクルの更新（カメラ位置の画面右端に追従させる）
    if (snowParticle_) {
        Vector3 camPos = camera ? camera->GetTranslation() : CameraManager::GetInstance()->GetCameraPos();
        float halfW = (camera && camera->IsOrthographic()) ? (camera->GetOrthoWidth() * 0.5f) : 10.0f;
        Vector3 snowPos = { camPos.x + halfW, camPos.y, 0.0f };
        snowParticle_->SetPosition(snowPos);
        snowParticle_->Update(deltaTime);
    }
}

void GameEffectManager::Draw(ParticleCommon* particleCommon, const Matrix4x4& viewProjectionMatrix) {
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects() || ReplayManager::GetInstance()->IsPlaying()) {
        for (auto& effect : effects_) {
            if (effect) effect->Draw();
        }
    }
#else
    for (auto& effect : effects_) {
        if (effect) effect->Draw();
    }
#endif

    if (snowParticle_) {
#ifdef USE_IMGUI
        if (EditorManager::IsShowEffects() || EditorManager::IsPlaying() || ReplayManager::GetInstance()->IsPlaying()) {
#endif
            if (particleCommon) {
                particleCommon->PreDraw();
                auto commandList = DirectXCommon::GetInstance()->GetCommandList();
                Matrix4x4 cameraMatrix = TransformFunctions::Inverse(CameraManager::GetInstance()->GetViewMatrix());
                ModelManager* modelManager = ModelManager::GetInstance();
                snowParticle_->Draw(commandList, viewProjectionMatrix, cameraMatrix, particleCommon, modelManager);
            }
#ifdef USE_IMGUI
        }
#endif
    }
}

void GameEffectManager::EmitCoin(const Vector3& playerPosition) {
    if (coinEffect_) {
        float playerWidth = 1.0f;
        float playerHeight = 1.0f;
        Vector3 hitEmitterPos = {
            playerPosition.x + playerWidth * 0.5f,
            playerPosition.y + playerHeight * 0.5f,
            0.0f
        };
        coinEffect_->Emit(hitEmitterPos);
    }
}

void GameEffectManager::ResetAllEffects() {
    for (auto& effect : effects_) {
        if (effect) {
            effect->Reset();
        }
    }
}

std::vector<PrimitiveObject*> GameEffectManager::GetPrimitives() const {
    std::vector<PrimitiveObject*> result;
    for (const auto& effect : effects_) {
        if (effect) {
            auto effectPrims = effect->GetPrimitives();
            result.insert(result.end(), effectPrims.begin(), effectPrims.end());
        }
    }
    return result;
}
