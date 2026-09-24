#include "TitleScene.h"
#include <cmath>
#include "../externals/imgui/imgui.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneManager.h"
#include "Resource/Sprite/SpriteCommon.h"
#include <wrl.h>
#include "Resource/Model/ModelManager.h"
#include "Graphics/CameraManager.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Scene/SceneFactory.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/Renderer.h"
#include "Component/TransformComponent.h"
#include "GameObject/Object3D.h"
#include "Graphics/GameCamera.h"

TitleScene::~TitleScene() {
}

void TitleScene::OnEnter(SceneManager* sceneManager) {
    (void)sceneManager;
    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};
    cameraTransform_.rotate = {0.0f, 0.0f, 0.0f};

    if (gameCamera_) {
        gameCamera_->Reset();
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }
    CameraManager::GetInstance()->ClearCullingCameraInfo();
}

void TitleScene::OnExit(SceneManager* sceneManager) {
    (void)sceneManager;
}

void TitleScene::Initialize() {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};
    cameraTransform_.rotate = {0.0f, 0.0f, 0.0f};

    // カメラの初期化リセット
    if (gameCamera_) {
        gameCamera_->Reset();
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }
    CameraManager::GetInstance()->ClearCullingCameraInfo();

    // Skyboxの初期化処理
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    // UIスプライトの初期化
    titleTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/title.png");
    startTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/start.png");

    if (spriteCommon_) {
        // title.png: 元サイズ (800 x 92) の1.2倍 (960 x 110.4)
        titleSprite_ = std::make_unique<Sprite>();
        titleSprite_->Initialize(spriteCommon_, titleTextureHandle_);
        const float titleWidth = 800.0f * 1.2f;
        const float titleHeight = 92.0f * 1.2f;
        const float titleX = (1280.0f - titleWidth) * 0.5f;
        const float titleBaseY = 150.0f;
        titleSprite_->SetPosition({ titleX, titleBaseY });
        titleSprite_->SetSize({ titleWidth, titleHeight });
        titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        // start.png: 中央下部に配置 (250 x 92)
        startSprite_ = std::make_unique<Sprite>();
        startSprite_->Initialize(spriteCommon_, startTextureHandle_);
        const float startWidth = 250.0f;
        const float startHeight = 92.0f;
        const float startX = (1280.0f - startWidth) * 0.5f;
        const float startY = 520.0f;
        startSprite_->SetPosition({ startX, startY });
        startSprite_->SetSize({ startWidth, startHeight });
        startSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    animationTime_ = 0.0f;
}

void TitleScene::Update(SceneManager *sceneManager) {
    // シーン遷移直後の同一フレームでの入力を拾わないようにする
    if (isFirstFrame_) {
        isFirstFrame_ = false;
    } else {
        bool isTriggered = KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) ||
                           GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::A) ||
                           GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start);
        if (isTriggered) {
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kStageSelect));
            return;
        }
    }

    // 演出用タイマー加算
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    animationTime_ += deltaTime;

    // タイトル文字: sin波で上下に浮遊移動
    if (titleSprite_) {
        const float titleWidth = 800.0f * 1.2f;
        const float titleX = (1280.0f - titleWidth) * 0.5f;
        const float titleBaseY = 150.0f;
        const float titleOffsetY = std::sin(animationTime_ * 2.0f) * 12.0f;
        titleSprite_->SetPosition({ titleX, titleBaseY + titleOffsetY });
        titleSprite_->Update();
    }

    // START文字: sin波で透明度をフェード変化（出現/透明化）
    if (startSprite_) {
        float alpha = (std::sin(animationTime_ * 4.0f) + 1.0f) * 0.5f;
        startSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
        startSprite_->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    (void)viewProjectionMatrix;

    // モデル描画の前準備
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    // Skyboxを描画
    if (skybox_) {
        skybox_->Draw();

        // Skybox描画後はPSOが切り替わるためモデル用設定を復帰
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }
}

void TitleScene::Draw2D() {
    // UIスプライトの最前面描画（ポストエフェクト完了後に描画）
    if (spriteCommon_) {
        spriteCommon_->PreDraw();
        if (titleSprite_) {
            titleSprite_->Draw();
        }
        if (startSprite_) {
            startSprite_->Draw();
        }
    }
}

std::vector<Object3D *> TitleScene::GetObjects() {
    return {};
}

std::vector<ParticleManager *> TitleScene::GetParticles() {
    return {};
}

std::vector<PrimitiveObject *> TitleScene::GetPrimitives() {
    return {};
}

void TitleScene::UpdateEditor() {
    if (titleSprite_) {
        const float titleWidth = 800.0f * 1.2f;
        const float titleX = (1280.0f - titleWidth) * 0.5f;
        const float titleBaseY = 150.0f;
        titleSprite_->SetPosition({ titleX, titleBaseY });
        titleSprite_->SetSize({ titleWidth, 92.0f * 1.2f });
        titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        titleSprite_->Update();
    }
    if (startSprite_) {
        const float startWidth = 250.0f;
        const float startHeight = 92.0f;
        const float startX = (1280.0f - startWidth) * 0.5f;
        const float startY = 520.0f;
        startSprite_->SetPosition({ startX, startY });
        startSprite_->SetSize({ startWidth, startHeight });
        startSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        startSprite_->Update();
    }
    if (skybox_) {
        skybox_->Update();
    }
}

void TitleScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    (void)selectedPrimitive;
#endif
}