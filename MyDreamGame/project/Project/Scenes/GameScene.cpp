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

    // エフェクトマネージャーの生成と初期化
    effectManager_ = std::make_unique<GameEffectManager>();
    effectManager_->Initialize();
    Log("GameScene::Initialize: EffectManager Initialized\n");

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

    // 8. 開始演出・クリア演出用UIの初期化
    readyGoUI_ = std::make_unique<ReadyGoUI>();
    readyGoUI_->Initialize(GetSpriteCommon());

    clearUI_ = std::make_unique<ClearUI>();
    clearUI_->Initialize(GetSpriteCommon());

    // ポーズメニューとコインHUDの初期化
    pauseMenu_ = std::make_unique<PauseMenu>();
    pauseMenu_->Initialize(GetSpriteCommon());

    coinHUD_ = std::make_unique<CoinHUD>();
    coinHUD_->Initialize(GetSpriteCommon());

    replayController_ = std::make_unique<GameReplayController>();
    ghostRenderer_ = std::make_unique<ReplayGhostRenderer>();

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

    if (skybox_) {
        skybox_->Update();
    }

    float dt = TimeManager::GetInstance().GetDeltaTime();

    if (effectManager_) {
        effectManager_->Update(dt, isGameActive, GetGameCamera());
    }

    // フェードイン演出
    float transitionSpeed = ParameterManager::GetInstance()->GetValue("GameScene", "transitionSpeed", 1.5f);
    if (transitionAlpha_ > 0.0f) {
        transitionAlpha_ -= dt * transitionSpeed;
        if (transitionAlpha_ < 0.0f) transitionAlpha_ = 0.0f;
    }

    if (gameState_ == GameState::StartReady) {
        if (readyGoUI_) {
            if (readyGoUI_->Update(dt)) {
                gameState_ = GameState::Playing;
            }
        }
    } else if (gameState_ == GameState::Clear) {
        if (clearUI_) {
            clearUI_->Update(dt, GetGameCamera());
            if (clearUI_->IsReturnRequested()) {
                clearUI_->Reset(GetGameCamera());
                sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
                return;
            }
        }
    } else {
        if (clearUI_) {
            clearUI_->Reset(GetGameCamera());
        }
    }

    if (gameState_ != GameState::StartReady) {
        if (readyGoUI_) {
            readyGoUI_->Reset();
        }
    }

    // ポーズ画面への移行（ゲームプレイ中にESCまたはコントローラーのSTARTボタン）
    if (gameState_ == GameState::Playing) {
        if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_ESCAPE) ||
            GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start)) {
            gameState_ = GameState::Pause;
            if (pauseMenu_) {
                pauseMenu_->Reset();
            }
        }
    } else if (gameState_ == GameState::Pause) {
        if (pauseMenu_) {
            PauseMenuAction action = pauseMenu_->Update(dt);
            if (action == PauseMenuAction::Resume) {
                gameState_ = GameState::Playing;
            } else if (action == PauseMenuAction::ExitToStageSelect) {
                if (GetGameCamera()) {
                    GetGameCamera()->SetScale(initialCameraScale_);
                    GetGameCamera()->SetFollowOffset({0.0f, 0.0f, 0.0f});
                }
                sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kStageSelect));
                return;
            }
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
        if (replayController_ && effectManager_) {
            replayController_->UpdatePlayback(player_, map_.get(), GetGameCamera(), effectManager_->GetEffects(), previousScore_);
        }

            if (player_) {
                bool isRespawning = player_->IsRespawning();
                if (isRespawning && !wasRespawning_) {
                    if (effectManager_) {
                        effectManager_->ResetAllEffects();
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
                if (readyGoUI_) {
                    canControl = readyGoUI_->CanControl();
                }
            } else if (gameState_ == GameState::Clear) {
                canControl = false;
            }

            player_->UpdateWithMap(*map_, GetGameCamera() && GetGameCamera()->IsTransitioning(), canControl);

            // ゴール判定
            if (gameState_ == GameState::Playing && player_->IsGoalComplete()) {
                gameState_ = GameState::Clear;
                if (clearUI_) {
                    clearUI_->Start(initialCameraScale_);
                }
                if (GetGameCamera()) {
                    GetGameCamera()->SetRooms({}); // 部屋境界制限を解除してプレイヤー中心へ直接追従
                }
            }

            // コイン獲得エフェクト
            int currentScore = player_->GetScore();
            if (currentScore > previousScore_) {
                if (effectManager_) {
                    effectManager_->EmitCoin(player_->GetPosition());
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
        if (replayController_) {
            replayController_->UpdateRecording(player_, map_.get(), GetGameCamera(), isCurrentlyPlaying);
        }
    }

    if (map_ && coinHUD_) {
        coinHUD_->Update(dt, map_->GetCollectedCoinCount(), map_->GetTotalCoinCount());
    }
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

    if (effectManager_) {
        effectManager_->Draw(GetParticleCommon(), viewProjectionMatrix);
    }

    // ゴースト残像・AI探索パスの描画
    if (ghostRenderer_) {
        ghostRenderer_->Draw3D(player_);
    }

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
            if (coinHUD_) {
                coinHUD_->Draw();
            }
        }
    }
    if (gameState_ == GameState::StartReady) {
        if (GetSpriteCommon() && readyGoUI_) {
            GetSpriteCommon()->PreDraw();
            readyGoUI_->Draw();
        }
    } else if (gameState_ == GameState::Clear) {
        if (GetSpriteCommon() && clearUI_) {
            GetSpriteCommon()->PreDraw();
            clearUI_->Draw();
        }
    } else if (gameState_ == GameState::Pause) {
        if (GetSpriteCommon() && pauseMenu_) {
            GetSpriteCommon()->PreDraw();
            pauseMenu_->Draw();
        }
    }
}

void GameScene::DrawEditorOverlay(const Matrix4x4 &viewProjectionMatrix) {
    if (ghostRenderer_) {
        ghostRenderer_->DrawEditorOverlay(player_, viewProjectionMatrix);
    }
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

    // 1. エフェクト群
    if (effectManager_) {
        auto effectPrims = effectManager_->GetPrimitives();
        result.insert(result.end(), effectPrims.begin(), effectPrims.end());
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
    if (effectManager_) {
        effectManager_->Update(dt, false, GetGameCamera());
    }

    // フェードイン演出 (エディタ停止中もフェードインさせる)
    if (transitionAlpha_ > 0.0f) {
        transitionAlpha_ -= dt * 1.5f;
        if (transitionAlpha_ < 0.0f) transitionAlpha_ = 0.0f;
    }

    // 録画状態のままエディタが停止した場合、確実に停止させて履歴に保存する
    if (replayController_) {
        replayController_->StopRecording();
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

