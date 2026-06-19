#include "GameScene.h"
#include "Scene/SceneManager.h"
#include "Effect/SnowParticle.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelCommon.h"
#include "Graphics/GameCamera.h"
#include "Scene/SceneFactory.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "Editor/EditorManager.h"
#endif
#include "Editor/ReplayManager.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"

void GameScene::Initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
    commandList_ = commandList.Get();
    rotateTimer_ = 0.0f; // 回転タイマーを確実にリセット

    // 1. Device取得
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    commandList->GetDevice(IID_PPV_ARGS(&device));

    // 2. PrimitiveManagerの初期化（まだの場合）
    PrimitiveManager::GetInstance()->Initialize(device.Get());

    // リプレイ保存リストの読み込み
    ReplayManager::GetInstance()->LoadSavedList();

    // ★ Skyboxの初期化処理を追加
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("Sprite/Original/skybox/skybox_highres_build.dds", commandList);
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    // 3. SnowParticleの生成 (unique_ptrで作る)
    auto snowParticle = std::make_unique<SnowParticle>();
    snowParticle->Initialize(commandList.Get(), particleCommon_, 1000, "Sprite/School/circle.png", srvIndex_, BlendMode::kBlendModeAdd);
    snowParticle->SetName("Snow Particles");

    // Commonに描画登録する (Modelと同じ仕組みにする)
    particleCommon_->AddParticle(snowParticle.get());
    snowParticle_ = snowParticle.get();
    particles_.push_back(std::move(snowParticle));

    // HitEffectの作成（コイン取得用）
    hitEffect_ = std::make_unique<HitEffect>();
    hitEffect_->Initialize(commandList.Get(), particleCommon_, 1024, "Sprite/School/circle2.png", 112, kBlendModeAdd);

    // 4. 3Dプリミティブオブジェクトの作成
    // 橙色の球体（環境マップ・ライティング有効）
    {
        auto sphere = std::make_unique<PrimitiveObject>();
        sphere->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Sphere, 1.0f, 32));
        sphere->SetTranslation({2.0f, 0.0f, 0.0f});
        sphere->GetMaterial().color = {1.0f, 0.5f, 0.0f, 1.0f};
        sphere->GetMaterial().enableEnvironmentMap = 1;
        sphere->GetMaterial().environmentCoefficient = 0.5f;
        sphere->GetMaterial().lightingType = 1;
        sphere->SetName("Game Sphere");
        primitives_.push_back(std::move(sphere));
    }

    // 水色の箱（環境マップ・ライティング有効）
    {
        auto box = std::make_unique<PrimitiveObject>();
        box->Initialize(device.Get(), PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box, 1.0f));
        box->SetTranslation({-2.0f, 0.0f, 0.0f});
        box->GetMaterial().color = {0.0f, 0.8f, 1.0f, 1.0f};
        box->GetMaterial().enableEnvironmentMap = 1;
        box->GetMaterial().environmentCoefficient = 0.5f;
        box->GetMaterial().lightingType = 1;
        box->SetName("Game Box");
        primitives_.push_back(std::move(box));
    }

    // 5. マップの生成と初期化
    map_ = std::make_unique<MapChip2D>();
    map_->Initialize(commandList.Get());

    // 6. プレイヤーの生成と初期化
    player_ = std::make_unique<Player2D>();
    player_->Initialize(commandList.Get());
    player_->FindSpawnPoint(*map_);

    // 7. GameCameraを正射影モード（2D表示）に切り替え
    if (gameCamera_) {
        gameCamera_->InitializeOrthographic(1280, 720, 20.0f, 11.25f);
        // プレイヤーの位置をカメラ追従ターゲットに設定
        gameCamera_->SetFollowTarget(&player_->GetPosition());
    }
}

void GameScene::Update(SceneManager *sceneManager) {
    // 1. 雪を発生させる (個別のポインタを使う)
    if(snowParticle_) {
        snowParticle_->Emit(snowEmitter_);
    }

    // 2. 全パーティクルを更新する (リストを使って一括更新)
    for(auto &particle : particles_) {
        particle->Update();
    }
    if (hitEffect_) {
        hitEffect_->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    // 3. 3Dプリミティブオブジェクトの回転と更新
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    rotateTimer_ += deltaTime;
    if (primitives_.size() >= 2) {
        primitives_[0]->SetRotation({0.0f, rotateTimer_, 0.0f}); // 球体のY軸回転
        primitives_[1]->SetRotation({rotateTimer_ * 0.5f, rotateTimer_, 0.0f}); // 箱の多軸回転
    }

    for (auto &primitive : primitives_) {
        primitive->Update();
    }

    // 4. プレイヤーの更新（入力・物理・当たり判定）
    if (player_ && map_) {
        bool isCurrentlyPlaying = true;
#ifdef USE_IMGUI
        isCurrentlyPlaying = EditorManager::IsPlaying();
#endif

        static bool wasCurrentlyPlaying = false;
        if (isCurrentlyPlaying && !wasCurrentlyPlaying) {
            player_->FindSpawnPoint(*map_);
        }
        wasCurrentlyPlaying = isCurrentlyPlaying;

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
            static bool wasPlayingLastFrame = false;
            if (ReplayManager::GetInstance()->IsPlaying()) {
                bool shouldRebuildState = !wasPlayingLastFrame || ReplayManager::GetInstance()->IsForceSnapNextFrame();
                if (shouldRebuildState) {
                    auto& replayData = ReplayManager::GetInstance()->GetCurrentReplay();
                    int curFrame = ReplayManager::GetInstance()->GetCurrentFrame();

                    // 1. マップを初期状態（文字列）から復元
                    if (!replayData.mapDataStr.empty()) {
                        map_->LoadFromString(replayData.mapDataStr);
                    }
                    
                    // 2. プレイヤースコアをリセット
                    player_->SetScore(0);
                    
                    // 3. 0フレーム目から現在フレームまで、記録された座標をたどってコインを回収
                    for (int i = 0; i <= curFrame; ++i) {
                        player_->SetPosition(replayData.frames[i].position);
                        player_->SimulateCollisions(*map_);
                    }
                }

                if (!wasPlayingLastFrame) {
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
                    wasPlayingLastFrame = true;
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
            } else {
                if (wasPlayingLastFrame) {
                    // リプレイが終了した（またはTAKEOVERで停止した）瞬間に、カメラの追従を復元する
                    if (gameCamera_) {
                        gameCamera_->SetFollowTarget(&player_->GetPosition());
                    }
                }
                wasPlayingLastFrame = false;

                // 巻き戻しから通常に戻ったときにカメラ追従を再開する
            }

            player_->Update(*map_);

            // ゴール判定
            if (player_->IsGoalComplete()) {
                sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
                return;
            }

            // コイン獲得エフェクト
            int currentScore = player_->GetScore();
            if (currentScore > previousScore_) {
                if (hitEffect_) {
                    Emitter hitEmitter{};
                    hitEmitter.transform.translate = player_->GetPosition();
                    hitEmitter.transform.scale = { 0.75f, 0.75f, 0.75f };
                    hitEmitter.count = 20;
                    hitEmitter.frequency = 0.05f;
                    hitEmitter.frequencyTime = 0.0f;
                    hitEffect_->Emit(hitEmitter);
                }
                previousScore_ = currentScore;
            }
        }

        static bool wasRewindingLastFrame = false;
        if (!isRewinding && wasRewindingLastFrame) {
            if (gameCamera_) {
                gameCamera_->SetFollowTarget(&player_->GetPosition());
            }
        }
        wasRewindingLastFrame = isRewinding;

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

    // 5. マップの更新
    if (map_) {
        map_->Update();
    }
}

void GameScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    if (player_ && player_->GetPrimitiveObject() == selectedPrimitive) {
        player_->DisplayImGui();
    }

    // スコアの簡易表示
    if (player_) {
        ImGui::Begin("Game HUD", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(2.0f);
        ImGui::Text("Score: %d", player_->GetScore());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }
#endif
}

void GameScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // Skyboxの描画前にDescriptorHeapをセットさせるため、PreDrawを呼ぶ
    if (modelCommon_) {
        modelCommon_->PreDraw(commandList_);
    }

    if (skybox_) {
        skybox_->Draw(commandList_);
        
        auto dxCommon = DirectXCommon::GetInstance();
        commandList_->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        commandList_->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw(commandList_);
        }
    }

    // 1. 3Dプリミティブの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &primitive : primitives_) {
            primitive->Draw(commandList_);
        }
#ifdef USE_IMGUI
    }
#endif

    // 2. 2Dオブジェクト（マップ・プレイヤー）の描画
    // ModelCommonの描画前処理
    modelCommon_->PreDraw(commandList_);

    // マップの描画
    if (map_) {
        map_->Draw(commandList_);
    }

    // プレイヤーの描画
    if (player_) {
        player_->Draw(commandList_);
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
                        playerPrim->DrawGhost(commandList_, ghostTransform, ghostMaterial);
                    }
                }
            }
        }
    }
#endif

    // 3. パーティクルの描画
    // 描画前処理
    particleCommon_->PreDraw(commandList_);

    // 雪の描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects()) {
#endif
        particleCommon_->DrawAll(viewProjectionMatrix);
        if (hitEffect_) {
            hitEffect_->Draw(viewProjectionMatrix);
        }
#ifdef USE_IMGUI
    }
#endif
}

std::vector<ParticleManager *> GameScene::GetParticles() {
    std::vector<ParticleManager *> result;
    for (auto &p : particles_) {
        result.push_back(p.get());
    }
    if (hitEffect_) {
        result.push_back(hitEffect_.get());
    }
    return result;
}

std::vector<PrimitiveObject *> GameScene::GetPrimitives() {
    std::vector<PrimitiveObject *> result;

    // 1. 3Dプリミティブオブジェクト
    for (auto &p : primitives_) {
        result.push_back(p.get());
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

    return result;
}

void GameScene::UpdateEditor() {
    // 録画状態のままエディタが停止した場合、確実に停止させて履歴に保存する
    if (ReplayManager::GetInstance()->IsRecording()) {
        ReplayManager::GetInstance()->StopRecord();
    }

    for (auto &primitive : primitives_) {
        primitive->Update();
    }
    if (skybox_) {
        skybox_->Update();
    }
    if (hitEffect_) {
        hitEffect_->Update();
    }
    if (player_) {
        // エディタ停止中もマップの変更に追従してプレイヤー座標を更新
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
        for (auto* mapPrim : map_->GetPrimitiveObjects()) {
            if (mapPrim) {
                mapPrim->Update();
            }
        }
    }
}