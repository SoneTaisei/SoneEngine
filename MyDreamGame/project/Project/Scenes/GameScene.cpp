#include "GameScene.h"
#include <Windows.h>
#include "Scene/SceneManager.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/GameCamera.h"
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
#include "Core/Utility/ParameterManager.h"

std::string GameScene::s_TargetMapFilePath = "resources/json/shared/Map/map_data.json";

void GameScene::OnEnter(SceneManager* sceneManager) {
    // StageSelectSceneから選択されたステージのパスを受け取る
    if (sceneManager->HasData("SelectedStagePath")) {
        std::string selectedPath = sceneManager->GetData<std::string>("SelectedStagePath");
        if (!selectedPath.empty()) {
            s_TargetMapFilePath = selectedPath;
            // TODO: マップの再読み込みなどをここで行うか、Initializeのタイミングと調整する
        }
    }
}

void GameScene::OnExit(SceneManager* sceneManager) {
    // スコアなどを保存してTitleやStageSelectに渡す
    if (player_) {
        sceneManager->SetData("LastScore", player_->GetScore());
    }
    if (gameCamera_) {
        gameCamera_->SetScale(initialCameraScale_);
        gameCamera_->SetFollowOffset({ 0.0f, 0.0f, 0.0f });
    }
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
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/qwantani_dusk_2_puresky_2k/qwantani_dusk_2_puresky_2k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));
    Log("GameScene::Initialize: Skybox loaded\n");

    // CoinEffectの作成（コイン取得用）
    coinEffect_ = std::make_unique<CoinEffect>();
    coinEffect_->Initialize(DirectXCommon::GetInstance()->GetDevice());
    Log("GameScene::Initialize: CoinEffect Initialized\n");

    uint32_t gradationHandle = TextureManager::GetInstance()->Load("resources/Sprite/School/gradationLine.png");
    ringEffect_ = std::make_unique<RingEffect>();
    ringEffect_->Initialize(device.Get(), gradationHandle);
    cylinderEffect_ = std::make_unique<CylinderEffect>();
    cylinderEffect_->Initialize(device.Get(), gradationHandle);
    Log("GameScene::Initialize: Effects Initialized\n");

    // 5. マップの生成と初期化
    map_ = std::make_unique<MapChip2D>();
    map_->Initialize( s_TargetMapFilePath);
    Log("GameScene::Initialize: Map Initialized\n");

    // 6. プレイヤーの生成と初期化
    playerObj_ = std::make_unique<GameObject>("Player");
    playerObj_->AddComponent<TransformComponent>();
    player_ = playerObj_->AddComponent<Player2D>();
    player_->SetCamera(gameCamera_); // 画面揺れ連携用にカメラを渡す
    Log("GameScene::Initialize: Player Initialized\n");
    
    player_->FindSpawnPoint(*map_);
    Log("GameScene::Initialize: Player SpawnPoint found\n");

    // 7. GameCameraを正射影モード（2D表示）に切り替え
    if (gameCamera_) {
        Log("GameScene::Initialize: Camera config...\n");
        float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
        float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
        gameCamera_->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);
        if (map_) {
            gameCamera_->SetRooms(map_->GetRooms());
        }
        // プレイヤーの位置をカメラ追従ターゲットに設定
        gameCamera_->SetFollowTarget(&player_->GetPosition());
        gameCamera_->SnapToTarget();
        initialCameraScale_ = gameCamera_->GetScale();
        Log("GameScene::Initialize: Camera configured\n");
    }

    // 8. 開始演出用スプライトの初期化 (READY / GO / タイムバー)
    readyTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/ready.png");
    goTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/go.png");
    whiteTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/white.png");
    if (spriteCommon_) {
        float centerX = 1280.0f * 0.5f;

        readySprite_ = std::make_unique<Sprite>();
        readySprite_->Initialize(spriteCommon_, readyTextureHandle_);
        float readyScale = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteScale", 2.0f);
        float readyW = 240.0f * readyScale;
        float readyH = 92.0f * readyScale;
        float centerY = ParameterManager::GetInstance()->GetValue("GameScene", "readySpriteCenterY", 360.0f);
        readySprite_->SetPosition({ centerX - readyW * 0.5f, centerY - readyH * 0.5f });
        readySprite_->SetSize({ readyW, readyH });
        readySprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f }); // 赤色

        goSprite_ = std::make_unique<Sprite>();
        goSprite_->Initialize(spriteCommon_, goTextureHandle_);
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
        readyBarBgSprite_->Initialize(spriteCommon_, whiteTextureHandle_);
        readyBarBgSprite_->SetPosition({ centerX - barWidth * 0.5f - 2.0f, barY - 2.0f });
        readyBarBgSprite_->SetSize({ barWidth + 4.0f, barHeight + 4.0f });
        readyBarBgSprite_->SetColor({ 0.1f, 0.1f, 0.1f, 0.0f });

        readyBarFillSprite_ = std::make_unique<Sprite>();
        readyBarFillSprite_->Initialize(spriteCommon_, whiteTextureHandle_);
        readyBarFillSprite_->SetPosition({ centerX - barWidth * 0.5f, barY });
        readyBarFillSprite_->SetSize({ barWidth, barHeight });
        readyBarFillSprite_->SetColor({ 1.0f, 0.15f, 0.15f, 0.0f });

        Log("GameScene::Initialize: Ready/Go Sprites Initialized\n");
    }

    // 9. クリア演出用スプライトの初期化
    clearTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/clear.png");
    if (spriteCommon_) {
        clearSprite_ = std::make_unique<Sprite>();
        clearSprite_->Initialize(spriteCommon_, clearTextureHandle_);
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

    bool isGameActive = isPlayingOrReplaying && !ReplayManager::GetInstance()->IsPaused();

    if (isGameActive) {
        if (coinEffect_) {
            coinEffect_->Update(1.0f / 60.0f);
        }
        if (ringEffect_) {
            ringEffect_->Update(1.0f / 60.0f);
        }
        if (cylinderEffect_) {
            cylinderEffect_->Update(1.0f / 60.0f);
        }
    }

    if (skybox_) {
        skybox_->Update();
    }

    float dt = TimeManager::GetInstance().GetDeltaTime();
    
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
        if (gameCamera_) {
            // クリア時はプレイヤーへカメラをズームイン（アップ）
            float targetScale = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraZoomScale", 2.2f);
            float zoomSpeed = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraZoomSpeed", 3.0f);
            float currentScale = gameCamera_->GetScale();
            float newScale = currentScale + (targetScale - currentScale) * (1.0f - std::exp(-zoomSpeed * dt));
            gameCamera_->SetScale(newScale);

            // プレイヤーを画面のやや下側に配置するため、カメラの注視点を上方にオフセット
            float targetOffsetY = ParameterManager::GetInstance()->GetValue("GameScene", "clearCameraOffsetY", 1.0f);
            Vector3 currentOffset = gameCamera_->GetFollowOffset();
            float newOffsetY = currentOffset.y + (targetOffsetY - currentOffset.y) * (1.0f - std::exp(-zoomSpeed * dt));
            gameCamera_->SetFollowOffset({ currentOffset.x, newOffsetY, currentOffset.z });
        }
        bool isReturn = KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::A) ||
                        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start);
        if (isReturn) {
            if (gameCamera_) {
                gameCamera_->SetScale(initialCameraScale_);
                gameCamera_->SetFollowOffset({ 0.0f, 0.0f, 0.0f });
            }
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
            return;
        }
    } else {
        if (clearSprite_) {
            clearSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        }
        if (gameCamera_) {
            if (std::abs(gameCamera_->GetScale() - initialCameraScale_) > 0.001f) {
                gameCamera_->SetScale(initialCameraScale_);
            }
            if (std::abs(gameCamera_->GetFollowOffset().y) > 0.001f) {
                gameCamera_->SetFollowOffset({ 0.0f, 0.0f, 0.0f });
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

    // 4. プレイヤーの更新（入力・物理・当たり判定）
    if (player_ && map_) {
        bool isCurrentlyPlaying = true;
#ifdef USE_IMGUI
        isCurrentlyPlaying = EditorManager::IsPlaying();
#endif

        if (isCurrentlyPlaying && !wasCurrentlyPlaying_) {
            player_->FindSpawnPoint(*map_);
            if (gameCamera_) {
                gameCamera_->SetRooms(map_->GetRooms());
                gameCamera_->SnapToTarget();
            }
        }
        wasCurrentlyPlaying_ = isCurrentlyPlaying;

        bool isRewinding = false;
        if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying()) {
            auto keyboard = KeyboardInput::GetInstance();
            auto gamepad = GamepadInput::GetInstance();
            bool keyRewind = (keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL)) &&
                             keyboard->IsKeyDown(DIK_LEFT);
            bool padRewind = gamepad->IsButtonDown(GamepadButton::LB) || (gamepad->GetLeftTrigger() > 0.3f);
            if (keyRewind || padRewind) {
                isRewinding = true;
            }
        }

        if (isRewinding) {
            FrameData poppedFrame;
            if (ReplayManager::GetInstance()->PopRecordedFrame(poppedFrame)) {
                player_->SetPosition(poppedFrame.position);
                if (auto* prim = player_->GetPrimitiveObject()) {
                    prim->GetMaterial().color = poppedFrame.color;
                    prim->SetScale(poppedFrame.scale);
                    prim->SetRotation(poppedFrame.rotation);
                    prim->SetTranslation(poppedFrame.position);
                    prim->Update();
                }
                if (gameCamera_) {
                    gameCamera_->SetFollowTarget(nullptr);
                    gameCamera_->SetTranslation(poppedFrame.cameraPosition);
                }

                // マップチップ（コインなど）の巻き戻し
                std::string initMapStr = ReplayManager::GetInstance()->GetCurrentMapDataStr();
                if (!initMapStr.empty()) {
                    // 再構築を一時停止
                    map_->SetRebuildEnabled(false);

                    // 初期状態のマップに戻す
                    map_->LoadFromString(initMapStr);
                    player_->SetScore(0);
                    
                    // 録画されているフレームを最初からたどってコインの取得状態を再構築する
                    const auto& frames = ReplayManager::GetInstance()->GetTemporaryRecordedFrames();
                    for (const auto& frame : frames) {
                        player_->SetPosition(frame.position);
                    }
                    
                    // 今ポップしたフレームの座標でも判定しておく
                    player_->SetPosition(poppedFrame.position);
                    
                    // 再構築を再開（ここで一括構築される）
                    map_->SetRebuildEnabled(true);

                    // スコアを同期
                    previousScore_ = player_->GetScore();
                }
            }
        } else {
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
                    if (coinEffect_) coinEffect_->Clear();
                    
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
                    if (gameCamera_) {
                        if (ReplayManager::GetInstance()->IsSnapEnabled()) {
                            // 再生中は自動追従を一時的に無効化し、記録されたカメラ座標に同期させる
                            gameCamera_->SetFollowTarget(nullptr);
                        } else {
                            // 座標補正がOFF（TASモード）の場合はプレイヤーに追従させる
                            gameCamera_->SetFollowTarget(&player_->GetPosition());
                        }
                        gameCamera_->SetTranslation(ReplayManager::GetInstance()->GetCurrentReplay().cameraInitPos);
                    }
                    wasPlayingLastFrame_ = true;
                }
                Vector3 pos = player_->GetPosition();
                Vector3 camPos = gameCamera_ ? gameCamera_->GetTranslation() : Vector3{ 0.0f, 0.0f, 0.0f };
                ReplayManager::GetInstance()->UpdatePlayback(pos, camPos);
                player_->SetPosition(pos);
                if (gameCamera_) {
                    if (ReplayManager::GetInstance()->IsSnapEnabled()) {
                        gameCamera_->SetFollowTarget(nullptr);
                        gameCamera_->SetTranslation(camPos);
                    } else {
                        gameCamera_->SetFollowTarget(&player_->GetPosition());
                        // 追従モードのため、SetTranslation(camPos) は実行しない
                    }
                }
                }
            } else {
                if (wasPlayingLastFrame_) {
                    // リプレイが終了した（またはTAKEOVERで停止した）瞬間に、カメラの追従を復元する
                    if (gameCamera_) {
                        gameCamera_->SetFollowTarget(&player_->GetPosition());
                    }
                }
                wasPlayingLastFrame_ = false;

                // 巻き戻しから通常に戻ったときにカメラ追従を再開する
            }

            if (gameCamera_ && !ReplayManager::GetInstance()->IsPlaying() && !isRewinding) {
                if (player_->IsDead()) {
                    gameCamera_->SetFollowTarget(nullptr);
                } else {
                    gameCamera_->SetFollowTarget(&player_->GetPosition());
                }
            }

            if (gameCamera_ && map_ && gameState_ != GameState::Clear) {
                gameCamera_->SetRooms(map_->GetRooms());
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

            player_->UpdateWithMap(*map_, gameCamera_ && gameCamera_->IsTransitioning(), canControl);

            // ゴール判定
            if (gameState_ == GameState::Playing && player_->IsGoalComplete()) {
                gameState_ = GameState::Clear;
                stateTimer_ = 0.0f;
                if (gameCamera_) {
                    gameCamera_->SetRooms({}); // 部屋境界制限を解除してプレイヤー中心へ直接追従
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
        }

        if (!isRewinding && wasRewindingLastFrame_) {
            if (gameCamera_) {
                gameCamera_->SetFollowTarget(&player_->GetPosition());
            }
        }
        wasRewindingLastFrame_ = isRewinding;

        // プレイ中の場合、リプレイ録画を行う
        if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying()) {
            if (!isRewinding) {
                Vector3 camPos = gameCamera_ ? gameCamera_->GetTranslation() : Vector3{ 0.0f, 0.0f, 0.0f };
                if (!ReplayManager::GetInstance()->IsRecording()) {
                    std::string mapStr = map_ ? map_->GetMapDataAsString() : "";
                    ReplayManager::GetInstance()->StartRecord(player_->GetPosition(), camPos, mapStr);
                }
                Vector4 pColor = player_->GetPrimitiveObject() ? player_->GetPrimitiveObject()->GetMaterial().color : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                Vector3 pScale = player_->GetPrimitiveObject() ? player_->GetPrimitiveObject()->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
                Vector3 pRot = player_->GetPrimitiveObject() ? player_->GetPrimitiveObject()->GetRotation() : Vector3(0.0f, 0.0f, 0.0f);
                ReplayManager::GetInstance()->RecordFrame(player_->GetPosition(), camPos, pColor, pScale, pRot);
            }
        } else {
            if (ReplayManager::GetInstance()->IsRecording()) {
                ReplayManager::GetInstance()->StopRecord();
            }
        }
    }

}

void GameScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI

    if (player_ && player_->GetPrimitiveObject() == selectedPrimitive) {
        player_->DisplayImGui();
    }

    // エディター側でプレイ状態になっていないときは、インゲームUI（スコア等）を描画しない
    if (!EditorManager::IsPlaying()) {
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
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "[LB] / [LT] : Rewind");
        } else {
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "A/D or Left/Right : Move");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "SPACE : Jump / Wall Jump");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "J : Dash");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "K : Wall Cling (W/S to Climb)");
            ImGui::TextColored(ImVec4(1,1,1,0.8f), "Ctrl + Left : Rewind");
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
    if (modelCommon_) {
        OutputDebugStringA("[DEBUG] GameScene::Draw ModelCommon::PreDraw Start\n");
        modelCommon_->PreDraw();
        OutputDebugStringA("[DEBUG] GameScene::Draw ModelCommon::PreDraw End\n");
    }

    // マップの描画
    if (map_) {
        map_->Draw();
    }

    // プレイヤーの描画
    if (player_) {
        player_->Draw();
    }


    // コンポーネントの描画を実行
    Renderer::GetInstance()->RenderComponents();

    if (coinEffect_) {
#ifdef USE_IMGUI
        if (EditorManager::IsShowEffects() || ReplayManager::GetInstance()->IsPlaying()) {
            coinEffect_->Draw();
        }
#else
        coinEffect_->Draw();
#endif
    }

#ifdef USE_IMGUI
    // --- ゴースト残像の描画（マリオメーカー仕様） ---
    // エディタ停止中で、かつリプレイの再生/録画もしていない時に「選択中のリプレイ全体」の軌跡を表示する
    if (!EditorManager::IsPlaying() && player_) {
        ReplayManager* replayManager = ReplayManager::GetInstance();
        if (replayManager && !replayManager->IsPlaying() && !replayManager->IsRecording()) {
            ReplayData& currentReplay = replayManager->GetCurrentReplay();
            if (!currentReplay.frames.empty()) {
                const float GHOST_ALPHA = 0.5f;
                const int FRAME_STEP = 10;
                auto* playerPrim = player_->GetPrimitiveObject();
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
                auto* playerPrim = player_->GetPrimitiveObject();
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
    particleCommon_->PreDraw();

    // パーティクルの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects() || ReplayManager::GetInstance()->IsPlaying()) {
#endif
        particleCommon_->DrawAll(viewProjectionMatrix);
#ifdef USE_IMGUI
    }
#endif
}

void GameScene::Draw2D() {
    if (gameState_ == GameState::StartReady) {
        if (spriteCommon_) {
            spriteCommon_->PreDraw();
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
        if (spriteCommon_ && clearSprite_) {
            spriteCommon_->PreDraw();
            clearSprite_->Draw();
        }
    }
}

void GameScene::DrawEditorOverlay(const Matrix4x4 &viewProjectionMatrix) {
#ifdef USE_IMGUI
    if (!EditorManager::IsPlaying() && player_) {
        ReplayManager* replayManager = ReplayManager::GetInstance();
        if (replayManager && !replayManager->IsPlaying() && !replayManager->IsRecording()) {
            auto* playerPrim = player_->GetPrimitiveObject();
            ImVec2 gameViewPos = EditorManager::GetGameViewPos();
            ImVec2 gameViewSize = EditorManager::GetGameViewSize();
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 1. プレイヤー残像の2D軌跡描画
            if (playerPrim && playerPrim->GetShowTrail()) {
                ReplayData& currentReplay = replayManager->GetCurrentReplay();
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

    // 1. 背景エフェクト
    if (cylinderEffect_) {
        result.push_back(cylinderEffect_->GetRoot());
    }
    if (ringEffect_) {
        result.push_back(ringEffect_->GetRoot());
    }

    // 2. プレイヤー
    if (player_) {
        result.push_back(player_->GetPrimitiveObject());
    }

    // 3. マップチップ
    if (map_) {
        auto mapPrims = map_->GetPrimitiveObjects();
        result.insert(result.end(), mapPrims.begin(), mapPrims.end());
    }

    if (coinEffect_) {
        auto coinPrims = coinEffect_->GetParticles();
        result.insert(result.end(), coinPrims.begin(), coinPrims.end());
    }

    return result;
}

void GameScene::UpdateEditor() {
    float dt = TimeManager::GetInstance().GetDeltaTime();
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
        auto* playerPrim = player_->GetPrimitiveObject();
        if (playerPrim) {
            playerPrim->SetTranslation(player_->GetPosition());
            playerPrim->Update();
        }
    }

    if (skybox_) {
        skybox_->Update();
    }
}