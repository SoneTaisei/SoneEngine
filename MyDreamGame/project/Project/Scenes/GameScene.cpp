#include "GameScene.h"
#include <Windows.h>
#include "Scene/SceneManager.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Resource/Model/ModelManager.h"
#include "Graphics/GameCamera.h"
#include "Scene/SceneFactory.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "Editor/EditorManager.h"
#endif
#include "Editor/Replay/ReplayManager.h"
#include "Game2D/Blocks/FragileBlock.h"
#include "Game2D/Blocks/GuardBlock.h"
#include "BlockDesignPanel.h"
#include "Renderer/Renderer.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"
#include "Graphics/Skybox.h"
#include "Core/Utility/ParameterManager.h"
#include "Effect/TransitionDirector.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/Camera.h"
#include "Resource/Sprite/Sprite.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "Input/GamepadInput.h"

std::string GameScene::s_TargetMapFilePath = "resources/json/shared/Map/map_data.json";
bool GameScene::s_QuickRestart = false;

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
    (void)sceneManager;
    isPaused_ = false;
    isIrisInActive_ = false;
    if (gameCamera_) {
        gameCamera_->SetFollowTarget(nullptr);
        gameCamera_->SetOrthographic(false);
    }
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetCompositeIrisEnabled(false);
    }
}

GameScene::~GameScene() {
    // 覆い切って次シーンへ持ち越す途中以外は遷移演出を捨てる（このシーンの鎖・マップへの参照を切る）
    TransitionDirector::GetInstance()->OnSceneDestroyed(chainManager_.get());
    // リプレイのオブジェクト記録対象から外す（次シーンのマップと混ざらないようにする）
    ReplayManager::GetInstance()->UnregisterObjectProvider(map_.get());
    if (alert_) {
        ReplayManager::GetInstance()->UnregisterObjectProvider(alert_.get());
        alert_->SetAsCurrent(false);
    }
}

void GameScene::GoToNextStage(SceneManager* sceneManager) {
    // クリア後はいったんタイトルシーンのステージ選択フェーズへ戻る（持ち越し用の鎖・カメラ操作などの演出状態はここで捨てる）
    TransitionDirector::GetInstance()->Abort();
    Log("GameScene: stage clear -> TitleScene (StageSelect phase)\n");

#ifdef USE_IMGUI
    if (EditorManager::GetInstance()) {
        EditorManager::GetInstance()->SetCurrentSceneType(SceneType::kTitle);
        EditorManager::GetInstance()->SetUseDebugCamera(false);
    }
    EditorManager::SetPlaying(true);
#endif

    // タイトルシーンに「ステージ選択画面から直接開始する」フラグを渡す
    sceneManager->SetData("StartAtStageSelect", true);
    sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
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

    // 4.5. マップ背景板ポリゴンの生成（スポットライト等のライティング視認用）
    Primitive* planePrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Plane, 1.0f);
    if (planePrim) {
        backgroundPlane_ = std::make_unique<PrimitiveObject>();
        backgroundPlane_->Initialize(device.Get(), planePrim);
        backgroundPlane_->SetName("BackgroundPlane");
        
        // 法線を手前（Z負方向）に向けるためX軸を-90度回転
        backgroundPlane_->SetRotation({ -std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
        // マップ全体を覆うスケール（X: 横幅, Z: 高さ）
        backgroundPlane_->SetScale({ 300.0f, 1.0f, 150.0f });
        // ブロック（Z=0, 厚み1.0）の奥（Z=1.6f）に配置
        backgroundPlane_->SetTranslation({ 100.0f, 20.0f, 1.6f });
        
        auto& mat = backgroundPlane_->GetMaterial();
        mat.lightingType = 1; // ライティング有効化
        mat.enableEnvironmentMap = 0;
        mat.color = { 0.28f, 0.30f, 0.35f, 1.0f }; // スポットライトが映えやすい背景色
        mat.shininess = 20.0f;
        backgroundPlane_->Update();
        LoadBackgroundConfig();
        Log("GameScene::Initialize: BackgroundPlane Initialized\n");
    }

    // 5. マップの生成と初期化
    map_ = std::make_unique<MapChip2D>();
    map_->Initialize( s_TargetMapFilePath);
    // 動く床・扉などの状態をリプレイに記録・復元できるように登録する
    ReplayManager::GetInstance()->RegisterObjectProvider(map_.get());
    Log("GameScene::Initialize: Map Initialized\n");

    // 5.5. 警戒度（ステージ開始で 0。警備員・鎖・崩れる床は AlertSystem::Current() 経由で事象を足す）
    alert_ = std::make_unique<AlertSystem>();
    alert_->LoadParams();
    alert_->Reset();
    alert_->SetAsCurrent(true);
    ReplayManager::GetInstance()->RegisterObjectProvider(alert_.get());

    // 6. プレイヤーの生成と初期化
    playerObj_ = std::make_unique<GameObject>("Player");
    playerObj_->AddComponent<TransformComponent>();
    player_ = playerObj_->AddComponent<Player2D>();
    player_->SetCamera(gameCamera_); // 画面揺れ連携用にカメラを渡す
    Log("GameScene::Initialize: Player Initialized\n");
    
    player_->FindSpawnPoint(*map_);
    Log("GameScene::Initialize: Player SpawnPoint found\n");

    // 6.4. 前のステージから持ち越した鎖の個数を引き継ぐ（遷移用の鎖と同じ長さで生成され、着地の切り替えが見えない）
    {
        TransitionDirector* director = TransitionDirector::GetInstance();
        if (director->HasCarry() && director->GetParams().carryChainLength_) {
            player_->SetChainLength(director->GetCarryUnits());
        }
    }

    // 6.5. 鎖の生成（プレイヤー鎖 + 末端のお宝。吊り鎖はマップ配置で AddWorldChain）
    chainManager_ = std::make_unique<ChainManager>();
    chainManager_->Initialize(player_);
    Log("GameScene::Initialize: ChainManager Initialized\n");

    // 7. GameCameraを正射影モード（2D表示）に切り替え
    if (gameCamera_) {
        Log("GameScene::Initialize: Camera config...\n");
        float orthoWidth = ParameterManager::GetInstance()->GetValue("GameScene", "orthoWidth", 20.0f);
        float orthoHeight = ParameterManager::GetInstance()->GetValue("GameScene", "orthoHeight", 11.25f);
        gameCamera_->InitializeOrthographic(1280, 720, orthoWidth, orthoHeight);
        // プレイヤーの位置をカメラ追従ターゲットに設定
        gameCamera_->SetFollowTarget(&player_->GetPosition());
        Log("GameScene::Initialize: Camera configured\n");
    }


    // 7.5. ステージクリア遷移の続き（前のステージを黒で覆って来た場合、黒から円が開き、持ち越した宝石と鎖が上から降りてくる）
    {
        TransitionDirector* director = TransitionDirector::GetInstance();
        if (director->IsCovered() && player_) {
            float w = gameCamera_ ? gameCamera_->GetOrthoWidth() : 20.0f;
            float h = gameCamera_ ? gameCamera_->GetOrthoHeight() : 11.25f;
            director->StartStageOpen(chainManager_.get(), player_->GetPosition(), w, h);
            transitionAlpha_ = 0.0f; // 既存のフェードインは円が開く演出に置き換える
        }
    }

    // 7.6. ゲーム開始時のアイリスイン演出（プレイヤー座標を中心に開く）
    {
        TransitionDirector* director = TransitionDirector::GetInstance();
        if ((!director || !director->IsPlaying()) && player_) {
            StartIrisIn(player_->GetPosition(), 1.2f);
        }
    }

    // -------------------------------------------------------------
    // 8. ポーズメニュー スプライトの初期化 (poseText, restartText, titleText)
    // -------------------------------------------------------------
    if (spriteCommon_) {
        // 暗幕（半透明ブラック）
        pauseBackdropTexHandle_ = TextureManager::GetInstance()->Load("resources/Object/Original/kusari/kusari_2/white.png");
        pauseBackdropSprite_ = std::make_unique<Sprite>();
        pauseBackdropSprite_->Initialize(spriteCommon_, pauseBackdropTexHandle_);
        pauseBackdropSprite_->SetPosition({ 0.0f, 0.0f });
        pauseBackdropSprite_->SetSize({ 1280.0f, 720.0f });
        pauseBackdropSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.65f });

        // 「ポーズ」タイトル (poseText.png: 300x100)
        pauseTitleTexHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/poseText.png");
        pauseTitleSprite_ = std::make_unique<Sprite>();
        pauseTitleSprite_->Initialize(spriteCommon_, pauseTitleTexHandle_);
        const float pTitleW = 240.0f;
        const float pTitleH = 80.0f;
        pauseTitleSprite_->SetSize({ pTitleW, pTitleH });
        pauseTitleSprite_->SetPosition({ (1280.0f - pTitleW) * 0.5f, 150.0f });
        pauseTitleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        // 「リトライ」項目 (restartText.png: 500x100)
        pauseRestartTexHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/restartText.png");
        pauseRestartSprite_ = std::make_unique<Sprite>();
        pauseRestartSprite_->Initialize(spriteCommon_, pauseRestartTexHandle_);
        const float rW = 280.0f;
        const float rH = 56.0f;
        pauseRestartSprite_->SetSize({ rW, rH });
        pauseRestartSprite_->SetPosition({ (1280.0f - rW) * 0.5f, 320.0f });

        // 「タイトル」項目 (titleText.png: 500x100)
        pauseTitleTextTexHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/titleText.png");
        pauseTitleTextSprite_ = std::make_unique<Sprite>();
        pauseTitleTextSprite_->Initialize(spriteCommon_, pauseTitleTextTexHandle_);
        const float tW = 280.0f;
        const float tH = 56.0f;
        pauseTitleTextSprite_->SetSize({ tW, tH });
        pauseTitleTextSprite_->SetPosition({ (1280.0f - tW) * 0.5f, 430.0f });
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

    if (skybox_) {
        skybox_->Update();
    }

    if (backgroundPlane_) {
        backgroundPlane_->Update();
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
        if (s_QuickRestart) {
            // ミス直後のやり直しは待たせない（テンポ優先）
            startReadyTime = ParameterManager::GetInstance()->GetValue("GameScene", "quickRestartReadyTime", 0.3f);
        }
        if (stateTimer_ > startReadyTime) {
            gameState_ = GameState::Playing;
            stateTimer_ = 0.0f;
            s_QuickRestart = false;
        }
    } else if (gameState_ == GameState::Captured) {
        // 捕獲演出（赤フラッシュ → 「捕獲」 → 暗転）の後、ステージ選択へ
        stateTimer_ += dt;
        // 捕獲（見つかった回数）は演出を見せる。普通のミスは即やり直し（短い赤フラッシュだけ）
        float captureTime = capturedByMiss_
            ? (ParameterManager::GetInstance()->GetValue("Alert", "missPlayTime_", 0.6f) +
               ParameterManager::GetInstance()->GetValue("Alert", "missRestartTime_", 0.25f))
            : ParameterManager::GetInstance()->GetValue("Alert", "captureSceneTime_", 2.8f);
        if (stateTimer_ >= captureTime) {
            // 失敗：同じステージを最初からやり直す（ステージ選択でこのステージを選んだ時と同じ）
            TransitionDirector::GetInstance()->Abort();
            s_QuickRestart = capturedByMiss_;
            Log("GameScene: captured -> restart same stage\n");
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
            return;
        }
    } else if (gameState_ == GameState::Clear) {
        stateTimer_ += dt;
        // 遷移演出が動いていない時（中断された場合など）だけ SPACE でタイトル（ステージ選択画面）へ戻れる
        if (!TransitionDirector::GetInstance()->IsPlaying() && KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
#ifdef USE_IMGUI
            if (EditorManager::GetInstance()) {
                EditorManager::GetInstance()->SetCurrentSceneType(SceneType::kTitle);
                EditorManager::GetInstance()->SetUseDebugCamera(false);
            }
            EditorManager::SetPlaying(true);
#endif
            sceneManager->SetData("StartAtStageSelect", true);
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
            return;
        }
    }

    // -------------------------------------------------------------
    // ポーズ入力の監視 (ESC, P, ゲームパッド Startボタン)
    // -------------------------------------------------------------
    if (pauseCooldown_ > 0.0f) {
        pauseCooldown_ -= dt;
    }

    bool togglePause = false;
    // プレイ中、またはポーズ中の場合にトグル可能（クリア中・開始直後は無効）
    if (gameState_ == GameState::Playing || isPaused_) {
        auto kb = KeyboardInput::GetInstance();
        auto pad = GamepadInput::GetInstance();

        if (kb->IsKeyPressed(DIK_TAB) || kb->IsKeyPressed(DIK_ESCAPE)) {
            togglePause = true;
        }
        if (pad && (pad->IsButtonPressed(7) || pad->IsButtonPressed(9))) { // Start / Option
            togglePause = true;
        }

        if (togglePause && pauseCooldown_ <= 0.0f) {
            isPaused_ = !isPaused_;
            pauseCooldown_ = 0.25f;
            pausePulseTimer_ = 0.0f;
            if (isPaused_) {
                pauseMenuIndex_ = 0; // 開いた時は「リトライ」に初期選択
            }
        }
    }

    // ポーズ中ならポーズメニューのみ更新し、ゲーム進行をすべて停止
    if (isPaused_) {
        UpdatePauseMenu(dt, sceneManager);
        return;
    }

    // 4. プレイヤーの更新（入力・物理・当たり判定）
    if (player_ && map_) {
        bool isCurrentlyPlaying = true;
#ifdef USE_IMGUI
        isCurrentlyPlaying = EditorManager::IsPlaying();
#endif

        if (isCurrentlyPlaying && !wasCurrentlyPlaying_) {
            player_->FindSpawnPoint(*map_);
            // プレイ開始時は鎖と個数を初期状態に戻す（毎回同じ初期状態から始めてリプレイ再現性を保つ）
            if (chainManager_) {
                chainManager_->ResetAll();
            }
            // 警戒度もステージ開始として 0 に（死亡のリスポーンでは戻さない）
            if (alert_) {
                alert_->Reset();
            }
            if (gameState_ == GameState::Captured) {
                gameState_ = GameState::StartReady;
                stateTimer_ = 0.0f;
            }
        }
        if (isCurrentlyPlaying && !wasCurrentlyPlaying_) {
            // プレイ開始時はゲーム内クロックを0に戻す（動く床の位相を毎回同じにするため）
            ReplayManager::GetInstance()->ResetPlayClock();
            // プレイ開始時のアイリスイン演出（プレイヤー座標を中心に開く）
            StartIrisIn(player_->GetPosition(), 1.2f);
        }
        wasCurrentlyPlaying_ = isCurrentlyPlaying;

        // 動く床などが参照する共有クロックを進める。
        // 再生中は記録された時刻になるため、シークやループでも録画時と同じ位置になる。
        // UpdatePlayback より前に呼ぶこと（UpdatePlayback がフレーム番号を進めてしまうため）。
        int replayFrameForThisTick = ReplayManager::GetInstance()->GetCurrentFrame();
        if (isCurrentlyPlaying || ReplayManager::GetInstance()->IsPlaying()) {
            ReplayManager::GetInstance()->UpdatePlayClock(dt);
        }

        bool isRewinding = false;
        if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying()) {
            auto keyboard = KeyboardInput::GetInstance();
            if ((keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL)) &&
                keyboard->IsKeyDown(DIK_LEFT)) {
                isRewinding = true;
            }
        }

        if (isRewinding) {
            // 巻き戻し中は鎖が更新されないので、スピンは中断しておく（明けの幻の発射・チャージのずれ防止）
            if (chainManager_) {
                chainManager_->OnRewindBegin();
            }
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
                    
                    // 録画されているフレームを最初からたどって再構築する
                    const auto& frames = ReplayManager::GetInstance()->GetTemporaryRecordedFrames();
                    for (const auto& frame : frames) {
                        player_->SetPosition(frame.position);
                    }
                    
                    // 今ポップしたフレームの座標でも判定しておく
                    player_->SetPosition(poppedFrame.position);
                    
                    // 再構築を再開（ここで一括構築される）
                    map_->SetRebuildEnabled(true);
                }

                // 再構築でブロックが作り直されるので、その後に動く床などの状態を巻き戻す
                ReplayManager::GetInstance()->RestoreRecordedObjectsAtCurrent();
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
                    
                    // 2. プレイヤー状態(速度含む)をリセット
                    player_->ResetState(replayData.playerInitPos);
                    player_->ClearEffects();

                    // 鎖も初期状態から再現する（鎖はプレイヤー位置と入力の決定論的な関数なので再シミュレーションで一致する）
                    if (chainManager_) {
                        chainManager_->ResetAll();
                    }
                    
                    // 3. 0フレーム目から現在フレームまで座標を再現
                    for (int i = 0; i <= curFrame; ++i) {
                        player_->SetPosition(replayData.frames[i].position);
                    }
                    map_->SetRebuildEnabled(true);

                    // 4. シミュレーション再開用の正しい座標に戻す
                    if (curFrame == 0) {
                        player_->SetPosition(replayData.playerInitPos);
                    } else {
                        player_->SetPosition(replayData.frames[curFrame - 1].position);
                    }
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

            if (gameCamera_ && !ReplayManager::GetInstance()->IsPlaying() && !isRewinding &&
                !TransitionDirector::GetInstance()->IsCameraControlled()) { // クリア演出中はカメラを演出側が動かす
                if (player_->IsDead()) {
                    gameCamera_->SetFollowTarget(nullptr);
                } else {
                    gameCamera_->SetFollowTarget(&player_->GetPosition());
                }
            }

            if (gameCamera_ && map_) {
                gameCamera_->SetRooms(map_->GetRooms());
            }

            // リプレイ再生中は、動く床・扉・スイッチ等の状態を記録時のものへ戻す。
            // マップ再構築（LoadFromString）の後・マップ更新の前に行う必要がある。
            if (ReplayManager::GetInstance()->IsPlaying()) {
                ReplayManager::GetInstance()->RestoreObjectsAtFrame(replayFrameForThisTick);
            }

            // 警戒度は Playing 中だけ動く（StartReady / Clear / 捕獲後は止める）。再生中は記録から復元されるので進めない
            if (alert_) {
                bool alertActive = (gameState_ == GameState::Playing) && !player_->IsGoal();
                alert_->SetActive(alertActive);
                if (!ReplayManager::GetInstance()->IsPlaying()) {
                    alert_->Update(dt);
                }
                // 捕獲は死亡より優先（同フレームならステージ失敗の方が重い）
                if (alertActive && alert_->IsCaptured()) {
                    gameState_ = GameState::Captured;
                    capturedByMiss_ = false;
                    stateTimer_ = 0.0f;
                    alert_->SetActive(false);
                    if (ReplayManager::GetInstance()->IsRecording()) {
                        ReplayManager::GetInstance()->StopRecord();
                    }
                    Log("GameScene: captured (alert full)\n");
                } else if (gameState_ == GameState::Playing && player_->IsDead()) {
                    // 普通のミス（接触・落下・危険ブロック）も部屋リスポーンではなく、捕獲と同じく最初からやり直し
                    gameState_ = GameState::Captured;
                    capturedByMiss_ = true;
                    stateTimer_ = 0.0f;
                    alert_->SetActive(false);
                    if (ReplayManager::GetInstance()->IsRecording()) {
                        ReplayManager::GetInstance()->StopRecord();
                    }
                    Log("GameScene: miss -> restart same stage\n");
                }
            }
            bool worldFrozen = (gameState_ == GameState::Captured);
            bool playerFrozen = worldFrozen;
            if (gameState_ == GameState::Captured && capturedByMiss_) {
                // 普通のミス：すぐ止めずに少しの間そのまま動かす（鎖と宝石が落ち、警備員が動き、カメラが追う）。
                // プレイヤー自身は死亡アニメの間だけ動かし、リスポーンで飛ぶ前にその場で止める
                float playTime = ParameterManager::GetInstance()->GetValue("Alert", "missPlayTime_", 0.6f);
                if (stateTimer_ < playTime) {
                    worldFrozen = false;
                    playerFrozen = (stateTimer_ >= player_->GetParams().deathDuration_ - 0.02f);
                }
            }

            // マップの更新をプレイヤーより先に行う（移動リフト等の新しい座標に対して判定するため）
            if (map_ && !worldFrozen) {
                map_->Update();
            }

            if (!playerFrozen) {
                player_->UpdateWithMap(*map_, gameCamera_ && gameCamera_->IsTransitioning());
            }

            // 復活直後の猶予：時間経過と加算を止め、警備員の見られゲージを 0 に戻す（復活位置で見られて即 +25 を防ぐ）
            if (alert_) {
                alert_->SetPlayerPosition(player_->GetPosition());
                bool dead = player_->IsDead();
                if (playerWasDead_ && !dead && map_) {
                    alert_->StartGrace(alert_->GetParams().respawnGrace_);
                    for (const auto& block : map_->GetUpdateBlocks()) {
                        if (auto* guard = dynamic_cast<GuardBlock*>(block.get())) guard->ResetAlertGauge();
                    }
                }
                playerWasDead_ = dead;
            }

            // プレイヤーと危険な光（スポットライト）の当たり判定
            if (gameState_ == GameState::Playing && !player_->IsDead() && !player_->IsGoal()) {
                bool hitDangerousLight = false;
#ifdef USE_IMGUI
                if (auto* editorMgr = EditorManager::GetInstance()) {
                    if (auto* lightEditor = editorMgr->GetLightEditor()) {
                        hitDangerousLight = lightEditor->CheckAABBHit(player_->GetAABB()) ||
                                            lightEditor->CheckPlayerHit(player_->GetPosition(), player_->GetParams().halfWidth_);
                    }
                }
#endif
                if (hitDangerousLight) {
                    player_->Kill();
                }
            }


            // 鎖の更新（K入力 → 個数照合 → 物理。鎖がプレイヤーに反応するため、プレイヤー位置確定後に行う）
            if (chainManager_ && !worldFrozen) {
                chainManager_->HandleInput();
                chainManager_->Reconcile();
                FragileBlock::SetCurrentChainWeight(player_->GetChainLength()); // 崩れる床の赤い予告用
                chainManager_->Update(dt, map_.get());
            }

            // 警備員の懐中電灯スポットライトを同期
            UpdateGuardLights();

            // ゴール判定 → ステージクリア遷移の開始（宝石と鎖を遷移側へ渡し、黒い円で絞る）
            if (gameState_ == GameState::Playing && player_->IsGoalComplete()) {
                gameState_ = GameState::Clear;
                stateTimer_ = 0.0f;
                if (ReplayManager::GetInstance()->IsRecording()) {
                    ReplayManager::GetInstance()->StopRecord(); // 記録はゴール時点で止める
                }
                if (chainManager_) {
                    // プレイヤーのモデルとカメラは演出（寄る・喜ぶ・奥へ・飛び抜け）に使う
                    TransitionDirector::GetInstance()->StartStageClear(chainManager_.get(), map_.get(), player_, gameCamera_);
                }
            }

            // ステージクリア遷移（絞る → 宝石と鎖が降りる → 覆い切ったら次のステージへ。次シーン側では円が開いて降りてくる）
            {
                TransitionDirector* director = TransitionDirector::GetInstance();
                director->Update(dt);
                if (director->ConsumeCoveredEvent()) {
                    GoToNextStage(sceneManager);
                    return;
                }
            }

        }

        if (!isRewinding && wasRewindingLastFrame_) {
            if (gameCamera_) {
                gameCamera_->SetFollowTarget(&player_->GetPosition());
            }
            // 巻き戻し明けは鎖の暴れ防止のため暗黙速度をリセット（落とした鎖は再現できないため消去）
            if (chainManager_) {
                chainManager_->OnRewindEnd();
            }
        }
        wasRewindingLastFrame_ = isRewinding;

        // プレイ中の場合、リプレイ録画を行う
        if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying() && gameState_ != GameState::Clear) {
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

    // プレイヤー座標を基準にしたアイリスイン演出の更新
    if (player_) {
        UpdateIrisIn(player_->GetPosition(), dt);
    }
}

#ifdef USE_IMGUI
namespace {
    // ===== 崩れる床の調整パネル（インスペクター内の折りたたみ） =====
    // ・ゲームビューで床にマウスを乗せると水色に点滅、クリックでその橋を選択
    // ・隣り合う崩れる床は「橋」として1行にまとめ、1つの値で全部に適用（1枚ずつも開ける）
    // ・「次に置く床の上限」を決めておくと、塗った瞬間にその上限になる
    // ・「試し本数」で、何本持っていればどの床が崩れるかを色で確かめられる
    constexpr int kFragileMaxLimit = 8;

    struct FragileGroup {
        std::vector<FragileBlock*> members; // 下の段から、左から
        int minX = 0, maxX = 0, minY = 0, maxY = 0;
        int key = 0;          // 先頭メンバーのキー（選択の識別用）
        int minLimit = 0;
        int maxLimit = 0;
    };

    int FragileKey(const FragileBlock* f) { return f->GetChipX() * 100000 + f->GetChipY(); }

    bool FragileLess(const FragileBlock* a, const FragileBlock* b) {
        if (a->GetChipY() != b->GetChipY()) return a->GetChipY() < b->GetChipY();
        return a->GetChipX() < b->GetChipX();
    }

    // 上下左右でつながっている崩れる床を「橋」としてまとめる
    std::vector<FragileGroup> BuildFragileGroups(MapChip2D* map) {
        std::vector<FragileBlock*> floors;
        std::map<std::pair<int, int>, FragileBlock*> byChip;
        for (const auto& block : map->GetUpdateBlocks()) {
            if (auto* f = dynamic_cast<FragileBlock*>(block.get())) {
                floors.push_back(f); // 崩れて消えた床も含める（消えた後も調整・復活できるように）
                byChip[{f->GetChipX(), f->GetChipY()}] = f;
            }
        }
        std::sort(floors.begin(), floors.end(), FragileLess);

        std::vector<FragileGroup> groups;
        std::set<FragileBlock*> visited;
        for (auto* start : floors) {
            if (visited.count(start)) continue;
            FragileGroup g;
            std::vector<FragileBlock*> stack;
            stack.push_back(start);
            visited.insert(start);
            while (!stack.empty()) {
                FragileBlock* f = stack.back();
                stack.pop_back();
                g.members.push_back(f);
                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    auto it = byChip.find({f->GetChipX() + dx[i], f->GetChipY() + dy[i]});
                    if (it != byChip.end() && !visited.count(it->second)) {
                        visited.insert(it->second);
                        stack.push_back(it->second);
                    }
                }
            }
            std::sort(g.members.begin(), g.members.end(), FragileLess);
            g.minX = g.maxX = g.members.front()->GetChipX();
            g.minY = g.maxY = g.members.front()->GetChipY();
            g.minLimit = g.maxLimit = g.members.front()->GetPassLimit();
            for (auto* f : g.members) {
                g.minX = (std::min)(g.minX, f->GetChipX());
                g.maxX = (std::max)(g.maxX, f->GetChipX());
                g.minY = (std::min)(g.minY, f->GetChipY());
                g.maxY = (std::max)(g.maxY, f->GetChipY());
                g.minLimit = (std::min)(g.minLimit, f->GetPassLimit());
                g.maxLimit = (std::max)(g.maxLimit, f->GetPassLimit());
            }
            g.key = FragileKey(g.members.front());
            groups.push_back(std::move(g));
        }
        return groups;
    }

    void ApplyFragileLimit(MapChip2D* map, FragileBlock* f, int limit) {
        limit = std::clamp(limit, 0, kFragileMaxLimit);
        f->SetBreakWeight(limit + 1);
        map->SetBlockOverride(f->GetChipX(), f->GetChipY(), {{"breakWeight", limit + 1}});
    }

    void ResetFragileLimit(MapChip2D* map, FragileBlock* f) {
        map->ClearBlockOverride(f->GetChipX(), f->GetChipY());
        nlohmann::json def = map->GetPaletteProperties(f->GetChipX(), f->GetChipY());
        int bw = 4;
        if (def.contains("breakWeight") && def["breakWeight"].is_number()) bw = def["breakWeight"].get<int>();
        f->SetBreakWeight(bw);
    }

    // 「−  N 本  ＋」の入力。変わったら true
    bool FragileLimitInput(const char* id, int& limit, bool mixed) {
        ImGui::PushID(id);
        bool changed = false;
        if (ImGui::Button("-")) { limit -= 1; changed = true; }
        ImGui::SameLine(0.0f, 4.0f);
        if (mixed) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "混在");
        } else {
            ImGui::Text("%d 本", limit);
        }
        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::Button("+")) { limit += 1; changed = true; }
        limit = std::clamp(limit, 0, kFragileMaxLimit);
        ImGui::PopID();
        return changed;
    }

    void DrawFragileFloorImGui(MapChip2D* map, Camera* camera, const std::string& stagePath) {
        static bool highlightAll = false;
        static int selectedGroupKey = -1;   // 橋ごとの選択
        static int selectedBlockKey = -1;   // 1枚だけの選択
        static bool scrollToSelected = false;
        static bool previewEnabled = false;
        static int previewCount = 3;
        static bool placementEnabled = false;
        static int placementLimit = 3;

        // --- 見つける ---
        if (ImGui::Checkbox("崩れる床を全部点滅させる", &highlightAll)) {
            FragileBlock::SetHighlightAll(highlightAll);
        }
        static bool debugNoBreak = FragileBlock::IsDebugNoBreak();
        if (ImGui::Checkbox("デバッグ中は崩れても消えない（震えた後に元に戻る）", &debugNoBreak)) {
            FragileBlock::SetDebugNoBreak(debugNoBreak);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("消えた床を全部復活")) {
            for (const auto& block : map->GetUpdateBlocks()) {
                if (auto* f = dynamic_cast<FragileBlock*>(block.get())) {
                    if (f->IsDestroyed()) f->Reset();
                }
            }
        }
        ImGui::TextDisabled("床の点の数 = 通れる鎖の上限本数（この本数までは乗れる、超えると震えて落ちる）");

        // --- 試し本数 ---
        if (ImGui::Checkbox("試し本数で色分け", &previewEnabled)) {
            FragileBlock::SetPreviewChainWeight(previewEnabled ? previewCount : -1);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderInt("##preview", &previewCount, 0, kFragileMaxLimit, "%d 本持っていたら")) {
            if (previewEnabled) FragileBlock::SetPreviewChainWeight(previewCount);
        }
        if (previewEnabled) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "赤い床 = この本数では崩れる");
        }

        // --- 次に置く床の上限 ---
        if (ImGui::Checkbox("次に置く崩れる床の上限を決める", &placementEnabled)) {
            if (placementEnabled) {
                map->SetPlacementOverride("FragileBlock", {{"breakWeight", placementLimit + 1}});
            } else {
                map->ClearPlacementOverride("FragileBlock");
            }
        }
        ImGui::SameLine();
        {
            int v = placementLimit;
            if (FragileLimitInput("placement", v, false)) {
                placementLimit = v;
                if (placementEnabled) map->SetPlacementOverride("FragileBlock", {{"breakWeight", placementLimit + 1}});
            }
        }
        if (placementEnabled) {
            ImGui::TextDisabled("有効な間、Fragile Floor を塗るとこの上限で置かれる（塗った後に一覧で直すこともできる）");
        }

        // --- ゲームビューでの選択 ---
        auto groups = BuildFragileGroups(map);
        FragileBlock* hovered = nullptr;
        int mx = 0, my = 0;
        bool debugCam = false;
        if (BlockDesignPanel::MouseToChip(map, camera, mx, my)) {
            hovered = dynamic_cast<FragileBlock*>(map->GetBlock(mx, my));
            if (hovered) {
                hovered->SetHovered(true);
                bool selectNow = BlockDesignPanel::CanClickSelect() ? ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                                                   : (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
                if (selectNow) {
                    int key = FragileKey(hovered);
                    for (const auto& g : groups) {
                        for (auto* f : g.members) {
                            if (f == hovered) {
                                selectedGroupKey = (selectedGroupKey == g.key && selectedBlockKey == -1) ? -1 : g.key;
                                selectedBlockKey = -1;
                                scrollToSelected = true;
                                (void)key;
                            }
                        }
                    }
                }
            }
        }
        if (hovered) {
            ImGui::Text("ゲームビュー: 崩れる床 (%d, %d) にマウス。クリックでその橋を選ぶ", hovered->GetChipX(), hovered->GetChipY());
        } else {
            ImGui::TextDisabled("ゲームビューで床にマウスを乗せると水色に点滅、クリックでその橋を選ぶ");
        }

        ImGui::Separator();
        if (groups.empty()) {
            ImGui::Text("このステージに崩れる床はありません（パレットの Fragile Floor を置いてください）");
            return;
        }

        // --- 橋の一覧 ---
        int index = 0;
        for (auto& g : groups) {
            ++index;
            ImGui::PushID(g.key);
            bool groupSelected = (selectedGroupKey == g.key && selectedBlockKey == -1);
            for (auto* f : g.members) {
                bool sel = groupSelected || (selectedBlockKey == FragileKey(f));
                f->SetSelected(sel);
            }

            char label[96];
            if (g.members.size() == 1) {
                snprintf(label, sizeof(label), "床 %d  (%d, %d)", index, g.minX, g.minY);
            } else if (g.minY == g.maxY) {
                snprintf(label, sizeof(label), "橋 %d  (%d〜%d, %d)  %d枚", index, g.minX, g.maxX, g.minY, static_cast<int>(g.members.size()));
            } else {
                snprintf(label, sizeof(label), "橋 %d  (%d〜%d, %d〜%d)  %d枚", index, g.minX, g.maxX, g.minY, g.maxY, static_cast<int>(g.members.size()));
            }
            if (ImGui::Selectable(label, groupSelected, 0, ImVec2(210.0f, 0.0f))) {
                selectedGroupKey = groupSelected ? -1 : g.key;
                selectedBlockKey = -1;
            }
            if (groupSelected && scrollToSelected) {
                ImGui::SetScrollHereY();
                scrollToSelected = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("押すとこの橋が白く点滅する");
            {
                int destroyedCount = 0;
                for (auto* f : g.members) if (f->IsDestroyed()) ++destroyedCount;
                if (destroyedCount > 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%d 枚消えている", destroyedCount);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("復活")) { for (auto* f : g.members) if (f->IsDestroyed()) f->Reset(); }
                }
            }
            ImGui::SameLine();
            ImGui::Text("通れる上限");
            ImGui::SameLine();
            {
                bool mixed = (g.minLimit != g.maxLimit);
                int v = g.minLimit;
                if (FragileLimitInput("group", v, mixed)) {
                    for (auto* f : g.members) ApplyFragileLimit(map, f, v);
                    BlockDesignPanel::MarkUnsaved();
                }
            }
            ImGui::SameLine();
            bool anyOverride = false;
            for (auto* f : g.members) {
                if (map->GetBlockOverride(f->GetChipX(), f->GetChipY())) { anyOverride = true; break; }
            }
            if (!anyOverride) ImGui::BeginDisabled();
            if (ImGui::SmallButton("パレットの値に戻す")) {
                for (auto* f : g.members) ResetFragileLimit(map, f);
                BlockDesignPanel::MarkUnsaved();
            }
            if (!anyOverride) ImGui::EndDisabled();

            if (g.members.size() > 1) {
                ImGui::Indent(16.0f);
                if (ImGui::TreeNode("1枚ずつ")) {
                    for (auto* f : g.members) {
                        int key = FragileKey(f);
                        ImGui::PushID(key);
                        bool sel = (selectedBlockKey == key);
                        char l2[48];
                        snprintf(l2, sizeof(l2), "(%d, %d)", f->GetChipX(), f->GetChipY());
                        if (ImGui::Selectable(l2, sel, 0, ImVec2(80.0f, 0.0f))) {
                            selectedBlockKey = sel ? -1 : key;
                            selectedGroupKey = -1;
                        }
                        ImGui::SameLine();
                        int v = f->GetPassLimit();
                        if (FragileLimitInput("one", v, false)) {
                            ApplyFragileLimit(map, f, v);
                            BlockDesignPanel::MarkUnsaved();
                        }
                        ImGui::SameLine();
                        bool ov = (map->GetBlockOverride(f->GetChipX(), f->GetChipY()) != nullptr);
                        if (!ov) ImGui::BeginDisabled();
                        if (ImGui::SmallButton("戻す")) {
                            ResetFragileLimit(map, f);
                            BlockDesignPanel::MarkUnsaved();
                        }
                        if (!ov) ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::Unindent(16.0f);
            }
            ImGui::PopID();
        }

        ImGui::Separator();
        BlockDesignPanel::DrawSaveRow(map, stagePath, "fragile");
    }
}
#endif

void GameScene::DrawAlertHud(const ImVec2& viewPos, float viewWidth, float viewHeight) {
#ifdef USE_IMGUI
    (void)viewHeight;
    if (!alert_) return;
    // StartReady の間も出す（0 のバーが見えていた方が「これが上がる」と分かる）
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float ratio = std::clamp(alert_->GetRatio(), 0.0f, 1.0f);
    float pulse = alert_->GetPulse();

    const float barW = 240.0f;
    const float barH = 14.0f + 6.0f * pulse;
    const float margin = 16.0f;
    ImVec2 p0(viewPos.x + viewWidth - margin - barW, viewPos.y + margin);
    ImVec2 p1(p0.x + barW, p0.y + barH);

    // 色：緑 → 黄 → 赤（連続）
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    float r, g, b;
    if (ratio < 0.5f) {
        float t = ratio / 0.5f;
        r = lerp(0.25f, 0.95f, t); g = lerp(0.85f, 0.85f, t); b = lerp(0.35f, 0.25f, t);
    } else {
        float t = (ratio - 0.5f) / 0.5f;
        r = lerp(0.95f, 0.92f, t); g = lerp(0.85f, 0.20f, t); b = lerp(0.25f, 0.15f, t);
    }
    ImU32 fill = IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255), 235);

    // 下地・バー・枠
    dl->AddRectFilled(ImVec2(p0.x - 2.0f, p0.y - 2.0f), ImVec2(p1.x + 2.0f, p1.y + 2.0f), IM_COL32(0, 0, 0, 150), 4.0f);
    dl->AddRectFilled(p0, ImVec2(p0.x + barW * ratio, p1.y), fill, 3.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 200), 3.0f, 0, 1.5f);
    // 見られている：枠が黄色く点滅して「見られている」（+25 が来る前の猶予を猶予として見せる）
    if (alert_->IsBeingSeen()) {
        float blink = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 16.0));
        dl->AddRect(ImVec2(p0.x - 3.0f, p0.y - 3.0f), ImVec2(p1.x + 3.0f, p1.y + 3.0f), IM_COL32(255, 220, 60, static_cast<int>(230 * blink)), 5.0f, 0, 2.5f);
        const char* seenText = "見られている";
        ImVec2 st = ImGui::CalcTextSize(seenText);
        dl->AddText(ImVec2(p1.x - st.x, p0.y - st.y - 4.0f), IM_COL32(255, 230, 80, 255), seenText);
    } else if (ratio > 0.8f) {
        // 満タン付近は枠が赤く点滅
        float blink = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 10.0));
        dl->AddRect(ImVec2(p0.x - 3.0f, p0.y - 3.0f), ImVec2(p1.x + 3.0f, p1.y + 3.0f), IM_COL32(255, 60, 60, static_cast<int>(200 * blink)), 5.0f, 0, 2.0f);
    }
    // 復活直後の猶予：バーを青く覆う
    if (alert_->IsInGrace()) {
        dl->AddRectFilled(p0, p1, IM_COL32(120, 180, 255, 90), 3.0f);
        const char* graceText = "猶予";
        ImVec2 gt = ImGui::CalcTextSize(graceText);
        dl->AddText(ImVec2(p0.x + (barW - gt.x) * 0.5f, p0.y + (barH - gt.y) * 0.5f), IM_COL32(220, 240, 255, 255), graceText);
    }
    // 見出し（数字は出さない）
    ImVec2 ts = ImGui::CalcTextSize("ALERT");
    dl->AddText(ImVec2(p0.x - ts.x - 8.0f, p0.y + (barH - ts.y) * 0.5f), IM_COL32(255, 255, 255, 220), "ALERT");

    // 加点のポップアップ：バーの下から上へ浮かんで消える
    float y = p1.y + 6.0f;
    for (const auto& e : alert_->GetEvents()) {
        float t = std::clamp(e.age / 1.6f, 0.0f, 1.0f);
        float alpha = (t < 0.7f) ? 1.0f : (1.0f - (t - 0.7f) / 0.3f);
        float rise = 14.0f * t;
        ImVec2 tsz = ImGui::CalcTextSize(e.text.c_str());
        ImVec2 tp(p1.x - tsz.x, y - rise);
        dl->AddText(ImVec2(tp.x + 1.0f, tp.y + 1.0f), IM_COL32(0, 0, 0, static_cast<int>(200 * alpha)), e.text.c_str());
        ImU32 textCol = e.good ? IM_COL32(150, 255, 170, static_cast<int>(255 * alpha)) : IM_COL32(255, 230, 120, static_cast<int>(255 * alpha));
        dl->AddText(tp, textCol, e.text.c_str());
        y += tsz.y + 2.0f;
    }
#else
    (void)viewPos; (void)viewWidth; (void)viewHeight;
#endif
}

void GameScene::DrawCaptureOverlay(const ImVec2& viewPos, float viewWidth, float viewHeight) {
#ifdef USE_IMGUI
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 p0 = viewPos;
    ImVec2 p1(viewPos.x + viewWidth, viewPos.y + viewHeight);
    float t = stateTimer_;

    if (capturedByMiss_) {
        // 普通のミス：少しの間そのまま動かした後、一瞬の赤フラッシュだけで即やり直し（文字も暗転も出さない）
        float playTime = ParameterManager::GetInstance()->GetValue("Alert", "missPlayTime_", 0.6f);
        float missTime = ParameterManager::GetInstance()->GetValue("Alert", "missRestartTime_", 0.25f);
        if (t < playTime) {
            // 動いている間は画面の縁だけ薄く赤くして「ミスした」を伝える
            float edge = std::clamp(t / 0.1f, 0.0f, 1.0f);
            dl->AddRect(ImVec2(p0.x + 3.0f, p0.y + 3.0f), ImVec2(p1.x - 3.0f, p1.y - 3.0f), IM_COL32(255, 40, 40, static_cast<int>(200 * edge)), 0.0f, 0, 6.0f);
            return;
        }
        float k = (missTime > 0.0f) ? std::clamp((t - playTime) / missTime, 0.0f, 1.0f) : 1.0f;
        float flash = 0.7f * (1.0f - k) + 0.3f * k; // 消える直前まで赤く、そのまま読み直しへ
        dl->AddRectFilled(p0, p1, IM_COL32(255, 30, 30, static_cast<int>(255 * flash)));
        return;
    }

    // 赤フラッシュ（警報）：最初は強く、その後は点滅
    float flash = 0.0f;
    if (t < 0.35f) {
        flash = 0.65f * (1.0f - t / 0.35f);
    } else {
        flash = 0.18f + 0.12f * static_cast<float>(std::sin(t * 12.0));
    }
    dl->AddRectFilled(p0, p1, IM_COL32(255, 30, 30, static_cast<int>(255 * std::clamp(flash, 0.0f, 1.0f))));

    // 「捕獲」
    if (t > 0.25f) {
        float a = std::clamp((t - 0.25f) / 0.3f, 0.0f, 1.0f);
        ImGui::SetNextWindowPos(ImVec2(viewPos.x + viewWidth * 0.5f, viewPos.y + viewHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("CaptureUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(6.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, a), capturedByMiss_ ? "ミス" : "捕獲");
        ImGui::SetWindowFontScale(2.0f);
        if (capturedByMiss_) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, a * 0.9f), "最初からやり直し");
        } else if (alert_ && alert_->GetParams().strikeEnabled_ && !alert_->GetParams().enabled_) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, a * 0.9f), "%d 回見つかった  —  最初からやり直し", alert_->GetStrikes());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, a * 0.9f), "警戒度が上がりきった  —  最初からやり直し");
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }

    // 暗転（既存のアイリスの代わりに、中心から絞る黒い円）
    float captureTime = ParameterManager::GetInstance()->GetValue("Alert", "captureSceneTime_", 2.8f);
    float darkStart = captureTime - 1.2f;
    if (t > darkStart) {
        float k = std::clamp((t - darkStart) / 1.2f, 0.0f, 1.0f);
        // 画面全体を覆う黒と、中心に残る円（縮む）
        float maxR = std::sqrt(viewWidth * viewWidth + viewHeight * viewHeight) * 0.5f;
        float radius = maxR * (1.0f - k);
        ImVec2 c(viewPos.x + viewWidth * 0.5f, viewPos.y + viewHeight * 0.5f);
        // 円の外側を黒く：4方向の矩形 + 円の縁は多角形で近似
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, static_cast<int>(255 * k * k)));
        if (radius > 1.0f) {
            dl->AddCircle(c, radius, IM_COL32(0, 0, 0, 255), 64, 6.0f);
        }
    }
#else
    (void)viewPos; (void)viewWidth; (void)viewHeight;
#endif
}

void GameScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI

    if (player_ && player_->GetPrimitiveObject() == selectedPrimitive) {
        player_->DisplayImGui();
    }

    if (backgroundPlane_ && backgroundPlane_.get() == selectedPrimitive) {
        backgroundPlane_->DisplayImGui("Background Plane");
    }

    // 灰色の奥壁（背景板ポリゴン）の調整UI（常にインスペクターから操作可能）
    if (backgroundPlane_ && ImGui::CollapsingHeader("Background Wall (灰色の壁・背景板)")) {
        EulerTransform transform = backgroundPlane_->GetTransform();
        Material& mat = backgroundPlane_->GetMaterial();
        bool changed = false;

        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "【位置 (Translate)】");
        if (ImGui::DragFloat("X (左右)##bgX", &transform.translate.x, 0.5f)) changed = true;
        if (ImGui::DragFloat("Y (上下)##bgY", &transform.translate.y, 0.5f)) changed = true;
        if (ImGui::DragFloat("Z (奥行き・手前/奥)##bgZ", &transform.translate.z, 0.05f)) changed = true;
        ImGui::TextDisabled("※ Z=0がブロック・プレイヤー、Z>0が奥（初期値: Z=1.6）");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "【サイズ・回転】");
        if (ImGui::DragFloat3("スケール (幅/厚み/高さ)##bgScale", &transform.scale.x, 1.0f, 1.0f, 2000.0f)) changed = true;
        if (ImGui::DragFloat3("回転 (ラジアン)##bgRot", &transform.rotate.x, 0.01f)) changed = true;

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "【見た目・色】");
        if (ImGui::ColorEdit4("壁の色 (Color)##bgColor", &mat.color.x)) changed = true;
        if (ImGui::DragFloat("光沢 (Shininess)##bgShine", &mat.shininess, 0.5f, 0.1f, 100.0f)) changed = true;
        bool lightEnabled = (mat.lightingType == 1);
        if (ImGui::Checkbox("ライティング有効 (光を当てる)##bgLight", &lightEnabled)) {
            mat.lightingType = lightEnabled ? 1 : 0;
            changed = true;
        }

        if (changed) {
            backgroundPlane_->SetTranslation(transform.translate);
            backgroundPlane_->SetRotation(transform.rotate);
            backgroundPlane_->SetScale(transform.scale);
            backgroundPlane_->Update();
        }

        ImGui::Spacing();
        if (ImGui::Button("初期値に戻す##bgReset")) {
            transform.translate = { 100.0f, 20.0f, 1.6f };
            transform.rotate = { -std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f };
            transform.scale = { 300.0f, 1.0f, 150.0f };
            mat.color = { 0.28f, 0.30f, 0.35f, 1.0f };
            mat.lightingType = 1;
            mat.shininess = 20.0f;
            backgroundPlane_->SetTranslation(transform.translate);
            backgroundPlane_->SetRotation(transform.rotate);
            backgroundPlane_->SetScale(transform.scale);
            backgroundPlane_->Update();
        }
        ImGui::SameLine();
        if (ImGui::Button("設定を保存 (Save)##bgSave")) {
            SaveBackgroundConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("設定を再読込 (Load)##bgLoad")) {
            LoadBackgroundConfig();
        }
    }

    // 鎖の調整（この関数はエディタの「インスペクター」ウィンドウの中から呼ばれるので、別ウィンドウを開かず折りたたみで出す。
    // ImGui::Begin で別ウィンドウにするとドックの外に浮いてしまう）
    if (chainManager_ && ImGui::CollapsingHeader("Chain Settings")) {
        chainManager_->DrawImGui();
        ImGui::Separator();
        TransitionDirector::GetInstance()->DrawImGui();
        if (player_ && gameState_ == GameState::Playing && ImGui::Button("Debug: Reach Goal (start clear transition)")) {
            player_->ReachGoal(); // goalWaitTime 後にクリア遷移が始まる
        }
    }

    // 警戒度
    if (alert_ && ImGui::CollapsingHeader("Alert (警戒度)")) {
        alert_->DrawImGui();
    }

    // ブロック設計：ゲームビューへの重ね描きとマウス選択は毎フレーム、パネルは開いている時だけ
    if (map_) {
        BlockDesignPanel::DrawOverlays(map_.get(), gameCamera_);
        if (ImGui::CollapsingHeader("Switch & Door (スイッチとドアの連動)")) {
            BlockDesignPanel::DrawLinksPanel(map_.get(), gameCamera_, s_TargetMapFilePath);
        }
        if (ImGui::CollapsingHeader("Block Design (ブロック設計)")) {
            BlockDesignPanel::Draw(map_.get(), gameCamera_, s_TargetMapFilePath);
        }
    }

    // 崩れる床の調整（どれが崩れる床か／1枚ずつの通れる上限／ステージへの保存）
    if (map_ && ImGui::CollapsingHeader("Fragile Floors (崩れる床)")) {
        DrawFragileFloorImGui(map_.get(), gameCamera_, s_TargetMapFilePath);
    }

    if (ImGui::CollapsingHeader("Pause Menu (ポーズメニュー)")) {
        ImGui::Checkbox("ポーズ状態 (isPaused)", &isPaused_);
        const char* menuItems[] = { "0: リトライ (restartText)", "1: タイトル (titleText)" };
        ImGui::Combo("選択中項目", &pauseMenuIndex_, menuItems, IM_ARRAYSIZE(menuItems));
        if (ImGui::Button(isPaused_ ? "ポーズ解除 (Resume)" : "ポーズ実行 (Pause)")) {
            isPaused_ = !isPaused_;
            pausePulseTimer_ = 0.0f;
        }
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

    // 回数制の HUD（右上の目のアイコン）
    if (alert_ && alert_->GetParams().strikeEnabled_) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        int limit = alert_->GetStrikeLimit();
        int used = alert_->GetStrikes();
        float pulse = alert_->GetStrikePulse();
        const float rx = 15.0f, ry = 9.5f, gap = 40.0f, margin = 18.0f;
        float y = windowPos.y + margin + ry + 4.0f;
        if (alert_->GetParams().enabled_) y += 34.0f; // バーも出ている時はその下
        bool lastOne = (limit - used == 1);
        float blink = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 6.0));
        for (int i = 0; i < limit; ++i) {
            float cx = windowPos.x + windowWidth - margin - rx - (limit - 1 - i) * gap;
            ImVec2 c(cx, y);
            bool spent = (i < used);
            bool justSpent = spent && (i == used - 1) && pulse > 0.0f;
            float scale = justSpent ? (1.0f + 0.5f * pulse) : 1.0f;
            ImVec2 r(rx * scale, ry * scale);
            // 縁取り
            dl->AddEllipseFilled(c, ImVec2(r.x + 2.0f, r.y + 2.0f), IM_COL32(0, 0, 0, 170));
            if (spent) {
                // 使った：赤い目に ×
                dl->AddEllipseFilled(c, r, IM_COL32(200, 40, 40, 230));
                dl->AddLine(ImVec2(c.x - r.y * 0.7f, c.y - r.y * 0.7f), ImVec2(c.x + r.y * 0.7f, c.y + r.y * 0.7f), IM_COL32(255, 255, 255, 240), 2.5f);
                dl->AddLine(ImVec2(c.x - r.y * 0.7f, c.y + r.y * 0.7f), ImVec2(c.x + r.y * 0.7f, c.y - r.y * 0.7f), IM_COL32(255, 255, 255, 240), 2.5f);
            } else {
                // 残り：白い目。残り1つは点滅
                int alpha = (lastOne ? static_cast<int>(120 + 135 * blink) : 240);
                dl->AddEllipseFilled(c, r, IM_COL32(245, 245, 245, alpha));
                dl->AddCircleFilled(c, r.y * 0.55f, IM_COL32(30, 40, 60, alpha));
                dl->AddCircleFilled(ImVec2(c.x + r.y * 0.2f, c.y - r.y * 0.2f), r.y * 0.18f, IM_COL32(255, 255, 255, alpha));
            }
        }
        // 発見直後は画面の縁が赤く光る
        if (pulse > 0.0f) {
            ImVec2 p0 = windowPos;
            ImVec2 p1(windowPos.x + windowWidth, windowPos.y + windowHeight);
            dl->AddRect(ImVec2(p0.x + 4.0f, p0.y + 4.0f), ImVec2(p1.x - 4.0f, p1.y - 4.0f), IM_COL32(255, 40, 40, static_cast<int>(220 * pulse)), 0.0f, 0, 8.0f);
        }
        // 「残り N 回」のポップアップ
        float ty = y + ry + 8.0f;
        for (const auto& e : alert_->GetEvents()) {
            float t = std::clamp(e.age / 1.6f, 0.0f, 1.0f);
            float alpha = (t < 0.7f) ? 1.0f : (1.0f - (t - 0.7f) / 0.3f);
            ImVec2 tsz = ImGui::CalcTextSize(e.text.c_str());
            ImVec2 tp(windowPos.x + windowWidth - margin - tsz.x, ty - 14.0f * t);
            dl->AddText(ImVec2(tp.x + 1.0f, tp.y + 1.0f), IM_COL32(0, 0, 0, static_cast<int>(200 * alpha)), e.text.c_str());
            dl->AddText(tp, e.good ? IM_COL32(150, 255, 170, static_cast<int>(255 * alpha)) : IM_COL32(255, 200, 120, static_cast<int>(255 * alpha)), e.text.c_str());
            ty += tsz.y + 2.0f;
        }
    }

    // 値の警戒度の HUD（右上のバー。ハードモード用。OFF の時は出さない）
    if (alert_ && alert_->GetParams().enabled_) {
        DrawAlertHud(windowPos, windowWidth, windowHeight);
    }
    // 捕獲演出（回数制でも値でも同じ）
    if (alert_ && gameState_ == GameState::Captured) {
        DrawCaptureOverlay(windowPos, windowWidth, windowHeight);
    }

    // 警備員の頭上の合図：疑う・調べる =「？」（黄）、追跡 =「！」（赤）。追跡中は出続けるので「まだ追われている」が分かる
    if (map_) {
        ImDrawList* markList = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        for (const auto& block : map_->GetUpdateBlocks()) {
            auto* guard = dynamic_cast<GuardBlock*>(block.get());
            if (!guard || guard->IsDestroyed()) continue;
            GuardBlock::Mark mark = guard->GetMark();
            if (mark == GuardBlock::Mark::None) continue;
            float sx, sy;
            if (!BlockDesignPanel::WorldToScreen(gameCamera_, guard->GetMarkPosition(), sx, sy)) continue;
            bool excl = (mark == GuardBlock::Mark::Exclamation);
            const char* text = excl ? "!" : "?";
            float bob = static_cast<float>(std::sin(ImGui::GetTime() * (excl ? 14.0 : 6.0))) * (excl ? 3.0f : 2.0f);
            float size = excl ? 44.0f : 36.0f;
            ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
            ImVec2 pos(sx - ts.x * 0.5f, sy - ts.y + bob);
            ImU32 col = excl ? IM_COL32(255, 70, 60, 255) : IM_COL32(255, 220, 70, 255);
            markList->AddText(font, size, ImVec2(pos.x + 2.0f, pos.y + 2.0f), IM_COL32(0, 0, 0, 200), text);
            markList->AddText(font, size, pos, col, text);
            // 追跡中に見られ続けているゲージ（満タンでもう1回「発見」）
            if (excl) {
                float ratio = guard->GetExposureRatio();
                if (ratio > 0.0f) {
                    ImVec2 b0(sx - 20.0f, sy + 4.0f);
                    ImVec2 b1(sx + 20.0f, sy + 9.0f);
                    markList->AddRectFilled(ImVec2(b0.x - 1.0f, b0.y - 1.0f), ImVec2(b1.x + 1.0f, b1.y + 1.0f), IM_COL32(0, 0, 0, 180), 2.0f);
                    markList->AddRectFilled(b0, ImVec2(b0.x + 40.0f * ratio, b1.y), IM_COL32(255, 80, 60, 240), 2.0f);
                }
            }
        }
    }

    // 操作ガイド
    if (player_) {
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + 10.0f, windowPos.y + 10.0f), ImGuiCond_Always);
        ImGui::Begin("Operation Guide", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);

        ImGui::TextColored(ImVec4(1,1,1,0.8f), "[Operation Guide]");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "A/D or Left/Right : Move");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "SPACE : Jump");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "K : Take (pick up chain / take back from bound guard)");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "J : Put (drop chain / bind glowing guard, -1 chain)");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "W (hold) : Raise the gem overhead");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "A/D while raised on wooden plank : Throw it -> swing (A/D to pump)");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "Release W : Fly in the weight's direction");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "Guard : gem hit = stun / J near glowing guard = bind / K near bound = take back / dropped chain trips");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "Fragile floor : dots = chain units it can hold / red = too heavy, it shakes then falls");
        ImGui::End();
    }

    // Start Ready 演出
    if (gameState_ == GameState::StartReady) {
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowWidth / 2.0f, windowPos.y + windowHeight / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("ReadyUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(6.0f);
        if (stateTimer_ < 1.0f) {
            const char* text = "READY...";
            float textW = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textW) * 0.5f);
            ImGui::TextColored(ImVec4(1,0.5f,0,1), "%s", text);
        } else {
            const char* text = "GO!";
            float textW = ImGui::CalcTextSize(text).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textW) * 0.5f);
            ImGui::TextColored(ImVec4(0,1,0,1), "%s", text);
        }
        ImGui::End();
    }

    // Clear 演出（黒い円の遷移が動いている間はその演出に任せる）
    if (gameState_ == GameState::Clear && !TransitionDirector::GetInstance()->IsPlaying()) {
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowWidth / 2.0f, windowPos.y + windowHeight / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("ClearUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(6.0f);
        const char* clearText = "STAGE CLEAR!";
        float textWidth = ImGui::CalcTextSize(clearText).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textWidth) * 0.5f);
        ImGui::TextColored(ImVec4(1,0.8f,0,1), "%s", clearText);

        // 警戒度の評価（低く保つ理由）
        if (alert_) {
            AlertRank r = alert_->ComputeRank();
            ImGui::SetWindowFontScale(3.0f);
            char rankText[64];
            snprintf(rankText, sizeof(rankText), "%s  RANK %c",
                     (r.rank == 'S') ? "静穏" : (r.rank == 'A') ? "潜入" : (r.rank == 'B') ? "強行" : "騒然", r.rank);
            float rw = ImGui::CalcTextSize(rankText).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - rw) * 0.5f);
            ImVec4 rankColor = (r.rank == 'S') ? ImVec4(0.6f, 1.0f, 0.8f, 1.0f) : (r.rank == 'A') ? ImVec4(0.7f, 0.9f, 1.0f, 1.0f)
                             : (r.rank == 'B') ? ImVec4(1.0f, 0.9f, 0.5f, 1.0f) : ImVec4(1.0f, 0.6f, 0.5f, 1.0f);
            ImGui::TextColored(rankColor, "%s", rankText);
            ImGui::SetWindowFontScale(1.6f);
            char detail[128];
            snprintf(detail, sizeof(detail), "発見 %d 回   通報 %d 回   騒音 %d 回   最大警戒度 %.0f", r.spotted, r.reported, r.noises, r.peak);
            float dw = ImGui::CalcTextSize(detail).x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - dw) * 0.5f);
            ImGui::TextColored(ImVec4(1, 1, 1, 0.9f), "%s", detail);
        }

        ImGui::SetWindowFontScale(2.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30.0f);
        const char* returnText = "Press SPACE to Return Title";
        float returnWidth = ImGui::CalcTextSize(returnText).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - returnWidth) * 0.5f);
        
        static float time = 0.0f;
        time += ImGui::GetIO().DeltaTime;
        float alpha = (sinf(time * 5.0f) + 1.0f) * 0.5f;
        ImGui::TextColored(ImVec4(1,1,1,alpha), "%s", returnText);
        ImGui::End();
    }

    // Game Over (ミス) 演出
    if (player_ && player_->IsDead() && gameState_ != GameState::Captured) {
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

    // ポーズ中の操作ガイド表示
    if (isPaused_) {
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowWidth * 0.5f, windowPos.y + windowHeight * 0.82f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("PauseGuideOverlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 0.9f), "W / S : 選択    SPACE / ENTER : 決定    TAB : 再開");
        ImGui::End();
    }
#endif
}

void GameScene::UpdateGuardLights() {
    if (!modelCommon_ || !map_) return;

    SpotLightGroup* slGroup = modelCommon_->GetSpotLightGroup();
    if (!slGroup) return;

    int32_t currentCount = 0;
    bool hasLightEditor = false;
#ifdef USE_IMGUI
    if (auto* editorMgr = EditorManager::GetInstance()) {
        if (auto* lightEditor = editorMgr->GetLightEditor()) {
            // LightEditor管理下の静的ライト数を基準とする
            currentCount = static_cast<int32_t>((std::min)(lightEditor->GetSpotLights().size(), static_cast<size_t>(kMaxSpotLights)));
            hasLightEditor = true;
        }
    }
#endif
    if (!hasLightEditor && slGroup->spotLightCount > 0) {
        // USE_IMGUI非定義時やLightEditorがない場合は既存のカウントを基準とする
        currentCount = (std::min)(slGroup->spotLightCount, static_cast<int32_t>(kMaxSpotLights));
    }

    // 1. マップ上のアクティブな GuardBlock を収集し、代表シャドウキャスター（プレイヤーに最も近いもの）を決定
    Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f};
    GuardBlock* primaryGuard = nullptr;
    float closestDistSq = 1e9f;

    std::vector<GuardBlock*> activeGuards;
    for (const auto& blockPtr : map_->GetUpdateBlocks()) {
        if (!blockPtr || blockPtr->IsDestroyed()) continue;
        if (auto* guard = dynamic_cast<GuardBlock*>(blockPtr.get())) {
            if (guard->IsLightActive()) {
                activeGuards.push_back(guard);
                if (guard->IsShadowEnabled()) {
                    Vector3 guardPos = guard->GetLightPosition();
                    float dx = guardPos.x - playerPos.x;
                    float dy = guardPos.y - playerPos.y;
                    float distSq = dx * dx + dy * dy;
                    if (distSq < closestDistSq) {
                        closestDistSq = distSq;
                        primaryGuard = guard;
                    }
                }
            }
        }
    }

    // GuardBlock が影を生成する場合、LightEditor 側の全ライトの影を無効化（単一シャドウマップの競合防止）
    if (primaryGuard) {
        for (int32_t i = 0; i < currentCount; ++i) {
            slGroup->spotLights[i].shadowMapIndex = -1;
        }
    }

    // 各 GuardBlock のライトを追加
    for (auto* guard : activeGuards) {
        if (currentCount < static_cast<int32_t>(kMaxSpotLights)) {
            SpotLight sl = guard->GetSpotLightData();
            if (sl.enable != 0) {
                // 代表シャドウキャスターのみ shadowMapIndex = 0、それ以外は -1
                if (guard == primaryGuard) {
                    sl.shadowMapIndex = 0;
                } else {
                    sl.shadowMapIndex = -1;
                }
                slGroup->spotLights[currentCount] = sl;
                currentCount++;
            }
        }
    }
    slGroup->spotLightCount = currentCount;
}

void GameScene::RenderShadowPass() {
    Matrix4x4 lightVP = TransformFunctions::MakeIdentity4x4();
    bool hasLightVP = false;

    // 1. まず SpotLightGroup からシャドウが有効なライト（選定された GuardBlock 等）の viewProjection を取得
    if (modelCommon_) {
        SpotLightGroup* slg = modelCommon_->GetSpotLightGroup();
        if (slg) {
            for (int32_t i = 0; i < slg->spotLightCount; ++i) {
                if (slg->spotLights[i].enable != 0 && slg->spotLights[i].shadowMapIndex >= 0) {
                    lightVP = slg->spotLights[i].viewProjection;
                    hasLightVP = true;
                    break;
                }
            }
        }
    }

    // 2. SpotLightGroup になければ、LightEditor の代表ライトを確認
#ifdef USE_IMGUI
    if (!hasLightVP && EditorManager::GetInstance() && EditorManager::GetInstance()->GetLightEditor()) {
        hasLightVP = EditorManager::GetInstance()->GetLightEditor()->GetPrimaryShadowViewProjection(&lightVP);
    }
#endif

    if (!hasLightVP) {
        return;
    }

    Renderer* renderer = Renderer::GetInstance();
    if (!renderer) {
        return;
    }

    // 1. シャドウマップパスの開始（シャドウDSVバインド・深度クリア）
    renderer->BeginShadowPass(lightVP);

    // 2. キャスター（影を落とすオブジェクト群）の描画
    if (map_) {
        map_->Draw();
    }
    if (player_) {
        player_->Draw();
    }
    if (chainManager_) {
        chainManager_->Draw();
    }

    // コンポーネント（MeshRenderer / PrimitiveRenderer）の描画
    renderer->RenderComponents();

    // 3. シャドウマップパスの終了（メインレンダーターゲット復元）
    renderer->EndShadowPass();
}

void GameScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 警備員の懐中電灯スポットライトを最新化
    UpdateGuardLights();

    // 0. シャドウマップパス（スポットライト視点から深度描画）
    RenderShadowPass();

    // ブロック設計パネルの重ね描きは、この描画に使われた行列で位置を合わせる（マップチップ画面の専用カメラにも対応）
    BlockDesignPanel::SetRenderViewProjection(viewProjectionMatrix);

    // 1. Skyboxの描画
    if (skybox_) {
        skybox_->Draw();
    }

    // 1.5. 背景板ポリゴンの描画
    if (backgroundPlane_) {
        backgroundPlane_->Draw();
    }

    // 2. 3Dモデル（マップ・プレイヤー）の描画準備
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    // マップの描画
    if (map_) {
        map_->Draw();
    }

    // プレイヤーの描画
    if (player_) {
        player_->Draw();
    }

    // 鎖の描画
    if (chainManager_) {
        chainManager_->Draw();
    }

    // ステージクリア遷移（持ち越し中の宝石と鎖 + 黒い穴あき板。同じ3Dパスなので深度で穴の外が隠れる）
    TransitionDirector::GetInstance()->Draw();


    // コンポーネントの描画を実行
    Renderer::GetInstance()->RenderComponents();

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
        if (!TransitionDirector::GetInstance()->IsCovering()) { // 黒で絞っている間は紙吹雪等を上に描かない
            particleCommon_->DrawAll(viewProjectionMatrix);
        }
#ifdef USE_IMGUI
    }
#endif

    // 4. ポーズメニューの描画 (最前面)
    DrawPauseMenu();
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

            // 3. スポットライトの危険光・当たり判定オーバーレイ描画
            if (auto* editorMgr = EditorManager::GetInstance()) {
                if (auto* lightEditor = editorMgr->GetLightEditor()) {
                    AABB2D playerAABB = player_->GetAABB();
                    lightEditor->DrawOverlay(viewProjectionMatrix, gameViewPos, gameViewSize, &playerAABB);
                }
            }

            // 4. 警備員の懐中電灯（VisionCone）の可視化オーバーレイ描画
            if (map_) {
                for (const auto& blockPtr : map_->GetUpdateBlocks()) {
                    if (!blockPtr || blockPtr->IsDestroyed()) continue;
                    if (auto* guard = dynamic_cast<GuardBlock*>(blockPtr.get())) {
                        VisionCone cone = guard->GetVisionCone();
                        Vector3 eyePos = cone.eyePosition;
                        Vector3 ndcEye = TransformFunctions::EulerTransform(eyePos, viewProjectionMatrix);
                        if (ndcEye.z >= 0.0f && ndcEye.z <= 1.0f) {
                            ImVec2 pEye(
                                gameViewPos.x + (ndcEye.x + 1.0f) * 0.5f * gameViewSize.x,
                                gameViewPos.y + (1.0f - ndcEye.y) * 0.5f * gameViewSize.y
                            );

                            Vector4 lightCol = guard->GetCurrentLightColor();
                            if (!guard->IsLightActive()) {
                                lightCol = { 0.4f, 0.4f, 0.4f, 0.5f };
                            }
                            ImU32 colLine = IM_COL32(
                                static_cast<int>(lightCol.x * 255),
                                static_cast<int>(lightCol.y * 255),
                                static_cast<int>(lightCol.z * 255),
                                200
                            );
                            ImU32 colFill = IM_COL32(
                                static_cast<int>(lightCol.x * 255),
                                static_cast<int>(lightCol.y * 255),
                                static_cast<int>(lightCol.z * 255),
                                35
                            );

                            float baseAngle = std::atan2(cone.forward.y, cone.forward.x);
                            float halfAngle = cone.halfAngleRad;
                            constexpr int kArcSegs = 16;
                            std::vector<ImVec2> polyPoints;
                            polyPoints.reserve(kArcSegs + 2);
                            polyPoints.push_back(pEye);

                            for (int seg = 0; seg <= kArcSegs; ++seg) {
                                float t = static_cast<float>(seg) / static_cast<float>(kArcSegs);
                                float ang = baseAngle - halfAngle + (halfAngle * 2.0f) * t;
                                Vector3 rimPt = {
                                    eyePos.x + std::cos(ang) * cone.distance,
                                    eyePos.y + std::sin(ang) * cone.distance,
                                    eyePos.z
                                };
                                Vector3 ndcRim = TransformFunctions::EulerTransform(rimPt, viewProjectionMatrix);
                                ImVec2 pRim(
                                    gameViewPos.x + (ndcRim.x + 1.0f) * 0.5f * gameViewSize.x,
                                    gameViewPos.y + (1.0f - ndcRim.y) * 0.5f * gameViewSize.y
                                );
                                polyPoints.push_back(pRim);
                            }

                            if (polyPoints.size() >= 3) {
                                drawList->AddConvexPolyFilled(polyPoints.data(), static_cast<int>(polyPoints.size()), colFill);
                                drawList->AddPolyline(polyPoints.data(), static_cast<int>(polyPoints.size()), colLine, true, 1.5f);
                            }

                            // 光の最大到達範囲（外枠ガイド線）
                            float lightDist = guard->GetLightDistance();
                            if (lightDist > cone.distance + 0.1f) {
                                std::vector<ImVec2> lightGuidePoints;
                                lightGuidePoints.reserve(kArcSegs + 2);
                                lightGuidePoints.push_back(pEye);
                                for (int seg = 0; seg <= kArcSegs; ++seg) {
                                    float t = static_cast<float>(seg) / static_cast<float>(kArcSegs);
                                    float ang = baseAngle - halfAngle + (halfAngle * 2.0f) * t;
                                    Vector3 rimPt = {
                                        eyePos.x + std::cos(ang) * lightDist,
                                        eyePos.y + std::sin(ang) * lightDist,
                                        eyePos.z
                                    };
                                    Vector3 ndcRim = TransformFunctions::EulerTransform(rimPt, viewProjectionMatrix);
                                    ImVec2 pRim(
                                        gameViewPos.x + (ndcRim.x + 1.0f) * 0.5f * gameViewSize.x,
                                        gameViewPos.y + (1.0f - ndcRim.y) * 0.5f * gameViewSize.y
                                    );
                                    lightGuidePoints.push_back(pRim);
                                }
                                ImU32 colGuide = IM_COL32(
                                    static_cast<int>(lightCol.x * 255),
                                    static_cast<int>(lightCol.y * 255),
                                    static_cast<int>(lightCol.z * 255),
                                    70
                                );
                                drawList->AddPolyline(lightGuidePoints.data(), static_cast<int>(lightGuidePoints.size()), colGuide, true, 1.0f);
                            }
                        }
                    }
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

    // 鎖のリンクモデルをヒエラルキーに表示する
    if (chainManager_) {
        auto links = chainManager_->GetLinkObjects();
        result.insert(result.end(), links.begin(), links.end());
    }
    // 遷移中の宝石・鎖・黒板
    {
        auto objs = TransitionDirector::GetInstance()->GetObjects();
        result.insert(result.end(), objs.begin(), objs.end());
    }

    return result;
}

std::vector<PrimitiveObject *> GameScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;

    // 1. プレイヤー
    if (player_) {
        result.push_back(player_->GetPrimitiveObject());
    }

    // 2. マップチップ
    if (map_) {
        auto mapPrims = map_->GetPrimitiveObjects();
        result.insert(result.end(), mapPrims.begin(), mapPrims.end());
    }

    // 3. 背景板ポリゴン
    if (backgroundPlane_) {
        result.push_back(backgroundPlane_.get());
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
        UpdateGuardLights();
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

    if (backgroundPlane_) {
        backgroundPlane_->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }
}

Vector2 GameScene::WorldToScreenUV(const Vector3& worldPos) const {
    if (!gameCamera_) {
        return Vector2(0.5f, 0.5f);
    }
    // カメラのビュープロジェクション行列を取得
    Matrix4x4 viewProj = gameCamera_->GetViewMatrix() * gameCamera_->GetProjectionMatrix();
    Vector3 ndc = TransformFunctions::EulerTransform(worldPos, viewProj);

    // NDC [-1, 1] から テクスチャUV [0, 1] へ変換（Y軸反転）
    float uvX = (ndc.x + 1.0f) * 0.5f;
    float uvY = (1.0f - ndc.y) * 0.5f;
    return Vector2(uvX, uvY);
}

void GameScene::StartIrisIn(const Vector3& playerPos, float duration) {
    isIrisInActive_ = true;
    irisInTimer_ = 0.0f;
    irisInDuration_ = (duration > 0.0f) ? duration : 1.2f;
    // 範囲を従来の2倍（3.2f）に設定して画面全体に大きく広がるようにする
    irisInMaxRadius_ = ParameterManager::GetInstance()->GetValue("GameScene", "irisInMaxRadius", 3.2f);

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        Vector2 uv = WorldToScreenUV(playerPos);
        dxCommon->SetIrisCenter(uv.x, uv.y);
        dxCommon->SetIrisRadius(0.0f);
        dxCommon->SetIrisSmoothness(0.03f);
        dxCommon->SetIrisIn(true); // Iris In (開く)
        dxCommon->SetIrisMaskColor(0.0f, 0.0f, 0.0f, 1.0f);
        dxCommon->SetCompositeIrisEnabled(true);
    }

    transitionAlpha_ = 0.0f;
}

void GameScene::UpdateIrisIn(const Vector3& playerPos, float dt) {
    if (!isIrisInActive_) return;

    irisInTimer_ += dt;
    float t = std::clamp(irisInTimer_ / irisInDuration_, 0.0f, 1.0f);

    // 線形補間（Linear: イーズイン/イーズアウトは不使用）
    float currentRadius = t * irisInMaxRadius_;

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        Vector2 uv = WorldToScreenUV(playerPos);
        dxCommon->SetIrisCenter(uv.x, uv.y);
        dxCommon->SetIrisRadius(currentRadius);
        dxCommon->SetIrisSmoothness(0.03f);
        dxCommon->SetIrisIn(true);
        dxCommon->SetCompositeIrisEnabled(true);
    }

    if (t >= 1.0f) {
        isIrisInActive_ = false;
        if (dxCommon) {
            dxCommon->SetCompositeIrisEnabled(false);
        }
    }
}

void GameScene::SaveBackgroundConfig() {
    if (!backgroundPlane_) return;
    EulerTransform t = backgroundPlane_->GetTransform();
    Material& m = backgroundPlane_->GetMaterial();
    nlohmann::json j;
    j["translate"] = { t.translate.x, t.translate.y, t.translate.z };
    j["rotate"] = { t.rotate.x, t.rotate.y, t.rotate.z };
    j["scale"] = { t.scale.x, t.scale.y, t.scale.z };
    j["color"] = { m.color.x, m.color.y, m.color.z, m.color.w };
    j["shininess"] = m.shininess;
    j["lightingType"] = m.lightingType;

    try {
        std::filesystem::path p("resources/json/shared/background_wall_config.json");
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream ofs(p);
        if (ofs.is_open()) {
            ofs << j.dump(4);
        }
    } catch (...) {}
}

void GameScene::LoadBackgroundConfig() {
    if (!backgroundPlane_) return;
    try {
        std::ifstream ifs("resources/json/shared/background_wall_config.json");
        if (!ifs.is_open()) return;
        nlohmann::json j;
        ifs >> j;
        EulerTransform t = backgroundPlane_->GetTransform();
        if (j.contains("translate") && j["translate"].is_array() && j["translate"].size() >= 3) {
            t.translate = { j["translate"][0].get<float>(), j["translate"][1].get<float>(), j["translate"][2].get<float>() };
        }
        if (j.contains("rotate") && j["rotate"].is_array() && j["rotate"].size() >= 3) {
            t.rotate = { j["rotate"][0].get<float>(), j["rotate"][1].get<float>(), j["rotate"][2].get<float>() };
        }
        if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
            t.scale = { j["scale"][0].get<float>(), j["scale"][1].get<float>(), j["scale"][2].get<float>() };
        }
        backgroundPlane_->SetTranslation(t.translate);
        backgroundPlane_->SetRotation(t.rotate);
        backgroundPlane_->SetScale(t.scale);

        auto& m = backgroundPlane_->GetMaterial();
        if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
            m.color = { j["color"][0].get<float>(), j["color"][1].get<float>(), j["color"][2].get<float>(), j["color"][3].get<float>() };
        }
        if (j.contains("shininess") && j["shininess"].is_number()) {
            m.shininess = j["shininess"].get<float>();
        }
        if (j.contains("lightingType") && j["lightingType"].is_number()) {
            m.lightingType = j["lightingType"].get<int>();
        }
        backgroundPlane_->Update();
    } catch (...) {}
}

void GameScene::UpdatePauseMenu(float dt, SceneManager* sceneManager) {
    pausePulseTimer_ += dt;

    auto kb = KeyboardInput::GetInstance();
    auto pad = GamepadInput::GetInstance();

    // W / S キーでメニュー項目の切り替え (上下矢印キー・アナログスティックは使用しない)
    bool moveUp = false;
    bool moveDown = false;

    if (kb->IsKeyPressed(DIK_W)) {
        moveUp = true;
    }
    if (kb->IsKeyPressed(DIK_S)) {
        moveDown = true;
    }

    if (moveUp) {
        pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 2; // 0 <-> 1
    } else if (moveDown) {
        pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 2; // 0 <-> 1
    }

    // 決定入力 (Space / Enter / パッド Aボタン)
    bool isDecision = false;
    if (kb->IsKeyPressed(DIK_SPACE) || kb->IsKeyPressed(DIK_RETURN) || kb->IsKeyPressed(DIK_NUMPADENTER)) {
        isDecision = true;
    }
    if (pad && pad->IsButtonPressed(0)) {
        isDecision = true;
    }

    if (isDecision) {
        if (pauseMenuIndex_ == 0) {
            // リトライ: 現在のステージを最初からリスタート
            isPaused_ = false;
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
            return;
        } else if (pauseMenuIndex_ == 1) {
            // タイトル: タイトル画面へ遷移
            isPaused_ = false;
#ifdef USE_IMGUI
            if (EditorManager::GetInstance()) {
                EditorManager::GetInstance()->SetCurrentSceneType(SceneType::kTitle);
                EditorManager::GetInstance()->SetUseDebugCamera(false);
            }
            EditorManager::SetPlaying(true);
#endif
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
            return;
        }
    }

    // 各スプライトのトランスフォーム・カラー更新
    if (pauseBackdropSprite_) {
        pauseBackdropSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.65f });
        pauseBackdropSprite_->Update();
    }
    if (pauseTitleSprite_) {
        pauseTitleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        pauseTitleSprite_->Update();
    }

    // 選択項目のパルス・ハイライト演出
    float pulse = (sinf(pausePulseTimer_ * 6.0f) * 0.5f + 0.5f) * 0.25f; // 0.0 ~ 0.25
    Vector4 highlightColor = { 1.0f, 0.88f + pulse * 0.12f, 0.20f, 1.0f }; // ゴールド/イエロー
    Vector4 unselectedColor = { 0.60f, 0.60f, 0.60f, 0.75f };               // 控えめなグレー/白

    // リトライ項目 (restartText.png: 500x100)
    if (pauseRestartSprite_) {
        const float baseW = 280.0f;
        const float baseH = 56.0f;
        if (pauseMenuIndex_ == 0) {
            float scale = 1.08f + pulse * 0.04f;
            float w = baseW * scale;
            float h = baseH * scale;
            pauseRestartSprite_->SetSize({ w, h });
            pauseRestartSprite_->SetPosition({ (1280.0f - w) * 0.5f, 320.0f - (h - baseH) * 0.5f });
            pauseRestartSprite_->SetColor(highlightColor);
        } else {
            pauseRestartSprite_->SetSize({ baseW, baseH });
            pauseRestartSprite_->SetPosition({ (1280.0f - baseW) * 0.5f, 320.0f });
            pauseRestartSprite_->SetColor(unselectedColor);
        }
        pauseRestartSprite_->Update();
    }

    // タイトル項目 (titleText.png: 500x100)
    if (pauseTitleTextSprite_) {
        const float baseW = 280.0f;
        const float baseH = 56.0f;
        if (pauseMenuIndex_ == 1) {
            float scale = 1.08f + pulse * 0.04f;
            float w = baseW * scale;
            float h = baseH * scale;
            pauseTitleTextSprite_->SetSize({ w, h });
            pauseTitleTextSprite_->SetPosition({ (1280.0f - w) * 0.5f, 430.0f - (h - baseH) * 0.5f });
            pauseTitleTextSprite_->SetColor(highlightColor);
        } else {
            pauseTitleTextSprite_->SetSize({ baseW, baseH });
            pauseTitleTextSprite_->SetPosition({ (1280.0f - baseW) * 0.5f, 430.0f });
            pauseTitleTextSprite_->SetColor(unselectedColor);
        }
        pauseTitleTextSprite_->Update();
    }
}

void GameScene::DrawPauseMenu() {
    if (!isPaused_) return;
    if (!spriteCommon_) return;

    spriteCommon_->PreDraw();

    // 1. 暗幕背景
    if (pauseBackdropSprite_) {
        pauseBackdropSprite_->Draw();
    }
    // 2. 「ポーズ」見出し
    if (pauseTitleSprite_) {
        pauseTitleSprite_->Draw();
    }
    // 3. 「リトライ」項目
    if (pauseRestartSprite_) {
        pauseRestartSprite_->Draw();
    }
    // 4. 「タイトル」項目
    if (pauseTitleTextSprite_) {
        pauseTitleTextSprite_->Draw();
    }
}