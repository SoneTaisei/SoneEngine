#include "GameScene.h"
#include "Scene/SceneManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/Camera/GameCamera.h"
#include "Scene/SceneFactory.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "Editor/EditorManager.h"
#endif
#include "Editor/Replay/ReplayManager.h"
#include "Renderer/Renderer.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include "Graphics/Skybox.h"
#include "Graphics/CameraManager.h"
#include "Resource/Model/ModelManager.h"
#include "Core/Utility/ParameterManager.h"
#include "GameObject/WorldGif.h"

std::string GameScene::s_TargetMapFilePath = "resources/json/shared/Map/map_data.json";

void GameScene::OnEnter(SceneManager* sceneManager) {
    // StageSelectSceneから選択されたステージのパスを受け取る
    if (sceneManager->HasData("SelectedStagePath")) {
        std::string selectedPath = sceneManager->GetData<std::string>("SelectedStagePath");
        if (!selectedPath.empty()) {
            if (selectedPath.find('/') == std::string::npos && selectedPath.find('\\') == std::string::npos) {
                selectedPath = "resources/json/shared/MapData/" + selectedPath;
            }
            s_TargetMapFilePath = selectedPath;
            if (map_) {
                map_->Initialize(s_TargetMapFilePath);
            }
            if (s_TargetMapFilePath.find("temp_play_map") == std::string::npos) {
                StageGifManager::GetInstance()->LoadForStage(s_TargetMapFilePath);
            }
        }
    }
    if (player_ && map_) {
        player_->FindSpawnPoint(*map_);
    }
    if (GetGameCamera()) {
        float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
        float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
        GetGameCamera()->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);
        if (map_) {
            GetGameCamera()->SetRooms(map_->GetRooms());
        }
        if (player_) {
            GetGameCamera()->SetFollowTarget(&player_->GetPosition());
        }
        GetGameCamera()->SetFollowEnabled(true);
        GetGameCamera()->SnapToTarget();
        initialCameraScale_ = GetGameCamera()->GetScale();
    }
}

void GameScene::OnExit(SceneManager* sceneManager) {
    // スコアなどを保存してTitleやStageSelectに渡す
    if (player_) {
        sceneManager->SetData("LastScore", player_->GetScore());
    }
    // 注: TitleSceneやStageSelectSceneは自前でOnEnter/InitializeにてReset()やカメラ座標設定を行っているため、
    // GameScene終了時に無差別にReset()を呼ぶと、GameScene再突入時や内部遷移時にカメラが3Dパースペクティブに破壊される原因となるため呼び出さない。
}

void GameScene::Initialize() {
    Log("GameScene::Initialize: Start\n");

    // 1. Device取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();
    Log("GameScene::Initialize: Device got\n");

    // 2. PrimitiveManagerの初期化（まだの場合）
    PrimitiveManager::GetInstance()->Initialize(device.Get());
    Log("GameScene::Initialize: PrimitiveManager Initialized\n");

    // リプレイ保存リストとマクロの読み込み
    ReplayManager::GetInstance()->LoadSavedList();
    ReplayManager::GetInstance()->LoadMacros();
    Log("GameScene::Initialize: ReplayManager loaded\n");

    // ★ Skyboxの初期化処理を追加
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/skybox/BackGround.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));
    Log("GameScene::Initialize: Skybox loaded\n");

    // エフェクト群の作成（BaseEffectによるポリモーフィズム）
    effects_.clear();

    auto coinEffect = std::make_unique<CoinEffect>();
    coinEffect->Initialize(DirectXCommon::GetInstance()->GetDevice());
    coinEffect_ = coinEffect.get();
    effects_.push_back(std::move(coinEffect));
    Log("GameScene::Initialize: CoinEffect Initialized\n");

    uint32_t gradationHandle = TextureManager::GetInstance()->Load("resources/Sprite/School/gradationLine.png");
    auto ringEffect = std::make_unique<RingEffect>();
    ringEffect->Initialize(device.Get(), gradationHandle);
    effects_.push_back(std::move(ringEffect));

    auto cylinderEffect = std::make_unique<CylinderEffect>();
    cylinderEffect->Initialize(device.Get(), gradationHandle);
    effects_.push_back(std::move(cylinderEffect));

    // GPUパーティクル (snow) の初期化
    snowParticle_ = std::make_unique<GPUParticleSystem>();
    snowParticle_->Initialize(device.Get());
    if (snowParticle_->LoadFromFile("resources/json/shared/Particle/snow.json")) {
        snowParticle_->Play();
    }
    Log("GameScene::Initialize: Effects and Snow Particle Initialized\n");

    // 5. マップの生成と初期化
    map_ = std::make_unique<MapChip2D>();
    map_->Initialize( s_TargetMapFilePath);
    Log("GameScene::Initialize: Map Initialized\n");

    // ステージ別ワールドGIF（板ポリ）の初期化とロード
    StageGifManager::GetInstance()->Initialize(device.Get());
    std::string gifStagePath = s_TargetMapFilePath;
    if (gifStagePath.find("temp_play_map") != std::string::npos) {
        gifStagePath = StageGifManager::GetInstance()->GetCurrentStageName();
    }
    StageGifManager::GetInstance()->LoadForStage(gifStagePath);

    // 6. プレイヤーの生成と初期化
    playerObj_ = std::make_unique<GameObject>("Player");
    playerObj_->AddComponent<TransformComponent>();
    player_ = playerObj_->AddComponent<Player2D>();
    player_->SetCamera(GetGameCamera()); // 画面揺れ連携用にカメラを渡す
    Log("GameScene::Initialize: Player Initialized\n");
    
    player_->FindSpawnPoint(*map_);
    Log("GameScene::Initialize: Player SpawnPoint found\n");

    // 7. GameCameraを正射影モード（2D表示）に切り替え
    if (GetGameCamera()) {
        Log("GameScene::Initialize: Camera config...\n");
        float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
        float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
        GetGameCamera()->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);
        if (map_) {
            GetGameCamera()->SetRooms(map_->GetRooms());
        }
        // プレイヤーの位置をカメラ追従ターゲットに設定
        GetGameCamera()->SetFollowTarget(&player_->GetPosition());
        GetGameCamera()->SetFollowEnabled(true);
        GetGameCamera()->SnapToTarget();
        initialCameraScale_ = GetGameCamera()->GetScale();
        Log("GameScene::Initialize: Camera configured\n");
    }

    // 8. 開始演出用スプライトの初期化 (READY / GO / タイムバー)
    readyTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/ready.png");
    goTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/go.png");
    whiteTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/white.png");
    if (GetSpriteCommon()) {
        float centerX = 1280.0f * 0.5f;

        readySprite_ = std::make_unique<Sprite>();
        readySprite_->Initialize(GetSpriteCommon(), readyTextureHandle_);
        float readyScale = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteScale", 2.0f);
        float readyW = 240.0f * readyScale;
        float readyH = 92.0f * readyScale;
        float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteCenterY", 360.0f);
        readySprite_->SetPosition({ centerX - readyW * 0.5f, centerY - readyH * 0.5f });
        readySprite_->SetSize({ readyW, readyH });
        readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f }); // 赤色

        goSprite_ = std::make_unique<Sprite>();
        goSprite_->Initialize(GetSpriteCommon(), goTextureHandle_);
        float goScale = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteScale", 2.0f);
        float goW = 144.0f * goScale;
        float goH = 92.0f * goScale;
        float goCenterY = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteCenterY", 360.0f);
        goSprite_->SetPosition({ centerX - goW * 0.5f, goCenterY - goH * 0.5f });
        goSprite_->SetSize({ goW, goH });
        goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f }); // 黄色

        // タイムバー (細長い四角形)
        float barWidth = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarWidth", 360.0f);
        float barHeight = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarHeight", 8.0f);
        float barOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarOffsetY", 110.0f);
        float barY = centerY + barOffsetY;

        readyBarBgSprite_ = std::make_unique<Sprite>();
        readyBarBgSprite_->Initialize(GetSpriteCommon(), whiteTextureHandle_);
        readyBarBgSprite_->SetPosition({ centerX - barWidth * 0.5f - 2.0f, barY - 2.0f });
        readyBarBgSprite_->SetSize({ barWidth + 4.0f, barHeight + 4.0f });
        readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });

        readyBarFillSprite_ = std::make_unique<Sprite>();
        readyBarFillSprite_->Initialize(GetSpriteCommon(), whiteTextureHandle_);
        readyBarFillSprite_->SetPosition({ centerX - barWidth * 0.5f, barY });
        readyBarFillSprite_->SetSize({ barWidth, barHeight });
        readyBarFillSprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });

        Log("GameScene::Initialize: Ready/Go Sprites Initialized\n");
    }

    // 9. クリア演出用スプライトの初期化
    clearTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/clear.png");
    if (GetSpriteCommon()) {
        clearSprite_ = std::make_unique<Sprite>();
        clearSprite_->Initialize(GetSpriteCommon(), clearTextureHandle_);
        float clearScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteScale", 2.0f);
        float spriteW = 220.0f * clearScale;
        float spriteH = 92.0f * clearScale;
        float centerX = 1280.0f * 0.5f;
        float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteCenterY", 180.0f);
        clearSprite_->SetPosition({ centerX - spriteW * 0.5f, centerY - spriteH * 0.5f });
        clearSprite_->SetSize({ spriteW, spriteH });
        clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f }); // 初期状態は非表示
        Log("GameScene::Initialize: ClearSprite Initialized\n");
    }

    // ポーズ用スプライトの初期化
    pauseContinueTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/tudukeru.png");
    pauseSelectTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/serekutonimodoru.png");
    if (GetSpriteCommon()) {
        pauseOverlaySprite_ = std::make_unique<Sprite>();
        pauseOverlaySprite_->Initialize(GetSpriteCommon(), whiteTextureHandle_);
        pauseOverlaySprite_->SetPosition({ 640.0f - 5000.0f, 360.0f - 5000.0f });
        pauseOverlaySprite_->SetSize({ 10000.0f, 10000.0f });
        pauseOverlaySprite_->SetColor({ 0.2f, 0.2f, 0.2f, 0.65f }); // 薄灰色の半透明スクリーン

        float centerX = 1280.0f * 0.5f;

        // つづける
        pauseContinueSprite_ = std::make_unique<Sprite>();
        pauseContinueSprite_->Initialize(GetSpriteCommon(), pauseContinueTextureHandle_);
        pauseContinueSprite_->SetPosition({ centerX - 200.0f, 310.0f - 40.0f });
        pauseContinueSprite_->SetSize({ 400.0f, 80.0f });
        pauseContinueSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        // セレクトに戻る
        pauseSelectSprite_ = std::make_unique<Sprite>();
        pauseSelectSprite_->Initialize(GetSpriteCommon(), pauseSelectTextureHandle_);
        pauseSelectSprite_->SetPosition({ centerX - 200.0f, 430.0f - 40.0f });
        pauseSelectSprite_->SetSize({ 400.0f, 80.0f });
        pauseSelectSprite_->SetColor({ 0.45f, 0.45f, 0.45f, 0.65f });

        Log("GameScene::Initialize: Pause Sprites Initialized\n");
    }

    // コインUIの初期化
    coinTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/coin.png");
    numbersTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/numbers.png");
    if (GetSpriteCommon()) {
        coinSprite_ = std::make_unique<Sprite>();
        coinSprite_->Initialize(GetSpriteCommon(), coinTextureHandle_);
        coinSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        numberSprites_.clear();
        for (int i = 0; i < 16; ++i) {
            auto numSprite = std::make_unique<Sprite>();
            numSprite->Initialize(GetSpriteCommon(), numbersTextureHandle_);
            numSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            numberSprites_.push_back(std::move(numSprite));
        }
        Log("GameScene::Initialize: Coin UI Initialized\n");
    }

    Log("GameScene::Initialize: Finish\n");
}

void GameScene::Update(SceneManager *sceneManager) {
    bool isPlayingOrReplaying = false;
#ifdef USE_IMGUI
    if (EditorManager::IsPlaying()) {
        isPlayingOrReplaying = true;
    }
#else
    isPlayingOrReplaying = true;
#endif
    if (ReplayManager::GetInstance()->IsPlaying()) {
        isPlayingOrReplaying = true;
    }

    bool isGameActive = isPlayingOrReplaying && !ReplayManager::GetInstance()->IsPaused() && gameState_ != GameState::Pause;

    if (isGameActive) {
        for (auto& effect : effects_) {
            if (effect) {
                effect->Update(1.0f / 60.0f);
            }
        }
    }

    if (skybox_) {
        skybox_->Update();
    }

    float dt = TimeManager::GetInstance().GetDeltaTime();
    
    // GPUパーティクル (snow) の更新（画面右端に設置して常に降らせる）
    if (snowParticle_) {
        Vector3 camPos = GetGameCamera() ? GetGameCamera()->GetTranslation() : CameraManager::GetInstance()->GetCameraPos();
        float halfW = (GetGameCamera() && GetGameCamera()->IsOrthographic()) ? (GetGameCamera()->GetOrthoWidth() * 0.5f) : 10.0f;
        Vector3 snowPos = { camPos.x + halfW, camPos.y, 0.0f };
        snowParticle_->SetPosition(snowPos);
        snowParticle_->Update(dt);
    }

    // フェードイン演出
    float transitionSpeed = ParameterManager::GetInstance()->GetValue("GameScene", "transitionSpeed", 1.5f);
    if (transitionAlpha_ > 0.0f) {
        transitionAlpha_ -= dt * transitionSpeed;
        if (transitionAlpha_ < 0.0f) transitionAlpha_ = 0.0f;
    }

    if (gameState_ == GameState::StartReady) {
        stateTimer_ += dt;
        float startReadyTime = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyTime", 2.0f);
        float inputDelay = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyInputDelay", 1.0f);

        float centerX = 1280.0f * 0.5f;

        if (stateTimer_ < inputDelay) {
            // READY... 演出
            float alpha = (std::min)(stateTimer_ / 0.15f, 1.0f);
            float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteCenterY", 360.0f);

            if (readySprite_) {
                float appearDuration = 0.3f;
                float progress = (std::clamp)(stateTimer_ / appearDuration, 0.0f, 1.0f);
                float c1 = 1.70158f;
                float c3 = c1 + 1.0f;
                float ease = 1.0f + c3 * std::pow(progress - 1.0f, 3.0f) + c1 * std::pow(progress - 1.0f, 2.0f);
                float scaleFactor = (std::max)(ease, 0.0f);

                float readyScale = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteScale", 2.0f);
                float baseW = 240.0f * readyScale;
                float baseH = 92.0f * readyScale;
                float currentW = baseW * scaleFactor;
                float currentH = baseH * scaleFactor;

                readySprite_->SetPosition({ centerX - currentW * 0.5f, centerY - currentH * 0.5f });
                readySprite_->SetSize({ currentW, currentH });
                readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, alpha }); // 赤色
            }

            // タイムバー演出 (レディー表示残り時間を細長い棒で表示)
            float barWidth = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarWidth", 360.0f);
            float barHeight = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarHeight", 8.0f);
            float barOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "readyBarOffsetY", 110.0f);
            float barY = centerY + barOffsetY;
            float remainRatio = (std::clamp)((inputDelay - stateTimer_) / inputDelay, 0.0f, 1.0f);
            float currentFillW = barWidth * remainRatio;

            if (readyBarBgSprite_) {
                readyBarBgSprite_->SetPosition({ centerX - barWidth * 0.5f - 2.0f, barY - 2.0f });
                readyBarBgSprite_->SetSize({ barWidth + 4.0f, barHeight + 4.0f });
                readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.6f * alpha });
            }
            if (readyBarFillSprite_) {
                readyBarFillSprite_->SetPosition({ centerX - barWidth * 0.5f, barY });
                readyBarFillSprite_->SetSize({ currentFillW, barHeight });
                readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, alpha }); // 赤色
            }

            if (goSprite_) {
                goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f });
            }
        } else {
            // GO! 演出
            if (readySprite_) {
                readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });
            }
            if (readyBarBgSprite_) {
                readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });
            }
            if (readyBarFillSprite_) {
                readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.0f });
            }

            if (goSprite_) {
                float goTimer = stateTimer_ - inputDelay;
                float appearDuration = 0.25f;
                float progress = (std::clamp)(goTimer / appearDuration, 0.0f, 1.0f);
                float c1 = 1.70158f;
                float c3 = c1 + 1.0f;
                float ease = 1.0f + c3 * std::pow(progress - 1.0f, 3.0f) + c1 * std::pow(progress - 1.0f, 2.0f);
                float scaleFactor = (std::max)(ease, 0.0f);

                float goScale = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteScale", 2.0f);
                float baseW = 144.0f * goScale;
                float baseH = 92.0f * goScale;
                float currentW = baseW * scaleFactor;
                float currentH = baseH * scaleFactor;
                float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "goSpriteCenterY", 360.0f);

                goSprite_->SetPosition({ centerX - currentW * 0.5f, centerY - currentH * 0.5f });
                goSprite_->SetSize({ currentW, currentH });

                float alpha = 1.0f;
                float fadeStart = startReadyTime - 0.25f;
                if (stateTimer_ > fadeStart) {
                    alpha = (std::max)(0.0f, (startReadyTime - stateTimer_) / 0.25f);
                }
                goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, alpha }); // 黄色
            }
        }

        if (stateTimer_ > startReadyTime) {
            gameState_ = GameState::Playing;
            stateTimer_ = 0.0f;
            if (readySprite_) readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });
            if (readyBarBgSprite_) readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });
            if (readyBarFillSprite_) readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.0f });
            if (goSprite_) goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f });
        }
    } else if (gameState_ == GameState::Clear) {
        stateTimer_ += dt;
        if (clearSprite_) {
            // 文字が遠くからズームして出てくる演出 (EaseOutBack)
            float appearDuration = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteAppearDuration", 0.6f);
            float progress = (std::clamp)(stateTimer_ / appearDuration, 0.0f, 1.0f);
            
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            float ease = 1.0f + c3 * std::pow(progress - 1.0f, 3.0f) + c1 * std::pow(progress - 1.0f, 2.0f);
            float scaleFactor = (std::max)(ease, 0.0f);

            float clearScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteScale", 2.0f);
            float baseW = 220.0f * clearScale;
            float baseH = 92.0f * clearScale;
            float currentW = baseW * scaleFactor;
            float currentH = baseH * scaleFactor;

            float centerX = 1280.0f * 0.5f;
            float targetCenterY = ParameterManager::GetInstance()->GetValue("GameScene", "clearSpriteCenterY", 180.0f);

            clearSprite_->SetPosition({ centerX - currentW * 0.5f, targetCenterY - currentH * 0.5f });
            clearSprite_->SetSize({ currentW, currentH });

            float alpha = (std::min)(stateTimer_ / 0.2f, 1.0f);
            clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
        }
        if (GetGameCamera()) {
            // クリア時はプレイヤーへカメラをズームイン（アップ）
            float targetScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraZoomScale", 2.2f);
            float zoomSpeed = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraZoomSpeed", 3.0f);
            float currentScale = GetGameCamera()->GetScale();
            float newScale = currentScale + (targetScale - currentScale) * (1.0f - std::exp(-zoomSpeed * dt));
            GetGameCamera()->SetScale(newScale);

            // プレイヤーを画面のやや下側に配置するため、カメラの注視点を上方にオフセット
            float targetOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraOffsetY", 1.0f);
            Vector3 currentOffset = GetGameCamera()->GetFollowOffset();
            float newOffsetY = currentOffset.y + (targetOffsetY - currentOffset.y) * (1.0f - std::exp(-zoomSpeed * dt));
            GetGameCamera()->SetFollowOffset({currentOffset.x, newOffsetY, currentOffset.z});
        }
        bool isReturn = KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::A) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start);
        if (isReturn) {
            if (GetGameCamera()) {
                GetGameCamera()->SetScale(initialCameraScale_);
                GetGameCamera()->SetFollowOffset({0.0f, 0.0f, 0.0f});
            }
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
            return;
        }
    } else {
        if (clearSprite_) {
            clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        }
        if (GetGameCamera()) {
            if (std::abs(GetGameCamera()->GetScale() - initialCameraScale_) > 0.001f) {
                GetGameCamera()->SetScale(initialCameraScale_);
            }
            if (std::abs(GetGameCamera()->GetFollowOffset().y) > 0.001f) {
                GetGameCamera()->SetFollowOffset({0.0f, 0.0f, 0.0f});
            }
        }
    }

    if (gameState_ != GameState::StartReady) {
        if (readySprite_) {
            readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });
        }
        if (readyBarBgSprite_) {
            readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });
        }
        if (readyBarFillSprite_) {
            readyBarFillSprite_->SetColor({ 1.0f, 0.2f, 0.2f, 0.0f });
        }
        if (goSprite_) {
            goSprite_->SetColor({ 1.0f, 0.95f, 0.1f, 0.0f });
        }
    }

    // ポーズ画面への移行（ゲームプレイ中にESCまたはコントローラーのSTARTボタン）
    if (gameState_ == GameState::Playing) {
        if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_ESCAPE) ||
            GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start)) {
            gameState_ = GameState::Pause;
            pauseSelectedIndex_ = 0; // 初期選択は「つづける」
        }
    } else if (gameState_ == GameState::Pause) {
        // ポーズ画面の操作（キーボード / コントローラー両対応）
        bool isMoveUp = KeyboardInput::GetInstance()->IsKeyPressed(DIK_UP) ||
                        KeyboardInput::GetInstance()->IsKeyPressed(DIK_W) ||
                        GamepadInput::GetInstance()->IsDPadPressedUp() ||
                        GamepadInput::GetInstance()->IsLeftStickPushedUp();
        bool isMoveDown = KeyboardInput::GetInstance()->IsKeyPressed(DIK_DOWN) ||
                          KeyboardInput::GetInstance()->IsKeyPressed(DIK_S) ||
                          GamepadInput::GetInstance()->IsDPadPressedDown() ||
                          GamepadInput::GetInstance()->IsLeftStickPushedDown();

        if (isMoveUp) {
            pauseSelectedIndex_ = 0;
        } else if (isMoveDown) {
            pauseSelectedIndex_ = 1;
        }

        // 決定操作（SPACE / RETURN / Pad A）
        bool isSubmit = KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) ||
                        KeyboardInput::GetInstance()->IsKeyPressed(DIK_RETURN) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::A);

        // キャンセル操作（ESC / Pad B / Pad START でゲーム画面へ戻る）
        bool isCancel = KeyboardInput::GetInstance()->IsKeyPressed(DIK_ESCAPE) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::B) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start);

        if (isCancel) {
            gameState_ = GameState::Playing;
        } else if (isSubmit) {
            if (pauseSelectedIndex_ == 0) {
                // つづける: ゲーム画面に戻る
                gameState_ = GameState::Playing;
            } else if (pauseSelectedIndex_ == 1) {
                // セレクトに戻る
                if (GetGameCamera()) {
                    GetGameCamera()->SetScale(initialCameraScale_);
                    GetGameCamera()->SetFollowOffset({0.0f, 0.0f, 0.0f});
                }
                sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kStageSelect));
                return;
            }
        }

        // ポーズ中スプライトの更新（選択中は白色、非選択は薄色）
        const Vector4 kSelectedColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        const Vector4 kUnselectedColor = { 0.45f, 0.45f, 0.45f, 0.65f };
        float centerX = 1280.0f * 0.5f;

        if (pauseOverlaySprite_) {
            pauseOverlaySprite_->SetPosition({ centerX - 5000.0f, 360.0f - 5000.0f });
            pauseOverlaySprite_->SetSize({ 10000.0f, 10000.0f });
            pauseOverlaySprite_->Update();
        }
        if (pauseContinueSprite_) {
            bool isSelected = (pauseSelectedIndex_ == 0);
            pauseContinueSprite_->SetColor(isSelected ? kSelectedColor : kUnselectedColor);
            float w = isSelected ? 410.0f : 390.0f;
            float h = isSelected ? 82.0f : 78.0f;
            pauseContinueSprite_->SetSize({ w, h });
            pauseContinueSprite_->SetPosition({ centerX - w * 0.5f, 310.0f - h * 0.5f });
            pauseContinueSprite_->Update();
        }
        if (pauseSelectSprite_) {
            bool isSelected = (pauseSelectedIndex_ == 1);
            pauseSelectSprite_->SetColor(isSelected ? kSelectedColor : kUnselectedColor);
            float w = isSelected ? 410.0f : 390.0f;
            float h = isSelected ? 82.0f : 78.0f;
            pauseSelectSprite_->SetSize({ w, h });
            pauseSelectSprite_->SetPosition({ centerX - w * 0.5f, 430.0f - h * 0.5f });
            pauseSelectSprite_->Update();
        }

        // ポーズ中は以降のゲーム更新（プレイヤー、マップ、GIF等）を停止
        return;
    }

    // ステージ配置ワールドGIFの更新
    StageGifManager::GetInstance()->Update(TimeManager::GetInstance().GetDeltaTime());

    // 4. プレイヤーの更新（入力・物理・当たり判定）
    if (player_ && map_) {
        bool isCurrentlyPlaying = true;
#ifdef USE_IMGUI
        isCurrentlyPlaying = EditorManager::IsPlaying();
#endif

        if (isCurrentlyPlaying && !wasCurrentlyPlaying_) {
            player_->FindSpawnPoint(*map_);
            if (GetGameCamera()) {
                GetGameCamera()->SetRooms(map_->GetRooms());
                GetGameCamera()->SnapToTarget();
            }
        }
        wasCurrentlyPlaying_ = isCurrentlyPlaying;


            // リプレイ再生中の場合、キーを注入し、必要に応じて位置補正を行う
            if (ReplayManager::GetInstance()->IsPlaying()) {
                // ループ判定を再生処理の一番最初で行う
                if (ReplayManager::GetInstance()->GetCurrentFrame() >= ReplayManager::GetInstance()->GetCurrentReplay().totalFrames) {
                    if (ReplayManager::GetInstance()->IsLoopPlay()) {
                        ReplayManager::GetInstance()->SetCurrentFrame(0); // これにより forceSnapNextFrame_ = true がセットされる
                    } else {
                        ReplayManager::GetInstance()->StopPlayback();
                    }
                }

                if (ReplayManager::GetInstance()->IsPlaying()) {
                    bool shouldRebuildState = !wasPlayingLastFrame_ || ReplayManager::GetInstance()->IsForceSnapNextFrame();
                if (shouldRebuildState) {
                    auto& replayData = ReplayManager::GetInstance()->GetCurrentReplay();
                    int curFrame = ReplayManager::GetInstance()->GetCurrentFrame();

                    // 1. マップを初期状態（文字列）から復元
                    if (!replayData.mapDataStr.empty()) {
                        map_->LoadFromString(replayData.mapDataStr);
                    }
                    
                    // 2. プレイヤー状態(速度含む)とスコアをリセット
                    player_->ResetState(replayData.playerInitPos);
                    player_->ClearEffects();
                    for (auto& effect : effects_) {
                        if (effect) effect->Reset();
                    }
                    
                    // 3. 0フレーム目から現在フレームまで、記録された座標をたどってコインを回収
                    for (int i = 0; i <= curFrame; ++i) {
                        player_->SetPosition(replayData.frames[i].position);
                    }
                    map_->SetRebuildEnabled(true);

                    // 4. コイン回収用に座標を動かしたので、シミュレーション再開用の正しい座標に戻す
                    if (curFrame == 0) {
                        player_->SetPosition(replayData.playerInitPos);
                    } else {
                        player_->SetPosition(replayData.frames[curFrame - 1].position);
                    }
                    previousScore_ = player_->GetScore();
                }

                if (!wasPlayingLastFrame_) {
                    // 再生開始時に初期位置へ自動ワープ
                    player_->SetPosition(ReplayManager::GetInstance()->GetCurrentReplay().playerInitPos);
                    if (GetGameCamera()) {
                        if (ReplayManager::GetInstance()->IsSnapEnabled()) {
                            // 再生中は自動追従を一時的に無効化し、記録されたカメラ座標に同期させる
                            GetGameCamera()->SetFollowTarget(nullptr);
                        } else {
                            // 座標補正がOFF（TASモード）の場合はプレイヤーに追従させる
                            GetGameCamera()->SetFollowTarget(&player_->GetPosition());
                        }
                        GetGameCamera()->SetTranslation(ReplayManager::GetInstance()->GetCurrentReplay().cameraInitPos);
                    }
                    wasPlayingLastFrame_ = true;
                }
                Vector3 pos = player_->GetPosition();
                Vector3 camPos = GetGameCamera() ? GetGameCamera()->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
                ReplayManager::GetInstance()->UpdatePlayback(pos, camPos);
                player_->SetPosition(pos);
                if (GetGameCamera()) {
                    if (ReplayManager::GetInstance()->IsSnapEnabled()) {
                        GetGameCamera()->SetFollowTarget(nullptr);
                        GetGameCamera()->SetTranslation(camPos);
                    } else {
                        GetGameCamera()->SetFollowTarget(&player_->GetPosition());
                        // 追従モードのため、SetTranslation(camPos) は実行しない
                    }
                }
                }
            } else {
                if (wasPlayingLastFrame_) {
                    // リプレイが終了した（またはTAKEOVERで停止した）瞬間に、カメラの追従を復元する
                    if (GetGameCamera()) {
                        GetGameCamera()->SetFollowTarget(&player_->GetPosition());
                    }
                }
                wasPlayingLastFrame_ = false;

                // 巻き戻しから通常に戻ったときにカメラ追従を再開する
            }

            if (player_) {
                bool isRespawning = player_->IsRespawning();
                if (isRespawning && !wasRespawning_) {
                    for (auto& effect : effects_) {
                        if (effect) effect->Reset();
                    }
                    previousScore_ = player_->GetScore();
                }
                wasRespawning_ = isRespawning;
            }

            if (GetGameCamera() && !ReplayManager::GetInstance()->IsPlaying()) {
                if (player_->IsDead()) {
                    GetGameCamera()->SetFollowTarget(nullptr);
                } else {
                    GetGameCamera()->SetFollowTarget(&player_->GetPosition());
                }
            }

            if (GetGameCamera() && map_ && gameState_ != GameState::Clear) {
                GetGameCamera()->SetRooms(map_->GetRooms());
            }

            // マップの更新をプレイヤーより先に行う（移動リフト等の新しい座標に対して判定するため）
            if (map_) {
                map_->Update();
            }

            bool canControl = true;
            if (gameState_ == GameState::StartReady) {
                float inputDelay = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyInputDelay", 1.0f);
                if (stateTimer_ < inputDelay) {
                    canControl = false;
                }
            } else if (gameState_ == GameState::Clear) {
                canControl = false;
            }

            player_->UpdateWithMap(*map_, GetGameCamera() && GetGameCamera()->IsTransitioning(), canControl);

            // ゴール判定
            if (gameState_ == GameState::Playing && player_->IsGoalComplete()) {
                gameState_ = GameState::Clear;
                stateTimer_ = 0.0f;
                if (GetGameCamera()) {
                    GetGameCamera()->SetRooms({}); // 部屋境界制限を解除してプレイヤー中心へ直接追従
                }
            }

            // コイン獲得エフェクト
            int currentScore = player_->GetScore();
            if (currentScore > previousScore_) {
                if (coinEffect_) {
                    Vector3 playerPos = player_->GetPosition();
                    float playerWidth = 1.0f; // 実際のプレイヤーサイズに合わせて調整
                    float playerHeight = 1.0f;
                    Vector3 hitEmitterPos = {
                        playerPos.x + playerWidth / 2.0f,
                        playerPos.y + playerHeight / 2.0f,
                        0.0f
                    };
                    coinEffect_->Emit(hitEmitterPos);
                }
                previousScore_ = currentScore;
            }

        // 右スティック（RStick）によるカメラ見渡し回転
        auto gamepad = GamepadInput::GetInstance();
        if (gamepad && gamepad->IsConnected()) {
            Vector2 rStick = gamepad->GetRightStick();
            if (std::abs(rStick.x) > 0.15f || std::abs(rStick.y) > 0.15f) {
                float targetYaw = -rStick.x * 0.6f;   // 左右見渡し（水平回転）
                float targetPitch = rStick.y * 0.4f;  // 上下見渡し（垂直チルト）
                cameraLookRotation_.y += (targetYaw - cameraLookRotation_.y) * 0.1f;
                cameraLookRotation_.x += (targetPitch - cameraLookRotation_.x) * 0.1f;
            } else {
                cameraLookRotation_.y += (0.0f - cameraLookRotation_.y) * 0.1f;
                cameraLookRotation_.x += (0.0f - cameraLookRotation_.x) * 0.1f;
            }
            if (GetGameCamera()) {
                Vector3 baseRot = GetGameCamera()->GetBaseRotation();
                GetGameCamera()->SetRotation({
                    baseRot.x + cameraLookRotation_.x,
                    baseRot.y + cameraLookRotation_.y,
                    baseRot.z
                });
            }
        }

        // プレイ中の場合、リプレイ録画を行う
        if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying()) {
            Vector3 camPos = GetGameCamera() ? GetGameCamera()->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
            if (!ReplayManager::GetInstance()->IsRecording()) {
                std::string mapStr = map_ ? map_->GetMapDataAsString() : "";
                ReplayManager::GetInstance()->StartRecord(player_->GetPosition(), camPos, mapStr);
            }
            Vector4 pColor = player_->GetPrimitiveObject() ? player_->GetPrimitiveObject()->GetMaterial().color : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            Vector3 pScale = player_->GetPrimitiveObject() ? player_->GetPrimitiveObject()->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
            Vector3 pRot = player_->GetPrimitiveObject() ? player_->GetPrimitiveObject()->GetRotation() : Vector3(0.0f, 0.0f, 0.0f);
            ReplayManager::GetInstance()->RecordFrame(player_->GetPosition(), camPos, pColor, pScale, pRot);
        } else {
            if (ReplayManager::GetInstance()->IsRecording()) {
                ReplayManager::GetInstance()->StopRecord();
            }
        }
    }

    UpdateCoinUI(dt);
}

void GameScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI

    if (skybox_) {
        skybox_->DrawImGui();
    }

    if (player_ && player_->GetPrimitiveObject() == selectedPrimitive) {
        player_->DisplayImGui();
    }

    // エディター側でプレイ状態になっていないときは、インゲームUI（スコア等）を描画しない
    if (!EditorManager::IsPlaying() || gameState_ == GameState::Pause) {
        return;
    }

    ImVec2 windowPos = ImVec2(0.0f, 0.0f);
    float windowWidth = 1280.0f;
    float windowHeight = 720.0f;

    windowPos = EditorManager::GetGameViewPos();
    windowWidth = EditorManager::GetGameViewSize().x;
    windowHeight = EditorManager::GetGameViewSize().y;

    // スコアの簡易表示
    if (player_) {
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + 10.0f, windowPos.y + 10.0f), ImGuiCond_Always);
        ImGui::Begin("Game HUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Score: %d", player_->GetScore());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();

        // 操作ガイド
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + 10.0f, windowPos.y + 70.0f), ImGuiCond_Always);
        ImGui::Begin("Operation Guide", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);

        ImGui::TextColored(ImVec4(1,1,1,0.8f), "[Operation Guide]");
        if (GamepadInput::GetInstance()->IsConnected()) {
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "LStick / D-Pad : Move (Up/Down to Climb)");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "[A] : Jump / Wall Jump");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "[X] : Dash");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "[RB] / [RT] : Wall Cling");
        } else {
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "A/D or Left/Right : Move");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "SPACE : Jump / Wall Jump");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "J : Dash");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "K : Wall Cling (W/S to Climb)");
        }
        ImGui::End();
    }

    // Game Over (ミス) 演出
    if (player_ && player_->IsDead()) {
        ImGui::SetNextWindowPos(windowPos);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
        ImGui::Begin("GameOverOverlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        drawList->AddRectFilled(p, ImVec2(p.x + windowWidth, p.y + windowHeight), IM_COL32(255, 0, 0, 100)); 

        ImGui::SetCursorPos(ImVec2(windowWidth/2.0f - 150.0f, windowHeight/2.0f - 50.0f));
        ImGui::SetWindowFontScale(6.0f);
        const char* text = "MISS!";
        float textW = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textW) * 0.5f);
        ImGui::TextColored(ImVec4(1,1,1,1), "%s", text);
        ImGui::End();
    }

    // フェードイン/アウト画面遷移演出
    if (transitionAlpha_ > 0.0f) {
        ImGui::SetNextWindowPos(windowPos);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
        ImGui::Begin("TransitionOverlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        drawList->AddRectFilled(p, ImVec2(p.x + windowWidth, p.y + windowHeight), IM_COL32(0, 0, 0, static_cast<int>(transitionAlpha_ * 255.0f)));
        ImGui::End();
    }
#endif
}

void GameScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    OutputDebugStringA("[DEBUG] GameScene::Draw Start\n");

    // 1. Skyboxの描画
    if (skybox_) {
        OutputDebugStringA("[DEBUG] GameScene::Draw Skybox Start\n");
        skybox_->Draw();
        OutputDebugStringA("[DEBUG] GameScene::Draw Skybox End\n");
    }

    // 2. 3Dモデル（マップ・プレイヤー）の描画準備
    if (GetModelCommon()) {
        OutputDebugStringA("[DEBUG] GameScene::Draw ModelCommon::PreDraw Start\n");
        GetModelCommon()->PreDraw();
        OutputDebugStringA("[DEBUG] GameScene::Draw ModelCommon::PreDraw End\n");
    }

    // マップの描画
    if (map_) {
        map_->Draw();
    }

    // ステージ配置ワールドGIF（板ポリ）の描画
    StageGifManager::GetInstance()->Draw();

    // プレイヤーの描画
    if (player_) {
        player_->Draw();
    }


    // コンポーネントの描画を実行
    Renderer::GetInstance()->RenderComponents();

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
            if (GetParticleCommon()) {
                GetParticleCommon()->PreDraw();
                auto commandList = DirectXCommon::GetInstance()->GetCommandList();
                Matrix4x4 cameraMatrix = TransformFunctions::Inverse(CameraManager::GetInstance()->GetViewMatrix());
                ModelManager* modelManager = ModelManager::GetInstance();
                snowParticle_->Draw(commandList, viewProjectionMatrix, cameraMatrix, GetParticleCommon(), modelManager);
            }
#ifdef USE_IMGUI
        }
#endif
    }

#ifdef USE_IMGUI
    // --- ゴースト残像の描画（マリオメーカー仕様） ---
    // エディタ停止中で、かつリプレイの再生/録画もしていない時に「選択中のリプレイ全体」の軌跡を表示する
    if (!EditorManager::IsPlaying() && player_) {
        ReplayManager* replayManager = ReplayManager::GetInstance();
        if (replayManager && !replayManager->IsPlaying() && !replayManager->IsRecording()) {
            const ReplayData& currentReplay = replayManager->GetCurrentReplay();
            if (!currentReplay.frames.empty()) {
                const float GHOST_ALPHA = 0.5f;
                const int FRAME_STEP = 10;
                auto* playerPrim = const_cast<PrimitiveObject*>(player_->GetPrimitiveObject());
                if (playerPrim && playerPrim->GetShowTrail()) {
                    playerPrim->ResetGhostIndex();
                    for (int i = 0; i < static_cast<int>(currentReplay.frames.size()); i += FRAME_STEP) {
                        const FrameData& frameData = currentReplay.frames[i];
                        if (i >= FRAME_STEP) {
                            int prevIndex = i - FRAME_STEP;
                            if (prevIndex >= 0 && prevIndex < static_cast<int>(currentReplay.frames.size())) {
                                Vector3 diff;
                                diff.x = frameData.position.x - currentReplay.frames[prevIndex].position.x;
                                diff.y = frameData.position.y - currentReplay.frames[prevIndex].position.y;
                                diff.z = frameData.position.z - currentReplay.frames[prevIndex].position.z;
                                float distSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                                if (distSq < 0.0001f) {
                                    continue;
                                }
                            }
                        }
                        EulerTransform ghostTransform = playerPrim->GetTransform();
                        ghostTransform.translate = frameData.position;
                        ghostTransform.scale = frameData.scale;
                        ghostTransform.rotate = frameData.rotation;

                        Material ghostMaterial = playerPrim->GetMaterial();
                        Vector4 ghostColor = frameData.color;
                        ghostColor.w *= GHOST_ALPHA;
                        ghostMaterial.color = ghostColor;

                        playerPrim->DrawGhost(ghostTransform, ghostMaterial);
                    }
                }
            }

            // --- 物理ベースA* 探索ルート（AIゴースト）の描画 ---
            const auto& aiPath = replayManager->GetAIPathPositions();
            if (replayManager->IsShowAIGhost() && !aiPath.empty()) {
                auto* playerPrim = const_cast<PrimitiveObject*>(player_->GetPrimitiveObject());
                if (playerPrim) {
                    const int AI_STEP = 5;
                    const float GHOST_ALPHA = 0.6f;
                    for (int i = 0; i < static_cast<int>(aiPath.size()); i += AI_STEP) {
                        const Vector3& pos = aiPath[i];
                        EulerTransform ghostTransform = playerPrim->GetTransform();
                        ghostTransform.translate = pos;

                        Material ghostMaterial = playerPrim->GetMaterial();
                        ghostMaterial.color = Vector4{ 0.0f, 0.9f, 1.0f, GHOST_ALPHA };

                        playerPrim->DrawGhost(ghostTransform, ghostMaterial);
                    }
                }
            }
        }
    }
#endif

    // 3. パーティクルの描画
    // 描画前処理
    GetParticleCommon()->PreDraw();

    // パーティクルの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects() || ReplayManager::GetInstance()->IsPlaying()) {
#endif
        GetParticleCommon()->DrawAll(viewProjectionMatrix);
#ifdef USE_IMGUI
    }
#endif
}

void GameScene::Draw2D() {
    bool isPlayingOrReplaying = false;
#ifdef USE_IMGUI
    if (EditorManager::IsPlaying()) {
        isPlayingOrReplaying = true;
    }
#else
    isPlayingOrReplaying = true;
#endif
    if (ReplayManager::GetInstance()->IsPlaying()) {
        isPlayingOrReplaying = true;
    }

    if (isPlayingOrReplaying && gameState_ != GameState::Pause) {
        if (GetSpriteCommon()) {
            GetSpriteCommon()->PreDraw();
            DrawCoinUI();
        }
    }
    if (gameState_ == GameState::StartReady) {
        if (GetSpriteCommon()) {
            GetSpriteCommon()->PreDraw();
            float inputDelay = ParameterManager::GetInstance()->GetValue("GameScene", "startReadyInputDelay", 1.0f);
            if (stateTimer_ < inputDelay) {
                if (readyBarBgSprite_) readyBarBgSprite_->Draw();
                if (readyBarFillSprite_) readyBarFillSprite_->Draw();
                if (readySprite_) readySprite_->Draw();
            } else if (stateTimer_ >= inputDelay && goSprite_) {
                goSprite_->Draw();
            }
        }
    } else if (gameState_ == GameState::Clear) {
        if (GetSpriteCommon() && clearSprite_) {
            GetSpriteCommon()->PreDraw();
            clearSprite_->Draw();
        }
    } else if (gameState_ == GameState::Pause) {
        if (GetSpriteCommon()) {
            GetSpriteCommon()->PreDraw();
            if (pauseOverlaySprite_) pauseOverlaySprite_->Draw();
            if (pauseContinueSprite_) pauseContinueSprite_->Draw();
            if (pauseSelectSprite_) pauseSelectSprite_->Draw();
        }
    }
}

void GameScene::DrawEditorOverlay(const Matrix4x4 &viewProjectionMatrix) {
#ifdef USE_IMGUI
    if (!EditorManager::IsPlaying() && player_) {
        ReplayManager* replayManager = ReplayManager::GetInstance();
        if (replayManager && !replayManager->IsPlaying() && !replayManager->IsRecording()) {
            auto* playerPrim = const_cast<PrimitiveObject*>(player_->GetPrimitiveObject());
            ImVec2 gameViewPos = EditorManager::GetGameViewPos();
            ImVec2 gameViewSize = EditorManager::GetGameViewSize();
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 1. プレイヤー残像の2D軌跡描画
            if (playerPrim && playerPrim->GetShowTrail()) {
                const ReplayData& currentReplay = replayManager->GetCurrentReplay();
                if (!currentReplay.frames.empty()) {
                    const int FRAME_STEP = 10;
                    drawList->PushClipRect(gameViewPos, ImVec2(gameViewPos.x + gameViewSize.x, gameViewPos.y + gameViewSize.y), true);

                    for (int i = 0; i < static_cast<int>(currentReplay.frames.size()); i += FRAME_STEP) {
                        const FrameData& frameData = currentReplay.frames[i];
                        if (i >= FRAME_STEP) {
                            int prevIndex = i - FRAME_STEP;
                            if (prevIndex >= 0 && prevIndex < static_cast<int>(currentReplay.frames.size())) {
                                Vector3 diff;
                                diff.x = frameData.position.x - currentReplay.frames[prevIndex].position.x;
                                diff.y = frameData.position.y - currentReplay.frames[prevIndex].position.y;
                                diff.z = frameData.position.z - currentReplay.frames[prevIndex].position.z;
                                float distSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                                if (distSq < 0.0001f) {
                                    continue;
                                }
                            }
                        }

                        Vector3 ndcCurr = TransformFunctions::EulerTransform(frameData.position, viewProjectionMatrix);
                        ImVec2 pCurr;
                        bool isCurrVisible = false;

                        if (ndcCurr.z >= 0.0f && ndcCurr.z <= 1.0f) {
                            isCurrVisible = true;
                            pCurr = ImVec2(
                                gameViewPos.x + (ndcCurr.x + 1.0f) * 0.5f * gameViewSize.x,
                                gameViewPos.y + (1.0f - ndcCurr.y) * 0.5f * gameViewSize.y
                            );
                            drawList->AddCircleFilled(pCurr, 4.0f, IM_COL32(255, 50, 50, 255));
                        }

                        if (i >= FRAME_STEP) {
                            int prevIndex = i - FRAME_STEP;
                            if (prevIndex >= 0 && prevIndex < static_cast<int>(currentReplay.frames.size())) {
                                Vector3 ndcPrev = TransformFunctions::EulerTransform(currentReplay.frames[prevIndex].position, viewProjectionMatrix);
                                if (isCurrVisible && ndcPrev.z >= 0.0f && ndcPrev.z <= 1.0f) {
                                    ImVec2 pPrev(
                                        gameViewPos.x + (ndcPrev.x + 1.0f) * 0.5f * gameViewSize.x,
                                        gameViewPos.y + (1.0f - ndcPrev.y) * 0.5f * gameViewSize.y
                                    );
                                    drawList->AddLine(pPrev, pCurr, IM_COL32(255, 200, 0, 255), 2.0f);
                                }
                            }
                        }
                    }
                    drawList->PopClipRect();
                }
            }

            // 2. 物理ベースA* AIゴーストの2D軌跡描画
            const auto& aiPath = replayManager->GetAIPathPositions();
            if (replayManager->IsShowAIGhost() && !aiPath.empty()) {
                if (playerPrim) {
                    drawList->PushClipRect(gameViewPos, ImVec2(gameViewPos.x + gameViewSize.x, gameViewPos.y + gameViewSize.y), true);
                    const int AI_STEP = 5;

                    for (int i = 0; i < static_cast<int>(aiPath.size()); i += AI_STEP) {
                        const Vector3& pos = aiPath[i];
                        Vector3 ndcCurr = TransformFunctions::EulerTransform(pos, viewProjectionMatrix);
                        if (ndcCurr.z >= 0.0f && ndcCurr.z <= 1.0f) {
                            ImVec2 pCurr(
                                gameViewPos.x + (ndcCurr.x + 1.0f) * 0.5f * gameViewSize.x,
                                gameViewPos.y + (1.0f - ndcCurr.y) * 0.5f * gameViewSize.y
                            );
                            drawList->AddCircleFilled(pCurr, 3.5f, IM_COL32(0, 220, 255, 255));

                            if (i >= AI_STEP) {
                                Vector3 ndcPrev = TransformFunctions::EulerTransform(aiPath[i - AI_STEP], viewProjectionMatrix);
                                if (ndcPrev.z >= 0.0f && ndcPrev.z <= 1.0f) {
                                    ImVec2 pPrev(
                                        gameViewPos.x + (ndcPrev.x + 1.0f) * 0.5f * gameViewSize.x,
                                        gameViewPos.y + (1.0f - ndcPrev.y) * 0.5f * gameViewSize.y
                                    );
                                    drawList->AddLine(pPrev, pCurr, IM_COL32(0, 220, 255, 255), 2.0f);
                                }
                            }
                        }
                    }
                    drawList->PopClipRect();
                }
            }
        }
    }
#endif
}

std::vector<ParticleManager *> GameScene::GetParticles() {
    std::vector<ParticleManager *> result;
    return result;
}

std::vector<Object3D *> GameScene::GetObjects() {
    std::vector<Object3D *> result;
    return result;
}

std::vector<PrimitiveObject *> GameScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;

    // 1. エフェクト群（BaseEffectのポリモーフィズム）
    for (const auto& effect : effects_) {
        if (effect) {
            auto effectPrims = effect->GetPrimitives();
            result.insert(result.end(), effectPrims.begin(), effectPrims.end());
        }
    }

    // 2. プレイヤー
    if (player_) {
        result.push_back(const_cast<PrimitiveObject*>(player_->GetPrimitiveObject()));
    }

    // 3. マップチップ
    if (map_) {
        auto mapPrims = map_->GetPrimitiveObjects();
        result.insert(result.end(), mapPrims.begin(), mapPrims.end());
    }

    return result;
}

void GameScene::UpdateEditor() {
    float dt = TimeManager::GetInstance().GetDeltaTime();

    // ステージ配置ワールドGIFの更新 (エディタ停止中もアニメーション再生＆トランスフォーム反映)
    StageGifManager::GetInstance()->Update(dt);

    // GPUパーティクル (snow) の更新（エディタ停止中も画面右端から常時降らせる）
    if (snowParticle_) {
        Vector3 camPos = GetGameCamera() ? GetGameCamera()->GetTranslation() : CameraManager::GetInstance()->GetCameraPos();
        float halfW = (GetGameCamera() && GetGameCamera()->IsOrthographic()) ? (GetGameCamera()->GetOrthoWidth() * 0.5f) : 10.0f;
        Vector3 snowPos = { camPos.x + halfW, camPos.y, 0.0f };
        snowParticle_->SetPosition(snowPos);
        snowParticle_->Update(dt);
    }

    // フェードイン演出 (エディタ停止中もフェードインさせる)
    if (transitionAlpha_ > 0.0f) {
        transitionAlpha_ -= dt * 1.5f;
        if (transitionAlpha_ < 0.0f) transitionAlpha_ = 0.0f;
    }

    // 録画状態のままエディタが停止した場合、確実に停止させて履歴に保存する
    if (ReplayManager::GetInstance()->IsRecording()) {
        ReplayManager::GetInstance()->StopRecord();
    }

    // エディタ停止中もマップの変更（isDirty_時の再構築など）に追従させる
    if (map_) {
        map_->Update();
    }

    // エディタ停止中もマップの変更に追従してプレイヤー座標を更新
    if (player_) {
        if (map_) {
            player_->FindSpawnPoint(*map_);
        }
        auto* playerPrim = const_cast<PrimitiveObject*>(player_->GetPrimitiveObject());
        if (playerPrim) {
            playerPrim->SetTranslation(player_->GetPosition());
            playerPrim->Update();
        }
    }

    // エディタ停止中もカメラをプレイヤー位置にスナップ・追従させる
    if (GetGameCamera() && player_) {
        if (!GetGameCamera()->IsOrthographic()) {
            float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
            float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
            GetGameCamera()->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);
        }
        if (map_) {
            GetGameCamera()->SetRooms(map_->GetRooms());
        }
        GetGameCamera()->SetFollowTarget(&player_->GetPosition());
        GetGameCamera()->SetFollowEnabled(true);
        GetGameCamera()->SnapToTarget();
    }

    if (skybox_) {
        skybox_->Update();
    }
}

void GameScene::UpdateCoinUI(float deltaTime) {
    if (!map_) return;
    int collectedCoins = map_->GetCollectedCoinCount();
    if (collectedCoins > lastCollectedCoins_) {
        coinPopAnimTimer_ = 0.35f;
    }
    lastCollectedCoins_ = collectedCoins;

    if (coinPopAnimTimer_ > 0.0f) {
        coinPopAnimTimer_ -= deltaTime;
        if (coinPopAnimTimer_ < 0.0f) {
            coinPopAnimTimer_ = 0.0f;
        }
    }
}

void GameScene::DrawCoinUI() {
    if (!map_ || !coinSprite_ || !GetSpriteCommon()) return;
    int totalCoins = map_->GetTotalCoinCount();
    if (totalCoins <= 0) return;

    int collectedCoins = map_->GetCollectedCoinCount();
    std::string text = std::to_string(collectedCoins) + "/" + std::to_string(totalCoins);

    float baseX = ParameterManager::GetInstance()->GetValue("CoinUI", "posX", 40.0f);
    float baseY = ParameterManager::GetInstance()->GetValue("CoinUI", "posY", 35.0f);
    float iconSize = ParameterManager::GetInstance()->GetValue("CoinUI", "iconSize", 44.0f);
    float charW = ParameterManager::GetInstance()->GetValue("CoinUI", "charWidth", 24.0f);
    float charH = ParameterManager::GetInstance()->GetValue("CoinUI", "charHeight", 32.0f);
    float spacing = ParameterManager::GetInstance()->GetValue("CoinUI", "spacing", 2.0f);

    float scaleAnim = 1.0f;
    if (coinPopAnimTimer_ > 0.0f) {
        float progress = coinPopAnimTimer_ / 0.35f;
        scaleAnim = 1.0f + 0.4f * std::sin(progress * 3.14159265f);
    }

    float currentIconSize = iconSize * scaleAnim;
    float iconOffset = (currentIconSize - iconSize) * 0.5f;
    coinSprite_->SetPosition({ baseX - iconOffset, baseY - iconOffset });
    coinSprite_->SetSize({ currentIconSize, currentIconSize });
    coinSprite_->Update();
    coinSprite_->Draw();

    float currentX = baseX + iconSize + 12.0f;
    float textY = baseY + (iconSize - charH) * 0.5f;

    size_t spriteIdx = 0;
    for (char c : text) {
        if (spriteIdx >= numberSprites_.size()) break;
        int charIndex = -1;
        if (c >= '0' && c <= '9') {
            charIndex = c - '0';
        } else if (c == '/') {
            charIndex = 10;
        }

        if (charIndex >= 0) {
            auto& numSprite = numberSprites_[spriteIdx];
            numSprite->SetTextureRect(charIndex * 48.0f, 0.0f, 48.0f, 64.0f);
            numSprite->SetPosition({ currentX, textY });
            numSprite->SetSize({ charW, charH });
            numSprite->Update();
            numSprite->Draw();
            spriteIdx++;
        }
        currentX += charW + spacing;
    }
}