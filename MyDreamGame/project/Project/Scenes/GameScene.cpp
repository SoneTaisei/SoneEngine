#include "GameScene.h"
#include "Scene/SceneManager.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/GameCamera.h"
#include "Scene/SceneFactory.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "Editor/EditorManager.h"
#endif
#include "Editor/ReplayManager.h"
#include "Renderer/Renderer.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"
#include "Graphics/Skybox.h"
#include "Core/Utility/ParameterManager.h"

std::string GameScene::s_TargetMapFilePath = "resources/json/Map/map_data.json";

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
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/skybox/skybox_highres_build.dds");
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
    Log("GameScene::Initialize: Player Initialized\n");
    
    player_->FindSpawnPoint(*map_);
    Log("GameScene::Initialize: Player SpawnPoint found\n");

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
    Log("GameScene::Initialize: Finish\n");
}

void GameScene::Update(SceneManager *sceneManager) {
    if (coinEffect_) {
        coinEffect_->Update(1.0f / 60.0f);
    }
    if (ringEffect_) {
        ringEffect_->Update(1.0f / 60.0f);
    }
    if (cylinderEffect_) {
        cylinderEffect_->Update(1.0f / 60.0f);
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
        if (stateTimer_ > startReadyTime) {
            gameState_ = GameState::Playing;
            stateTimer_ = 0.0f;
        }
    } else if (gameState_ == GameState::Clear) {
        stateTimer_ += dt;
        if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE)) {
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
            return;
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
        }
        wasCurrentlyPlaying_ = isCurrentlyPlaying;

        bool isRewinding = false;
        if (isCurrentlyPlaying && !ReplayManager::GetInstance()->IsPlaying()) {
            auto keyboard = KeyboardInput::GetInstance();
            if ((keyboard->IsKeyDown(DIK_LCONTROL) || keyboard->IsKeyDown(DIK_RCONTROL)) &&
                keyboard->IsKeyDown(DIK_LEFT)) {
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
                        player_->SimulateCollisions(*map_);
                    }
                    
                    // 今ポップしたフレームの座標でも判定しておく
                    player_->SetPosition(poppedFrame.position);
                    player_->SimulateCollisions(*map_);
                    
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
                    
                    // 3. 0フレーム目から現在フレームまで、記録された座標をたどってコインを回収
                    for (int i = 0; i <= curFrame; ++i) {
                        player_->SetPosition(replayData.frames[i].position);
                        player_->SimulateCollisions(*map_);
                    }

                    // 4. コイン回収用に座標を動かしたので、シミュレーション再開用の正しい座標に戻す
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

            if (gameCamera_ && map_) {
                gameCamera_->SetRooms(map_->GetRooms());
            }

            // マップの更新をプレイヤーより先に行う（移動リフト等の新しい座標に対して判定するため）
            if (map_) {
                map_->Update();
            }

            player_->UpdateWithMap(*map_, gameCamera_ && gameCamera_->IsTransitioning());

            // ゴール判定
            if (gameState_ == GameState::Playing && player_->IsGoalComplete()) {
                gameState_ = GameState::Clear;
                stateTimer_ = 0.0f;
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
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "A/D or Left/Right : Move");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "SPACE : Jump / Wall Jump");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "J : Dash");
        ImGui::TextColored(ImVec4(1,1,1,0.8f), "K : Wall Cling (W/S to Climb)");
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

    // Clear 演出
    if (gameState_ == GameState::Clear) {
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowWidth / 2.0f, windowPos.y + windowHeight / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("ClearUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(6.0f);
        const char* clearText = "STAGE CLEAR!";
        float textWidth = ImGui::CalcTextSize(clearText).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textWidth) * 0.5f);
        ImGui::TextColored(ImVec4(1,0.8f,0,1), "%s", clearText);

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
    // Skyboxの描画前にDescriptorHeapをセットさせるため、PreDrawを呼ぶ
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    if (skybox_) {
        skybox_->Draw();
        
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }

    // 2. 2Dオブジェクト（マップ・プレイヤー）の描画
    // ModelCommonの描画前処理
    modelCommon_->PreDraw();

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
        if (EditorManager::IsShowEffects()) {
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
                // 全フレーム描画すると線のように繋がってしまうため、一定間隔（例：10フレーム毎）で描画して軌跡を表現
                const int FRAME_STEP = 10;

                auto* playerPrim = player_->GetPrimitiveObject();
                // エディターの「Show Trail」がONのときだけ残像を描画する
                if (playerPrim && playerPrim->GetShowTrail()) {
                    // クリップ矩形を設定して、GameViewの外に線や点がはみ出ないようにする
                    ImVec2 gameViewPos = EditorManager::GetGameViewPos();
                    ImVec2 gameViewSize = EditorManager::GetGameViewSize();
                    ImDrawList* drawList = ImGui::GetForegroundDrawList();
                    drawList->PushClipRect(gameViewPos, ImVec2(gameViewPos.x + gameViewSize.x, gameViewPos.y + gameViewSize.y), true);

                    // 描画前にゴースト用のインデックスをリセット
                    playerPrim->ResetGhostIndex();

                    // ベースとなるプレイヤーのTransformと色を取得
                    Vector4 baseColor = playerPrim->GetMaterial().color;

                    // 選択されているリプレイの全フレームを通して軌跡を描画
                    for (int i = 0; i < static_cast<int>(currentReplay.frames.size()); i += FRAME_STEP) {
                        const FrameData& frameData = currentReplay.frames[i];

                        // 直前に描画したゴーストと座標がほぼ同じならスキップ(立ち止まっている時のZファイティング/濃くなりすぎ防止)
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

                        // ゴースト用のTransformとMaterialを作成
                        Transform ghostTransform = playerPrim->GetTransform();
                        ghostTransform.translate = frameData.position;
                        ghostTransform.scale = frameData.scale;
                        ghostTransform.rotate = frameData.rotation;

                        Material ghostMaterial = playerPrim->GetMaterial();
                        // 記録されていた色（スプライト/モデルのカラー情報）を取り出し、アルファ値をかけて半透明にする
                        Vector4 ghostColor = frameData.color;
                        ghostColor.w *= GHOST_ALPHA; // 元のアルファ値に掛け算する
                        ghostMaterial.color = ghostColor;

                        // プレイヤーのPrimitiveを使って残像(ゴースト)を描画
                        playerPrim->DrawGhost(ghostTransform, ghostMaterial);

                        // 3D -> NDC Conversion for the current frame's position
                        Vector3 ndcCurr = TransformFunctions::Transform(frameData.position, viewProjectionMatrix);

                        ImVec2 pCurr;
                        bool isCurrVisible = false;

                        // Check if the point is in front of the camera
                        if (ndcCurr.z >= 0.0f && ndcCurr.z <= 1.0f) {
                            isCurrVisible = true;
                            float screenWidth = gameViewSize.x;
                            float screenHeight = gameViewSize.y;

                            pCurr = ImVec2(
                                gameViewPos.x + (ndcCurr.x + 1.0f) * 0.5f * screenWidth,
                                gameViewPos.y + (1.0f - ndcCurr.y) * 0.5f * screenHeight
                            );

                            // Draw a dot at the afterimage position
                            drawList->AddCircleFilled(pCurr, 4.0f, IM_COL32(255, 50, 50, 255));
                        }

                        // Draw orbital line between the current and previous afterimage
                        if (i >= FRAME_STEP) {
                            int prevIndex = i - FRAME_STEP;
                            if (prevIndex >= 0 && prevIndex < static_cast<int>(currentReplay.frames.size())) {
                                Vector3 ndcPrev = TransformFunctions::Transform(currentReplay.frames[prevIndex].position, viewProjectionMatrix);

                                // Draw only if both current and previous points are visible
                                if (isCurrVisible && ndcPrev.z >= 0.0f && ndcPrev.z <= 1.0f) {
                                    float screenWidth = gameViewSize.x;
                                    float screenHeight = gameViewSize.y;

                                    ImVec2 pPrev(
                                        gameViewPos.x + (ndcPrev.x + 1.0f) * 0.5f * screenWidth,
                                        gameViewPos.y + (1.0f - ndcPrev.y) * 0.5f * screenHeight
                                    );

                                    drawList->AddLine(pPrev, pCurr, IM_COL32(255, 200, 0, 255), 2.0f);
                                }
                            }
                        }
                    }

                    // クリップ矩形を解除
                    drawList->PopClipRect();
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
    if (EditorManager::IsShowEffects()) {
#endif
        particleCommon_->DrawAll(viewProjectionMatrix);
#ifdef USE_IMGUI
    }
#endif
}

std::vector<ParticleManager *> GameScene::GetParticles() {
    std::vector<ParticleManager *> result;
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

    if (skybox_) {
        skybox_->Update();
    }
    if (coinEffect_) {
        // ImGui更新（もしあれば）
        coinEffect_->Update(1.0f / 60.0f);
    }
    if (ringEffect_) {
        ringEffect_->Update(1.0f / 60.0f);
    }
    if (cylinderEffect_) {
        cylinderEffect_->Update(1.0f / 60.0f);
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
    if (map_) {
        map_->Update();
        for (auto* mapPrim : map_->GetPrimitiveObjects()) {
            if (mapPrim) {
                mapPrim->Update();
            }
        }
    }
}