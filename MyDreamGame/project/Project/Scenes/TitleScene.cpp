#include "TitleScene.h"
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
#include "Graphics/GameCamera.h"
// 配置モデル(レベルデータ)はエディター非搭載ビルドでも参照するためガード外に置く
#include "Editor/Model3DEditor/Model3DEditorContext.h"
#include "Editor/Model3DEditor/PlacedObject3D.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#include "Editor/Model3DEditor/Model3DEditor.h"
#endif

#include "Scene/SceneFactory.h"
#include "Resource/Audio/AudioManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/Renderer.h"
#include "Component/TransformComponent.h"
#include "Component/MeshRendererComponent.h"
#include "Component/PrimitiveRendererComponent.h"
#include "GameObject/Object3D.h"
#include "Scenes/GameScene.h"
#include <cmath>
#include <filesystem>

TitleScene::~TitleScene() {
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetDepthBasedOutlineEnabled(false);
    }
}

void TitleScene::OnEnter(SceneManager* sceneManager) {
    // シーン遷移時の開始処理
    isFirstFrame_ = true;
    titleTimer_ = 0.0f;
    stageSelectPulseTimer_ = 0.0f;
    isIrisOutActive_ = false;
    gameTransitionTimer_ = 0.0f;
    cardPhase_ = CardThrowPhase::kNone;
    cardTimer_ = 0.0f;
    cardShakeTimer_ = 0.0f;
    cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };

    // ゲームクリア等からステージ選択へ直接戻ってきたか判定
    bool startAtStageSelect = false;
    if (sceneManager && sceneManager->HasData("StartAtStageSelect")) {
        startAtStageSelect = sceneManager->GetData<bool>("StartAtStageSelect");
        sceneManager->SetData("StartAtStageSelect", false); // フラグを消費
    }

    if (startAtStageSelect) {
        // ステージ選択画面から直接開始（ビル群を見下ろすアングル）
        phase_ = Phase::kStageSelect;
        cameraTransform_.translate = targetSelectPos_;
        cameraTransform_.rotate = targetSelectRot_;
        titleLogoAlpha_ = 0.0f;
        titleMenuAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;

        // ステージクリア後の画面遷移：画面中央から円が開いてステージ選択画面が現れる！
        StartIrisIn({ 0.5f, 0.5f }, 0.7f);

        // セレクトモード時はTitle.mp3とSelect.mp3を同時に再生
        AudioManager::StopAllBGM();
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Title.mp3", true, 0.4f);
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Select.mp3", true, 0.4f);

    } else {
        // 通常のタイトル画面から開始（夜空を見上げるアングル）
        phase_ = Phase::kTitle;
        selectedStageIndex_ = 0;
        selectedTitleMenu_ = 0;
        cameraTransform_.translate = titleCameraPos_;
        cameraTransform_.rotate = titleCameraRot_;
        titleLogoAlpha_ = 1.0f;
        titleMenuAlpha_ = 1.0f;
        searchlightAlpha_ = 1.0f;
        creditAlpha_ = 0.0f;

        // タイトル画面時はTitle.mp3のみを再生
        AudioManager::StopAllBGM();
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Title.mp3", true, 0.4f);
    }

    // カメラをGameCameraおよびCameraManagerに即時反映
    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    CameraManager::GetInstance()->SetCameraInfo(cameraTransform_.translate, viewMatrix, projectionMatrix);

    if (gameCamera_) {
        gameCamera_->SetOrthographic(false);
        gameCamera_->SetFollowTarget(nullptr);
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }

#ifdef USE_IMGUI
    if (EditorManager::GetInstance()) {
        // ゲームプレイ中の遷移時のみ、デバッグカメラを解除してゲームビューに合わせる
        if (EditorManager::IsPlaying()) {
            EditorManager::GetInstance()->SetUseDebugCamera(false);
            EditorManager::GetInstance()->FocusGameView();
        }
    }
#endif
    // タイトル用3Dモデル(title_obj.json: ビル群・ステージ選択オブジェクト)は
    // SceneManager が GetLevelDataJsonPath() を見て読み込むため、ここでは何もしない

    if (callingCardObject_) {
        if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
            tc->SetScale({ 0.0f, 0.0f, 0.0f });
        }
    }

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        // 深度ベース・アウトライン（ポストエフェクトのシェーダーで輪郭を描くパス）はタイトルの標準の見た目。
        // USE_IMGUI が定義されないReleaseビルドでも確実に輪郭線が表示されるよう、シーン突入時に有効化する。
        dxCommon->SetDepthBasedOutlineEnabled(true);
        if (!isIrisInActive_) {
            dxCommon->SetCompositeIrisEnabled(false);
        }
    }

    if (titleLogoSprite_) {
        titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, titleLogoAlpha_ });
    }
    if (searchlightObjects_.size() >= 2) {
        if (auto prc1 = searchlightObjects_[0]->GetComponent<PrimitiveRendererComponent>()) {
            prc1->GetMaterial().color.w = 0.22f * searchlightAlpha_;
        }
        if (auto prc2 = searchlightObjects_[1]->GetComponent<PrimitiveRendererComponent>()) {
            prc2->GetMaterial().color.w = 0.18f * searchlightAlpha_;
        }
    }
}

void TitleScene::OnExit(SceneManager* sceneManager) {
    // シーン遷移時の終了処理
    isIrisOutActive_ = false;
    cardPhase_ = CardThrowPhase::kNone;
    cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetCompositeIrisEnabled(false);
        dxCommon->SetDepthBasedOutlineEnabled(false);
    }
}

void TitleScene::Initialize() {

    Microsoft::WRL::ComPtr<ID3D12Device> device;
    device = DirectXCommon::GetInstance()->GetDevice();

    cameraTransform_.translate = {0.0f, 0.0f, -10.0f};

    // -------------------------------------------------------------
    // 1. カメラ初期設定 (夜空を見上げるシネマティックアングル)
    // -------------------------------------------------------------
    cameraTransform_.translate = { 0.0f, 1.2f, -8.5f };
    cameraTransform_.rotate = { 0.06f, 0.0f, 0.0f };

    // -------------------------------------------------------------
    // 2. Skybox初期化 (qwantani_dusk)
    // -------------------------------------------------------------
    skyboxTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/temp_cube.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(device.Get(), skyboxTextureHandle_);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(skyboxTextureHandle_));

    // -------------------------------------------------------------
    // 3. サーチライト演出 (警察・警備員が夜空を走査する光の筋)
    // -------------------------------------------------------------
    searchlightObjects_.clear();
    Primitive* boxPrim = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Box);
    
    // サーチライト1 (黄色・左奥から右へスイング)
    {
        auto light1 = std::make_shared<GameObject>("Searchlight_Yellow");
        auto lt1 = light1->AddComponent<TransformComponent>();
        lt1->SetPosition({ -4.5f, 6.0f, 4.0f });
        lt1->SetScale({ 0.9f, 22.0f, 0.9f });
        lt1->SetRotation({ 0.0f, 0.0f, 0.35f });

        auto lr1 = light1->AddComponent<PrimitiveRendererComponent>();
        lr1->Initialize(device.Get(), boxPrim);
        lr1->GetMaterial().color = { 1.0f, 0.92f, 0.4f, 0.22f }; // 半透明の光線イエロー
        lr1->GetMaterial().lightingType = 0; // 自己発光

        gameObjects_.push_back(light1);
        searchlightObjects_.push_back(light1);
    }

    // サーチライト2 (シアンブルー・右奥から左へスイング)
    {
        auto light2 = std::make_shared<GameObject>("Searchlight_Cyan");
        auto lt2 = light2->AddComponent<TransformComponent>();
        lt2->SetPosition({ 4.0f, 6.5f, 6.0f });
        lt2->SetScale({ 0.8f, 24.0f, 0.8f });
        lt2->SetRotation({ 0.0f, 0.0f, -0.4f });

        auto lr2 = light2->AddComponent<PrimitiveRendererComponent>();
        lr2->Initialize(device.Get(), boxPrim);
        lr2->GetMaterial().color = { 0.3f, 0.85f, 1.0f, 0.18f }; // 半透明のサイバーシアン
        lr2->GetMaterial().lightingType = 0; // 自己発光

        gameObjects_.push_back(light2);
        searchlightObjects_.push_back(light2);
    }

    // -------------------------------------------------------------
    // 6. タイトルロゴ スプライト (title.png) - 画面中央に配置
    // -------------------------------------------------------------
    titleLogoTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/title.png");
    titleLogoSprite_ = std::make_unique<Sprite>();
    titleLogoSprite_->Initialize(spriteCommon_, titleLogoTextureHandle_);
    
    // 画面解像度 1280x720 の中央上部に配置 (幅600, 高さ266, X=(1280-600)/2=340)
    const float logoW = 600.0f;
    const float logoH = 266.0f;
    titleLogoSprite_->SetSize({ logoW, logoH });
    titleLogoSprite_->SetPosition({ (1280.0f - logoW) * 0.5f, 70.0f });

    // -------------------------------------------------------------
    // 6.2 スタートテキスト スプライト (startText.png)
    // -------------------------------------------------------------
    startTextTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/startText.png");
    // ステージ選択の見出し。位置と大きさは描く直前に決める
    stageSelectTitleTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/stage_select.png");
    stageSelectTitleSprite_ = std::make_unique<Sprite>();
    stageSelectTitleSprite_->Initialize(spriteCommon_, stageSelectTitleTextureHandle_);

    // 決定の操作案内（右下）。位置と大きさは描く直前に決める
    padPromptTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/A_select.png");
    padPromptSprite_ = std::make_unique<Sprite>();
    padPromptSprite_->Initialize(spriteCommon_, padPromptTextureHandle_);
    keyPromptTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/space_select.png");
    keyPromptSprite_ = std::make_unique<Sprite>();
    keyPromptSprite_->Initialize(spriteCommon_, keyPromptTextureHandle_);

    startTextSprite_ = std::make_unique<Sprite>();
    startTextSprite_->Initialize(spriteCommon_, startTextTextureHandle_);
    startTextSprite_->SetSize(startTextSize_);
    startTextSprite_->SetPosition(startTextPos_);

    // -------------------------------------------------------------
    // 6.3 クレジットテキスト スプライト (creditText.png)
    // -------------------------------------------------------------
    creditTextTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/creditText.png");
    creditTextSprite_ = std::make_unique<Sprite>();
    creditTextSprite_->Initialize(spriteCommon_, creditTextTextureHandle_);
    creditTextSprite_->SetSize(creditTextSize_);
    creditTextSprite_->SetPosition(creditTextPos_);

    // -------------------------------------------------------------
    // 6.4 クレジット画面表示スプライト (credit.png)
    // -------------------------------------------------------------
    creditTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/credit.png");
    creditSprite_ = std::make_unique<Sprite>();
    creditSprite_->Initialize(spriteCommon_, creditTextureHandle_);
    creditSprite_->SetSize(creditSize_);
    creditSprite_->SetPosition(creditPos_);
    creditSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });

    // -------------------------------------------------------------
    // 7. 予告状オブジェクト (callingCard.obj) の準備
    // -------------------------------------------------------------
    Model* cardModel = ModelManager::GetInstance()->GetModel("resources/Object/Original/callingCard", "callingCard.obj");
    callingCardObject_ = std::make_shared<GameObject>("CallingCard");
    auto tc = callingCardObject_->AddComponent<TransformComponent>();
    tc->SetPosition({ 0.0f, -100.0f, 0.0f });
    tc->SetScale({ 0.0f, 0.0f, 0.0f }); // 初期は非表示
    tc->SetRotation({ 0.0f, 0.0f, 0.0f });

    auto rc = callingCardObject_->AddComponent<MeshRendererComponent>();
    rc->Initialize(device.Get(), cardModel);
    rc->GetMaterial().color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 純白の予告状カード
    rc->GetMaterial().lightingType = 0; // 自己発光でハッキリ見せる

    gameObjects_.push_back(callingCardObject_);
}

void TitleScene::Update(SceneManager *sceneManager) {
    float dt = TimeManager::GetInstance().GetDeltaTime();
    titleTimer_ += dt;

    auto kb = KeyboardInput::GetInstance();
    auto pad = GamepadInput::GetInstance();
    bool isDecisionPressed = kb->IsKeyPressed(DIK_SPACE) || 
                             kb->IsKeyPressed(DIK_RETURN) || 
                             (pad && pad->IsButtonPressed(0)); // Aボタン

    // ステージ選択に入った瞬間（カメラが着いた瞬間）に見出しの演出を始める。
    // 入口が複数あるので、フェーズが変わったことで拾う
    if (phase_ == Phase::kStageSelect && prevPhase_ != Phase::kStageSelect) {
        stageSelectIntroTimer_ = 0.0f;
    } else if (phase_ != Phase::kStageSelect) {
        stageSelectIntroTimer_ = -1.0f; // 抜けたら次に入った時にまた最初から
    }
    if (stageSelectIntroTimer_ >= 0.0f) {
        stageSelectIntroTimer_ += dt;
    }
    prevPhase_ = phase_;

    // 右下の操作案内を、直前に触った方に合わせる（パッドを触れば A、キーを触れば SPACE）
    if (pad && pad->IsConnected()) {
        const Vector2 stick = pad->GetLeftStick();
        bool padTouched = pad->IsDPadUp() || pad->IsDPadDown() || pad->IsDPadLeft() || pad->IsDPadRight() ||
                          std::abs(stick.x) > 0.3f || std::abs(stick.y) > 0.3f;
        for (int b = 0; b < 12 && !padTouched; ++b) {
            if (pad->IsButtonDown(b)) {
                padTouched = true;
            }
        }
        if (padTouched) {
            usePadPrompt_ = true;
        }
    }
    if (kb && (kb->IsKeyPressed(DIK_SPACE) || kb->IsKeyPressed(DIK_RETURN) ||
               kb->IsKeyPressed(DIK_W) || kb->IsKeyPressed(DIK_S) ||
               kb->IsKeyPressed(DIK_UP) || kb->IsKeyPressed(DIK_DOWN))) {
        usePadPrompt_ = false;
    }

    // シーン遷移直後の同一フレームでの入力誤爆防止
    if (isFirstFrame_) {
        isFirstFrame_ = false;
    } else {
        if (phase_ == Phase::kTitle) {
            // メニュー項目の上下選択 (W/S, ↑/↓)
            // ※ ゲームパッドのPOVハットスイッチは未入力時に0を返す環境があり、常時上入力と誤判定されて
            //    勝手に選択が切り替わり続ける原因となっていたため（ポーズメニュー時と同様）、
            //    キーボードの確実な押下 (IsKeyPressed) で制御し、直接インデックスを指定します。
            int prevMenu = selectedTitleMenu_;
            if (kb->IsKeyPressed(DIK_UP) || kb->IsKeyPressed(DIK_W)) {
                selectedTitleMenu_ = 0; // 上: スタート
            } else if (kb->IsKeyPressed(DIK_DOWN) || kb->IsKeyPressed(DIK_S)) {
                selectedTitleMenu_ = 1; // 下: クレジット
            }
            if (prevMenu != selectedTitleMenu_) {
                AudioManager::Play("resources/Sound/10Dyas/SE/SelectMove.mp3", 0.7f);
            }

            // タイトル画面で決定ボタン押下時
            if (isDecisionPressed) {
                if (selectedTitleMenu_ == 0) {
                    AudioManager::Play("resources/Sound/10Dyas/SE/TitleCameraMove.mp3", 0.7f);

                    // 「スタート」選択時: ステージ選択カメラへの移動フェーズを開始
                    phase_ = Phase::kTransitionToSelect;
                    transitionStartPos_ = cameraTransform_.translate;
                    transitionStartRot_ = cameraTransform_.rotate;
                    transitionTimer_ = 0.0f;

                    // セレクトモード開始: Title.mp3 は流したまま、Select.mp3 を同時に重ねて再生
                    AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Select.mp3", true, 0.4f);
                } else if (selectedTitleMenu_ == 1) {
                    AudioManager::Play("resources/Sound/10Dyas/SE/TitleCameraMove.mp3", 0.7f);

                    // 「クレジット」選択時: クレジットモデル表示カメラへの移動フェーズを開始
                    phase_ = Phase::kTransitionToCredit;
                    transitionStartPos_ = cameraTransform_.translate;
                    transitionStartRot_ = cameraTransform_.rotate;
                    transitionTimer_ = 0.0f;
                    creditAnimTimer_ = 0.0f;
                    creditAlpha_ = 0.0f;
                    creditScale_ = 0.85f;
                }
            }
        } else if (phase_ == Phase::kCredit) {
            // クレジット画面で決定またはキャンセルボタン押下時: タイトル画面へ復帰
            bool isCancelPressed = kb->IsKeyPressed(DIK_ESCAPE) || 
                                   kb->IsKeyPressed(DIK_BACK) || 
                                   (pad && pad->IsButtonPressed(GamepadButton::B));
            if (isDecisionPressed || isCancelPressed) {
                AudioManager::Play("resources/Sound/10Dyas/SE/TitleCameraMove.mp3", 0.7f);
                phase_ = Phase::kTransitionFromCredit;
                transitionStartPos_ = cameraTransform_.translate;
                transitionStartRot_ = cameraTransform_.rotate;
                transitionTimer_ = 0.0f;
                // creditAlpha_ は kTransitionFromCredit の最初の0.15秒で縮小フェードアウト
            }
        } else if (phase_ == Phase::kStageSelect) {
            // ステージ選択画面で決定ボタン押下時: 選択中ステージオブジェクトへ向けて予告状突き刺し演出を開始
            if (isDecisionPressed && cardPhase_ == CardThrowPhase::kNone) {
                AudioManager::Play("resources/Sound/10Dyas/SE/Select.mp3", 0.8f);
                phase_ = Phase::kTransitionToGame;

                // 選択中のオブジェクト（select_1, select_2, select_3）のワールド座標を取得
                Vector3 targetWorldPos = { -18.5f, -8.8f, 22.94f }; // デフォルト: select_1 の位置
                {
                    auto context = Model3DEditorContext::GetInstance();
                    std::string targetName = "select_" + std::to_string(selectedStageIndex_ + 1);
                    for (const auto& obj : context->GetObjects()) {
                        if (obj && obj->GetName() == targetName) {
                            targetWorldPos = obj->GetTranslation();
                            break;
                        }
                    }
                }
                StartCallingCardThrow(targetWorldPos);
            }
        }
    }

    // -------------------------------------------------------------
    // カメラの更新とフェーズ別補間
    // -------------------------------------------------------------
    Vector3 camPos = cameraTransform_.translate;
    Vector3 camRot = cameraTransform_.rotate;

    if (phase_ == Phase::kTitle) {
        // タイトル画面: シネマティック揺れ演出
        if (enableCinematicSway_) {
            camPos.x += sinf(titleTimer_ * 0.4f) * 0.25f;
            camPos.y += sinf(titleTimer_ * 0.7f) * 0.12f;
            camPos.z += cosf(titleTimer_ * 0.3f) * 0.15f;
            camRot.x += sinf(titleTimer_ * 0.5f) * 0.015f;
            camRot.y += sinf(titleTimer_ * 0.3f) * 0.02f;
        }
        titleLogoAlpha_ = 1.0f;
        titleMenuAlpha_ = 1.0f;
        searchlightAlpha_ = 1.0f;
    } else if (phase_ == Phase::kTransitionToSelect) {
        // ステージ選択位置へ滑らかにカメラを補間移動 (Smoothstep)
        transitionTimer_ += dt;
        float t = std::clamp(transitionTimer_ / transitionDuration_, 0.0f, 1.0f);
        float ease = t * t * (3.0f - 2.0f * t); // Smoothstep

        camPos = {
            transitionStartPos_.x + (targetSelectPos_.x - transitionStartPos_.x) * ease,
            transitionStartPos_.y + (targetSelectPos_.y - transitionStartPos_.y) * ease,
            transitionStartPos_.z + (targetSelectPos_.z - transitionStartPos_.z) * ease
        };
        camRot = {
            transitionStartRot_.x + (targetSelectRot_.x - transitionStartRot_.x) * ease,
            transitionStartRot_.y + (targetSelectRot_.y - transitionStartRot_.y) * ease,
            transitionStartRot_.z + (targetSelectRot_.z - transitionStartRot_.z) * ease
        };

        cameraTransform_.translate = camPos;
        cameraTransform_.rotate = camRot;

        // タイトルロゴ、メニューテキスト、サーチライトを素早く自然にフェードアウト (logoFadeDuration_ 秒で完了)
        float fadeT = std::clamp(transitionTimer_ / logoFadeDuration_, 0.0f, 1.0f);
        float remain = 1.0f - fadeT;
        float fadeAlpha = remain * remain; // スッと自然に消える2乗減衰
        if (fadeAlpha < 0.001f) {
            fadeAlpha = 0.0f;
        }
        titleLogoAlpha_ = fadeAlpha;
        titleMenuAlpha_ = fadeAlpha;
        searchlightAlpha_ = fadeAlpha;

        if (t >= 1.0f) {
            phase_ = Phase::kStageSelect;
            cameraTransform_.translate = targetSelectPos_;
            cameraTransform_.rotate = targetSelectRot_;
            camPos = targetSelectPos_;
            camRot = targetSelectRot_;
            titleLogoAlpha_ = 0.0f;
            titleMenuAlpha_ = 0.0f;
            searchlightAlpha_ = 0.0f;
        }
    } else if (phase_ == Phase::kStageSelect || phase_ == Phase::kTransitionToGame) {
        // ステージ選択画面 / ゲーム移行中: 目標位置に固定
        camPos = targetSelectPos_;
        camRot = targetSelectRot_;
        cameraTransform_.translate = targetSelectPos_;
        cameraTransform_.rotate = targetSelectRot_;
        titleLogoAlpha_ = 0.0f;
        titleMenuAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;
    } else if (phase_ == Phase::kTransitionToCredit) {
        // クレジット画面へ滑らかにカメラを補間移動 (Smoothstep)
        // ※ ユーザー要望により移動時間を短縮 (creditTransitionDuration_)
        transitionTimer_ += dt;
        float t = std::clamp(transitionTimer_ / creditTransitionDuration_, 0.0f, 1.0f);
        float ease = t * t * (3.0f - 2.0f * t); // Smoothstep

        camPos = {
            transitionStartPos_.x + (targetCreditPos_.x - transitionStartPos_.x) * ease,
            transitionStartPos_.y + (targetCreditPos_.y - transitionStartPos_.y) * ease,
            transitionStartPos_.z + (targetCreditPos_.z - transitionStartPos_.z) * ease
        };
        camRot = {
            transitionStartRot_.x + (targetCreditRot_.x - transitionStartRot_.x) * ease,
            transitionStartRot_.y + (targetCreditRot_.y - transitionStartRot_.y) * ease,
            transitionStartRot_.z + (targetCreditRot_.z - transitionStartRot_.z) * ease
        };

        cameraTransform_.translate = camPos;
        cameraTransform_.rotate = camRot;

        // タイトルロゴ、メニューテキスト、サーチライトを素早く自然にフェードアウト
        float fadeT = std::clamp(transitionTimer_ / logoFadeDuration_, 0.0f, 1.0f);
        float remain = 1.0f - fadeT;
        float fadeAlpha = remain * remain;
        if (fadeAlpha < 0.001f) {
            fadeAlpha = 0.0f;
        }
        titleLogoAlpha_ = fadeAlpha;
        titleMenuAlpha_ = fadeAlpha;
        searchlightAlpha_ = fadeAlpha;
        creditAlpha_ = 0.0f;

        if (t >= 1.0f) {
            phase_ = Phase::kCredit;
            cameraTransform_.translate = targetCreditPos_;
            cameraTransform_.rotate = targetCreditRot_;
            camPos = targetCreditPos_;
            camRot = targetCreditRot_;
            titleLogoAlpha_ = 0.0f;
            titleMenuAlpha_ = 0.0f;
            searchlightAlpha_ = 0.0f;
            creditAnimTimer_ = 0.0f; // スプライト出現演出開始
        }
    } else if (phase_ == Phase::kCredit) {
        // クレジット画面: クレジットモデル正面の目標位置に固定
        camPos = targetCreditPos_;
        camRot = targetCreditRot_;
        cameraTransform_.translate = targetCreditPos_;
        cameraTransform_.rotate = targetCreditRot_;
        titleLogoAlpha_ = 0.0f;
        titleMenuAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;

        // クレジットスプライト演出 (出現ポップイン & 呼吸アニメーション)
        creditAnimTimer_ += dt;
        const float appearDuration = 0.35f;
        if (creditAnimTimer_ < appearDuration) {
            // 出現演出: スケール0.85 -> 1.03 -> 1.0 のポップイン & フェードイン (EaseOutBack)
            float progress = creditAnimTimer_ / appearDuration;
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            float p = progress - 1.0f;
            float easeBack = 1.0f + c3 * p * p * p + c1 * p * p;
            creditScale_ = 0.85f + 0.15f * easeBack;
            creditAlpha_ = std::clamp(creditAnimTimer_ / 0.22f, 0.0f, 1.0f);
        } else {
            // 出現完了後は等倍・完全不透明で固定
            creditScale_ = 1.0f;
            creditAlpha_ = 1.0f;
        }
    } else if (phase_ == Phase::kTransitionFromCredit) {
        // クレジットスプライトの退場演出: スペース等でタイトル復帰時、0.15秒でキュッと縮んでフェードアウト
        const float fadeOutDuration = 0.15f;
        if (transitionTimer_ < fadeOutDuration) {
            float ft = transitionTimer_ / fadeOutDuration;
            creditAlpha_ = (1.0f - ft) * (1.0f - ft);
            creditScale_ = 1.0f - 0.15f * ft;
        } else {
            creditAlpha_ = 0.0f;
            creditScale_ = 0.85f;
        }

        // クレジット画面からタイトル画面へ滑らかにカメラを復帰 (Smoothstep)
        // ※ ユーザー要望により移動時間を短縮 (creditTransitionDuration_)
        transitionTimer_ += dt;
        float t = std::clamp(transitionTimer_ / creditTransitionDuration_, 0.0f, 1.0f);
        float ease = t * t * (3.0f - 2.0f * t); // Smoothstep

        camPos = {
            transitionStartPos_.x + (titleCameraPos_.x - transitionStartPos_.x) * ease,
            transitionStartPos_.y + (titleCameraPos_.y - transitionStartPos_.y) * ease,
            transitionStartPos_.z + (titleCameraPos_.z - transitionStartPos_.z) * ease
        };
        camRot = {
            transitionStartRot_.x + (titleCameraRot_.x - transitionStartRot_.x) * ease,
            transitionStartRot_.y + (titleCameraRot_.y - transitionStartRot_.y) * ease,
            transitionStartRot_.z + (titleCameraRot_.z - transitionStartRot_.z) * ease
        };

        cameraTransform_.translate = camPos;
        cameraTransform_.rotate = camRot;

        // タイトル画面復帰直前にUIをフェードイン (後半 logoFadeDuration_ 秒でフェードイン)
        float remainingTime = creditTransitionDuration_ - transitionTimer_;
        if (remainingTime <= logoFadeDuration_) {
            float fadeT = 1.0f - std::clamp(remainingTime / logoFadeDuration_, 0.0f, 1.0f);
            float inAlpha = fadeT * fadeT;
            titleLogoAlpha_ = inAlpha;
            titleMenuAlpha_ = inAlpha;
            searchlightAlpha_ = inAlpha;
        } else {
            titleLogoAlpha_ = 0.0f;
            titleMenuAlpha_ = 0.0f;
            searchlightAlpha_ = 0.0f;
        }

        if (t >= 1.0f) {
            phase_ = Phase::kTitle;
            cameraTransform_.translate = titleCameraPos_;
            cameraTransform_.rotate = titleCameraRot_;
            camPos = titleCameraPos_;
            camRot = titleCameraRot_;
            titleLogoAlpha_ = 1.0f;
            titleMenuAlpha_ = 1.0f;
            searchlightAlpha_ = 1.0f;
        }
    }

    // タイトルメニューテキストのビジュアル更新（完全不透明、選択中の拡大・ゴールド強調）
    titleMenuPulseTimer_ += dt;

    if (startTextSprite_) {
        if (selectedTitleMenu_ == 0) {
            // 選択中: 鮮やかなゴールド、少し拡大 (1.08倍)
            const float scale = 1.08f;
            Vector2 sz = { startTextSize_.x * scale, startTextSize_.y * scale };
            Vector2 pos = { startTextPos_.x - (sz.x - startTextSize_.x) * 0.5f, startTextPos_.y - (sz.y - startTextSize_.y) * 0.5f };
            startTextSprite_->SetSize(sz);
            startTextSprite_->SetPosition(pos);
            startTextSprite_->SetColor({ 1.0f, 0.92f, 0.35f, titleMenuAlpha_ }); // 完全不透明ゴールド
        } else {
            // 非選択時: 通常サイズ、完全不透明な白（背景のビルが透けない）
            startTextSprite_->SetSize(startTextSize_);
            startTextSprite_->SetPosition(startTextPos_);
            startTextSprite_->SetColor({ 0.85f, 0.85f, 0.85f, titleMenuAlpha_ }); // 完全不透明ホワイト
        }
    }

    if (creditTextSprite_) {
        if (selectedTitleMenu_ == 1) {
            // 選択中: 鮮やかなゴールド、少し拡大 (1.08倍)
            const float scale = 1.08f;
            Vector2 sz = { creditTextSize_.x * scale, creditTextSize_.y * scale };
            Vector2 pos = { creditTextPos_.x - (sz.x - creditTextSize_.x) * 0.5f, creditTextPos_.y - (sz.y - creditTextSize_.y) * 0.5f };
            creditTextSprite_->SetSize(sz);
            creditTextSprite_->SetPosition(pos);
            creditTextSprite_->SetColor({ 1.0f, 0.92f, 0.35f, titleMenuAlpha_ }); // 完全不透明ゴールド
        } else {
            // 非選択時: 通常サイズ、完全不透明な白（背景のビルが透けない）
            creditTextSprite_->SetSize(creditTextSize_);
            creditTextSprite_->SetPosition(creditTextPos_);
            creditTextSprite_->SetColor({ 0.85f, 0.85f, 0.85f, titleMenuAlpha_ }); // 完全不透明ホワイト
        }
    }

    // クレジット表示スプライト (credit.png) の更新 (スケール演出を反映)
    if (creditSprite_) {
        Vector2 scaledSize = { creditSize_.x * creditScale_, creditSize_.y * creditScale_ };
        Vector2 centeredPos = {
            creditPos_.x - (scaledSize.x - creditSize_.x) * 0.5f,
            creditPos_.y - (scaledSize.y - creditSize_.y) * 0.5f
        };

        creditSprite_->SetSize(scaledSize);
        creditSprite_->SetPosition(centeredPos);
        creditSprite_->SetColor({ 1.0f, 1.0f, 1.0f, creditAlpha_ });
        creditSprite_->Update();
    }

    // カメラシェイク (着弾時の微小振動) の反映
    camPos.x += cameraShakeOffset_.x;
    camPos.y += cameraShakeOffset_.y;
    camPos.z += cameraShakeOffset_.z;

    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(camRot, camPos);
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    CameraManager::GetInstance()->SetCameraInfo(camPos, viewMatrix, projectionMatrix);

    if (gameCamera_) {
        gameCamera_->SetOrthographic(false);
        gameCamera_->SetFollowTarget(nullptr);
        gameCamera_->SetTranslation(camPos);
        gameCamera_->SetRotation(camRot);
        gameCamera_->UpdateMatrix();
    }

    // -------------------------------------------------------------
    // タイトルロゴの浮遊アニメーション (フワフワと微小に上下)
    // -------------------------------------------------------------
    if (titleLogoSprite_) {
        float floatOffsetY = sinf(titleTimer_ * 1.6f) * 8.0f;
        const float logoW = 600.0f;
        titleLogoSprite_->SetPosition({ (1280.0f - logoW) * 0.5f, 70.0f + floatOffsetY });
        titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, titleLogoAlpha_ });
        titleLogoSprite_->Update();
    }

    // -------------------------------------------------------------
    // サーチライトの首振り・スイングアニメーション & 移行時消去
    // -------------------------------------------------------------
    if (searchlightObjects_.size() >= 2) {
        // サーチライト1 (黄色): 左右にゆっくり首振り
        if (auto tc1 = searchlightObjects_[0]->GetComponent<TransformComponent>()) {
            float rotZ1 = sinf(titleTimer_ * 0.85f) * 0.35f + 0.18f;
            float posX1 = -4.5f + sinf(titleTimer_ * 0.85f) * 0.5f;
            tc1->SetPosition({ posX1, 6.0f, 4.0f });
            tc1->SetRotation({ 0.0f, 0.0f, rotZ1 });
        }
        if (auto prc1 = searchlightObjects_[0]->GetComponent<PrimitiveRendererComponent>()) {
            prc1->GetMaterial().color.w = 0.22f * searchlightAlpha_;
        }

        // サーチライト2 (シアン): 逆位相で首振り
        if (auto tc2 = searchlightObjects_[1]->GetComponent<TransformComponent>()) {
            float rotZ2 = sinf(titleTimer_ * 0.65f + 2.0f) * 0.4f - 0.22f;
            float posX2 = 4.0f + cosf(titleTimer_ * 0.65f + 2.0f) * 0.6f;
            tc2->SetPosition({ posX2, 6.5f, 6.0f });
            tc2->SetRotation({ 0.0f, 0.0f, rotZ2 });
        }
        if (auto prc2 = searchlightObjects_[1]->GetComponent<PrimitiveRendererComponent>()) {
            prc2->GetMaterial().color.w = 0.18f * searchlightAlpha_;
        }
    }

    // 全オブジェクトの更新
    for (auto &object : gameObjects_) {
        object->Update();
    }

    for (auto &sprite : sprites_) {
        sprite->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    // ステージ選択用オブジェクトの選択状態・色更新
    UpdateStageSelectInteraction(dt);

    // 予告状突き刺し＆ゲームシーン移行演出を更新
    if (phase_ == Phase::kTransitionToGame) {
        UpdateCallingCardThrow(dt, sceneManager);
    }

    // ステージクリアから復帰時のアイリスイン（円が開く）演出を更新
    UpdateIrisIn(dt);
}

void TitleScene::Draw(const Matrix4x4 &viewProjectionMatrix) {
    // 1. モデル描画の前準備
    if (modelCommon_) {
        modelCommon_->PreDraw();
    }

    // 2. Skyboxを描画
    if (skybox_) {
        skybox_->Draw();
        
        // Skybox描画後はPSOが切り替わるため、再度モデル用の設定を呼び出す
        auto dxCommon = DirectXCommon::GetInstance();
        DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(dxCommon->GetRootSignature());
        DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(dxCommon->GetGraphicsPipelineState());

        if (modelCommon_) {
            modelCommon_->PreDraw();
        }
    }

    // 3. 3Dモデル・GameObjectの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowObjects()) {
#endif
        for (auto &object : gameObjects_) {
            object->Draw();
        }
#ifdef USE_IMGUI
    }
#endif

    // コンポーネントの描画を実行
    Renderer::GetInstance()->RenderComponents();

    // 4. パーティクルの描画
#ifdef USE_IMGUI
    if (EditorManager::IsShowEffects()) {
#endif
        if (particleCommon_) {
            particleCommon_->PreDraw();
            particleCommon_->DrawAll(viewProjectionMatrix);
        }
#ifdef USE_IMGUI
    }
#endif
}

void TitleScene::Draw2D() {
    // 2Dスプライト（タイトルロゴ・メニューテキスト）の最前面描画
    // ※ 3Dモデル配置（Placed Models）やパーティクルよりも後に描画されるため、ビル群等に隠れません
    if (spriteCommon_) {
        spriteCommon_->PreDraw();
        if (titleLogoSprite_ && titleLogoAlpha_ > 0.001f) {
            titleLogoSprite_->Draw();
        }
        if (titleMenuAlpha_ > 0.001f) {
            if (startTextSprite_) {
                startTextSprite_->Draw();
            }
            if (creditTextSprite_) {
                creditTextSprite_->Draw();
            }
        }
        if (creditSprite_ && creditAlpha_ > 0.001f) {
            creditSprite_->Draw();
        }
        for (auto &sprite : sprites_) {
            sprite->Draw();
        }

        // ステージ選択の見出し。画面の外から一気に入ってきて、少し行き過ぎてから止まる
        if (phase_ == Phase::kStageSelect && stageSelectIntroTimer_ >= 0.0f && stageSelectTitleSprite_) {
            const float w = stageSelectTitleHeight_ * (500.0f / 100.0f);
            const float t = (stageSelectIntroDuration_ > 0.001f)
                                ? (std::min)(stageSelectIntroTimer_ / stageSelectIntroDuration_, 1.0f)
                                : 1.0f;
            // イーズアウトバック：終わり際に少し行き過ぎてから戻る（引っ張られて止まる感じ）
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            const float u = t - 1.0f;
            const float eased = 1.0f + c3 * u * u * u + c1 * u * u;

            const float startX = -w - 40.0f; // 画面の外（左）から
            const float x = startX + (stageSelectTitlePos_.x - startX) * eased;
            const float alpha = (std::min)(1.0f, t * 3.0f); // 出だしだけさっと濃くなる

            stageSelectTitleSprite_->SetSize({w, stageSelectTitleHeight_});
            stageSelectTitleSprite_->SetPosition({x, stageSelectTitlePos_.y});
            stageSelectTitleSprite_->SetColor({1.0f, 1.0f, 1.0f, alpha});
            stageSelectTitleSprite_->Update();
            stageSelectTitleSprite_->Draw();
        }

        // 決定の操作案内（右下）。カメラが動いている間は出さず、止まったらまた出す
        const bool cameraMoving = (phase_ == Phase::kTransitionToSelect ||
                                   phase_ == Phase::kTransitionToGame ||
                                   phase_ == Phase::kTransitionToCredit ||
                                   phase_ == Phase::kTransitionFromCredit);
        if (!cameraMoving) {
            Sprite *prompt = usePadPrompt_ ? padPromptSprite_.get() : keyPromptSprite_.get();
            if (prompt) {
                // 画像は A が 300x100、SPACE が 500x100。高さをそろえて幅を比率から出す
                const float aspect = usePadPrompt_ ? (300.0f / 100.0f) : (500.0f / 100.0f);
                const float w = promptHeight_ * aspect;
                prompt->SetSize({w, promptHeight_});
                prompt->SetPosition({1280.0f - promptMargin_ - w, 720.0f - promptMargin_ - promptHeight_});
                prompt->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
                prompt->Update();
                prompt->Draw();
            }
        }
    }
}

std::vector<Object3D *> TitleScene::GetObjects() {
    return {};
}

std::vector<ParticleManager *> TitleScene::GetParticles() {
    std::vector<ParticleManager *> result;
    for (auto &p : particles_) {
        result.push_back(p.get());
    }
    return result;
}

std::vector<PrimitiveObject *> TitleScene::GetPrimitives() {
    return {};
}

void TitleScene::UpdateEditor() {
    // エディタ停止中もカメラの位置・角度をゲームカメラおよびCameraManagerに反映
    Vector3 camPos = cameraTransform_.translate;
    Vector3 camRot = cameraTransform_.rotate;

    Matrix4x4 viewMatrix = TransformFunctions::MakeViewMatrix(camRot, camPos);
    Matrix4x4 projectionMatrix = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    CameraManager::GetInstance()->SetCameraInfo(camPos, viewMatrix, projectionMatrix);

    if (gameCamera_) {
        gameCamera_->SetOrthographic(false);
        gameCamera_->SetFollowTarget(nullptr);
        gameCamera_->SetTranslation(camPos);
        gameCamera_->SetRotation(camRot);
        gameCamera_->UpdateMatrix();
    }

    for (auto &object : gameObjects_) {
        object->Update();
    }
    if (searchlightObjects_.size() >= 2) {
        if (auto prc1 = searchlightObjects_[0]->GetComponent<PrimitiveRendererComponent>()) {
            prc1->GetMaterial().color.w = 0.22f * searchlightAlpha_;
        }
        if (auto prc2 = searchlightObjects_[1]->GetComponent<PrimitiveRendererComponent>()) {
            prc2->GetMaterial().color.w = 0.18f * searchlightAlpha_;
        }
    }
    if (titleLogoSprite_) {
        titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, titleLogoAlpha_ });
        titleLogoSprite_->Update();
    }
    if (creditSprite_) {
        Vector2 scaledSize = { creditSize_.x * creditScale_, creditSize_.y * creditScale_ };
        Vector2 centeredPos = {
            creditPos_.x - (scaledSize.x - creditSize_.x) * 0.5f,
            creditPos_.y - (scaledSize.y - creditSize_.y) * 0.5f
        };
        creditSprite_->SetSize(scaledSize);
        creditSprite_->SetPosition(centeredPos);
        creditSprite_->SetColor({ 1.0f, 1.0f, 1.0f, creditAlpha_ });
        creditSprite_->Update();
    }
    for (auto &sprite : sprites_) {
        sprite->Update();
    }
    if (skybox_) {
        skybox_->Update();
    }

    // エディタ停止中もステージ選択用オブジェクトの色更新を反映
    UpdateStageSelectInteraction(TimeManager::GetInstance().GetDeltaTime());
}

void TitleScene::UpdateStageSelectInteraction(float dt) {
    stageSelectPulseTimer_ += dt;

    // 配置モデルはエディター非搭載ビルドでも共有コンテキストから参照できる
    auto context = Model3DEditorContext::GetInstance();

    // ステージ選択フェーズ中のみ、キーボード・パッドで選択インデックスを切り替える
    if (phase_ == Phase::kStageSelect) {
        auto kb = KeyboardInput::GetInstance();
        auto pad = GamepadInput::GetInstance();

        static float s_padCooldown = 0.0f;
        if (s_padCooldown > 0.0f) {
            s_padCooldown -= dt;
        }

        bool prevStage = false;
        bool nextStage = false;

        if (kb->IsKeyPressed(DIK_LEFT) || kb->IsKeyPressed(DIK_A)) {
            prevStage = true;
        }
        if (kb->IsKeyPressed(DIK_RIGHT) || kb->IsKeyPressed(DIK_D)) {
            nextStage = true;
        }

        if (pad && s_padCooldown <= 0.0f) {
            float stickX = pad->GetLeftStick().x;
            if (pad->IsDPadLeft() || stickX < -0.5f) {
                prevStage = true;
                s_padCooldown = 0.25f;
            } else if (pad->IsDPadRight() || stickX > 0.5f) {
                nextStage = true;
                s_padCooldown = 0.25f;
            }
        }

        int prevStageIdx = selectedStageIndex_;
        if (prevStage) {
            selectedStageIndex_ = (selectedStageIndex_ + 2) % 3; // 0 -> 2, 1 -> 0, 2 -> 1
        }
        if (nextStage) {
            selectedStageIndex_ = (selectedStageIndex_ + 1) % 3; // 0 -> 1, 1 -> 2, 2 -> 0
        }

        // 数字キー (1, 2, 3) による直接選択
        if (kb->IsKeyPressed(DIK_1) || kb->IsKeyPressed(DIK_NUMPAD1)) {
            selectedStageIndex_ = 0;
        } else if (kb->IsKeyPressed(DIK_2) || kb->IsKeyPressed(DIK_NUMPAD2)) {
            selectedStageIndex_ = 1;
        } else if (kb->IsKeyPressed(DIK_3) || kb->IsKeyPressed(DIK_NUMPAD3)) {
            selectedStageIndex_ = 2;
        }

        if (prevStageIdx != selectedStageIndex_) {
            AudioManager::Play("resources/Sound/10Dyas/SE/SelectMove.mp3", 0.7f);
        }
    }

    // select_1, select_2, select_3 のマテリアルカラーを更新
    const auto& objects = context->GetObjects();
    for (const auto& obj : objects) {
        if (!obj) continue;
        const std::string& name = obj->GetName();

        int stageIdx = -1;
        if (name == "select_1") stageIdx = 0;
        else if (name == "select_2") stageIdx = 1;
        else if (name == "select_3") stageIdx = 2;

        if (stageIdx != -1) {
            if (stageIdx == selectedStageIndex_) {
                // 選択中のオブジェクト: 鮮やかなハイライト色（呼吸パルス発光付き）
                Vector4 color = selectHighlightColor_;
                if (enableStageSelectPulse_) {
                    float pulse = (sinf(stageSelectPulseTimer_ * 5.0f) * 0.5f + 0.5f) * 0.35f; // 0.0 ~ 0.35
                    color.x = (color.x + pulse > 1.0f) ? 1.0f : (color.x + pulse);
                    color.y = (color.y + pulse > 1.0f) ? 1.0f : (color.y + pulse);
                    color.z = (color.z + pulse > 1.0f) ? 1.0f : (color.z + pulse);
                }
                obj->SetColor(color);
            } else {
                // 非選択のオブジェクト: 落ち着いたダークカラー
                obj->SetColor(unselectedColor_);
            }
        }
    }
}

void TitleScene::DisplayImGui(PrimitiveObject* selectedPrimitive) {
#ifdef USE_IMGUI
    ImGui::Begin("タイトル/ステージ選択カメラ調整");

    // フェーズ状態の表示
    const char* phaseStr = "タイトル画面 (待機中)";
    if (phase_ == Phase::kTransitionToSelect) phaseStr = "ステージ選択カメラへ移動中...";
    else if (phase_ == Phase::kStageSelect) phaseStr = "ステージ選択画面 (待機中)";
    else if (phase_ == Phase::kTransitionToCredit) phaseStr = "クレジット画面へ移動中...";
    else if (phase_ == Phase::kCredit) phaseStr = "クレジット画面 (待機中)";
    else if (phase_ == Phase::kTransitionFromCredit) phaseStr = "クレジットからタイトルへ復帰中...";
    ImGui::Text("【現在の状態】: %s", phaseStr);
    ImGui::Separator();

    // タイトルメニューテキスト調整
    ImGui::Text("【タイトルメニュー (スタート / クレジット) 調整】");
    ImGui::Text("現在の選択: %s", (selectedTitleMenu_ == 0) ? "スタート" : "クレジット");
    if (ImGui::Button("選択: スタート")) { selectedTitleMenu_ = 0; }
    ImGui::SameLine();
    if (ImGui::Button("選択: クレジット")) { selectedTitleMenu_ = 1; }

    bool menuSpriteChanged = false;
    if (ImGui::DragFloat2("スタート 位置 (px)", &startTextPos_.x, 1.0f, 0.0f, 1280.0f)) menuSpriteChanged = true;
    if (ImGui::DragFloat2("スタート サイズ (px)", &startTextSize_.x, 1.0f, 10.0f, 600.0f)) menuSpriteChanged = true;
    if (ImGui::DragFloat2("クレジット 位置 (px)", &creditTextPos_.x, 1.0f, 0.0f, 1280.0f)) menuSpriteChanged = true;
    if (ImGui::DragFloat2("クレジット サイズ (px)", &creditTextSize_.x, 1.0f, 10.0f, 600.0f)) menuSpriteChanged = true;

    if (menuSpriteChanged) {
        if (startTextSprite_) {
            startTextSprite_->SetPosition(startTextPos_);
            startTextSprite_->SetSize(startTextSize_);
        }
        if (creditTextSprite_) {
            creditTextSprite_->SetPosition(creditTextPos_);
            creditTextSprite_->SetSize(creditTextSize_);
        }
    }

    ImGui::Separator();

    // 1. タイトルカメラ現在値
    ImGui::Text("【タイトルカメラ (現在値)】");
    ImGui::Checkbox("シネマティック微動 (自動揺れ)", &enableCinematicSway_);
    
    bool changed = false;
    if (ImGui::DragFloat3("位置 (Translate)", &cameraTransform_.translate.x, 0.1f)) {
        changed = true;
    }
    
    // 角度（ラジアンと度数法）
    float rotDeg[3] = {
        cameraTransform_.rotate.x * 180.0f / 3.14159265f,
        cameraTransform_.rotate.y * 180.0f / 3.14159265f,
        cameraTransform_.rotate.z * 180.0f / 3.14159265f
    };
    if (ImGui::DragFloat3("角度 (Deg)", rotDeg, 0.5f, -180.0f, 180.0f, "%.1f°")) {
        cameraTransform_.rotate.x = rotDeg[0] * 3.14159265f / 180.0f;
        cameraTransform_.rotate.y = rotDeg[1] * 3.14159265f / 180.0f;
        cameraTransform_.rotate.z = rotDeg[2] * 3.14159265f / 180.0f;
        changed = true;
    }

    if (changed && gameCamera_) {
        gameCamera_->SetTranslation(cameraTransform_.translate);
        gameCamera_->SetRotation(cameraTransform_.rotate);
        gameCamera_->UpdateMatrix();
    }

    ImGui::Separator();

    // 2. ステージ選択カメラ（目標値）
    ImGui::Text("【ステージ選択カメラ (目標アングル)】");
    ImGui::DragFloat3("選択時 位置", &targetSelectPos_.x, 0.1f);
    float selRotDeg[3] = {
        targetSelectRot_.x * 180.0f / 3.14159265f,
        targetSelectRot_.y * 180.0f / 3.14159265f,
        targetSelectRot_.z * 180.0f / 3.14159265f
    };
    if (ImGui::DragFloat3("選択時 角度 (Deg)", selRotDeg, 0.5f, -180.0f, 180.0f, "%.1f°")) {
        targetSelectRot_.x = selRotDeg[0] * 3.14159265f / 180.0f;
        targetSelectRot_.y = selRotDeg[1] * 3.14159265f / 180.0f;
        targetSelectRot_.z = selRotDeg[2] * 3.14159265f / 180.0f;
    }
    ImGui::DragFloat("カメラ移動時間 (秒)", &transitionDuration_, 0.1f, 0.5f, 5.0f, "%.1f秒");
    ImGui::DragFloat("ロゴ/ライト消去時間 (秒)", &logoFadeDuration_, 0.05f, 0.1f, 2.0f, "%.2f秒");

    if (ImGui::Button("ステージ選択位置に即座に配置")) {
        phase_ = Phase::kStageSelect;
        cameraTransform_.translate = targetSelectPos_;
        cameraTransform_.rotate = targetSelectRot_;
        titleLogoAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Title.mp3", true, 0.4f);
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Select.mp3", true, 0.4f);
        if (gameCamera_) {
            gameCamera_->SetTranslation(targetSelectPos_);
            gameCamera_->SetRotation(targetSelectRot_);
            gameCamera_->UpdateMatrix();
        }
        if (EditorManager::GetInstance() && EditorManager::GetInstance()->GetDebugCamera()) {
            auto dbgCam = EditorManager::GetInstance()->GetDebugCamera();
            dbgCam->SetTranslation(targetSelectPos_);
            dbgCam->SetRotation(targetSelectRot_);
            dbgCam->UpdateMatrix();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("タイトル位置に戻す")) {
        phase_ = Phase::kTitle;
        cameraTransform_.translate = { 0.0f, 1.2f, -8.5f };
        cameraTransform_.rotate = { 0.06f, 0.0f, 0.0f };
        titleLogoAlpha_ = 1.0f;
        searchlightAlpha_ = 1.0f;
        AudioManager::StopBGM("resources/Sound/10Dyas/BGM/Select.mp3");
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Title.mp3", true, 0.4f);
        if (gameCamera_) {
            gameCamera_->SetTranslation(cameraTransform_.translate);
            gameCamera_->SetRotation(cameraTransform_.rotate);
            gameCamera_->UpdateMatrix();
        }
        if (EditorManager::GetInstance() && EditorManager::GetInstance()->GetDebugCamera()) {
            auto dbgCam = EditorManager::GetInstance()->GetDebugCamera();
            dbgCam->SetTranslation(cameraTransform_.translate);
            dbgCam->SetRotation(cameraTransform_.rotate);
            dbgCam->UpdateMatrix();
        }
    }

    if (ImGui::Button("▶ デバッグカメラをステージ選択位置に移動")) {
        if (EditorManager::GetInstance() && EditorManager::GetInstance()->GetDebugCamera()) {
            auto dbgCam = EditorManager::GetInstance()->GetDebugCamera();
            dbgCam->SetTranslation(targetSelectPos_);
            dbgCam->SetRotation(targetSelectRot_);
            dbgCam->UpdateMatrix();
            EditorManager::GetInstance()->SetUseDebugCamera(true);
        }
    }

    if (ImGui::Button("▶ 決定演出 (ステージ選択カメラへ移動) をテスト再生")) {
        phase_ = Phase::kTransitionToSelect;
        transitionStartPos_ = cameraTransform_.translate;
        transitionStartRot_ = cameraTransform_.rotate;
        transitionTimer_ = 0.0f;
        AudioManager::PlayBGM("resources/Sound/10Dyas/BGM/Select.mp3", true, 0.4f);
    }

    ImGui::Separator();

    // 3. クレジットカメラ（目標値）
    ImGui::Text("【クレジットカメラ (目標アングル)】");
    ImGui::DragFloat3("クレジット 位置", &targetCreditPos_.x, 0.05f);
    float creditRotDeg[3] = {
        targetCreditRot_.x * 180.0f / 3.14159265f,
        targetCreditRot_.y * 180.0f / 3.14159265f,
        targetCreditRot_.z * 180.0f / 3.14159265f
    };
    if (ImGui::DragFloat3("クレジット 角度 (Deg)", creditRotDeg, 0.2f, -180.0f, 180.0f, "%.1f°")) {
        targetCreditRot_.x = creditRotDeg[0] * 3.14159265f / 180.0f;
        targetCreditRot_.y = creditRotDeg[1] * 3.14159265f / 180.0f;
        targetCreditRot_.z = creditRotDeg[2] * 3.14159265f / 180.0f;
    }
    if (ImGui::Button("クレジット位置に即座に配置")) {
        phase_ = Phase::kCredit;
        cameraTransform_.translate = targetCreditPos_;
        cameraTransform_.rotate = targetCreditRot_;
        titleLogoAlpha_ = 0.0f;
        titleMenuAlpha_ = 0.0f;
        searchlightAlpha_ = 0.0f;
        if (gameCamera_) {
            gameCamera_->SetTranslation(targetCreditPos_);
            gameCamera_->SetRotation(targetCreditRot_);
            gameCamera_->UpdateMatrix();
        }
        if (EditorManager::GetInstance() && EditorManager::GetInstance()->GetDebugCamera()) {
            auto dbgCam = EditorManager::GetInstance()->GetDebugCamera();
            dbgCam->SetTranslation(targetCreditPos_);
            dbgCam->SetRotation(targetCreditRot_);
            dbgCam->UpdateMatrix();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("▶ クレジット演出をテスト再生")) {
        AudioManager::Play("resources/Sound/10Dyas/SE/TitleCameraMove.mp3", 0.7f);
        phase_ = Phase::kTransitionToCredit;
        transitionStartPos_ = cameraTransform_.translate;
        transitionStartRot_ = cameraTransform_.rotate;
        transitionTimer_ = 0.0f;
    }
    if (ImGui::Button("デバッグカメラをクレジット位置に移動")) {
        if (EditorManager::GetInstance() && EditorManager::GetInstance()->GetDebugCamera()) {
            auto dbgCam = EditorManager::GetInstance()->GetDebugCamera();
            dbgCam->SetTranslation(targetCreditPos_);
            dbgCam->SetRotation(targetCreditRot_);
            dbgCam->UpdateMatrix();
            EditorManager::GetInstance()->SetUseDebugCamera(true);
        }
    }
    ImGui::Spacing();
    ImGui::Text("【クレジット画像 (credit.png) 表示調整】");
    ImGui::DragFloat2("クレジット画像 位置 (px)", &creditPos_.x, 1.0f, 0.0f, 1280.0f);
    ImGui::DragFloat2("クレジット画像 サイズ (px)", &creditSize_.x, 1.0f, 10.0f, 1280.0f);
    ImGui::SliderFloat("クレジット画像 不透明度", &creditAlpha_, 0.0f, 1.0f);
    ImGui::DragFloat("クレジットカメラ移動時間 (秒)", &creditTransitionDuration_, 0.05f, 0.3f, 3.0f, "%.2f秒");

    ImGui::Separator();
    ImGui::Text("【BGM コントロール】");
    static float titleVol = 0.4f;
    static float selectVol = 0.4f;
    if (ImGui::SliderFloat("Title BGM 音量", &titleVol, 0.0f, 1.0f, "%.2f")) {
        AudioManager::SetBGMVolume("resources/Sound/10Dyas/BGM/Title.mp3", titleVol);
    }
    if (ImGui::SliderFloat("Select BGM 音量", &selectVol, 0.0f, 1.0f, "%.2f")) {
        AudioManager::SetBGMVolume("resources/Sound/10Dyas/BGM/Select.mp3", selectVol);
    }
    ImGui::Text("Title BGM: %s", AudioManager::IsBGMPlaying("resources/Sound/10Dyas/BGM/Title.mp3") ? "再生中" : "停止中");
    ImGui::SameLine();
    ImGui::Text(" / Select BGM: %s", AudioManager::IsBGMPlaying("resources/Sound/10Dyas/BGM/Select.mp3") ? "再生中" : "停止中");

    ImGui::Separator();
    DebugCamera* liveDebugCam = EditorManager::GetInstance() ? EditorManager::GetInstance()->GetDebugCamera() : nullptr;
    bool isDebugCamActive = EditorManager::GetInstance() ? EditorManager::GetInstance()->UseDebugCamera() : false;

    if (liveDebugCam) {
        Vector3 dbgPos = liveDebugCam->GetTranslation();
        Vector3 dbgRot = liveDebugCam->GetRotation();
        float dbgDeg[3] = {
            dbgRot.x * 180.0f / 3.14159265f,
            dbgRot.y * 180.0f / 3.14159265f,
            dbgRot.z * 180.0f / 3.14159265f
        };

        ImGui::Text("【デバッグカメラ (マウス操作) の現在値】");
        if (isDebugCamActive) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "状態: デバッグカメラ操作中 (ON)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "状態: ゲームカメラ表示中 (エディタ上部でONにしてください)");
        }

        ImGui::Text("位置: (%.2f, %.2f, %.2f)", dbgPos.x, dbgPos.y, dbgPos.z);
        ImGui::Text("角度(ラジアン): (%.3f, %.3f, %.3f)", dbgRot.x, dbgRot.y, dbgRot.z);
        ImGui::Text("角度(度数法): (%.1f°, %.1f°, %.1f°)", dbgDeg[0], dbgDeg[1], dbgDeg[2]);

        if (ImGui::Button("デバッグカメラをステージ選択位置に移動")) {
            liveDebugCam->SetTranslation(targetSelectPos_);
            liveDebugCam->SetRotation(targetSelectRot_);
            liveDebugCam->UpdateMatrix();
            if (EditorManager::GetInstance()) {
                EditorManager::GetInstance()->SetUseDebugCamera(true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("デバッグカメラをタイトル位置に移動")) {
            liveDebugCam->SetTranslation(cameraTransform_.translate);
            liveDebugCam->SetRotation(cameraTransform_.rotate);
            liveDebugCam->UpdateMatrix();
            if (EditorManager::GetInstance()) {
                EditorManager::GetInstance()->SetUseDebugCamera(true);
            }
        }

        if (ImGui::Button("デバッグカメラの値をタイトルカメラにコピー")) {
            cameraTransform_.translate = dbgPos;
            cameraTransform_.rotate = dbgRot;
            if (gameCamera_) {
                gameCamera_->SetTranslation(dbgPos);
                gameCamera_->SetRotation(dbgRot);
                gameCamera_->UpdateMatrix();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("ステージ選択目標値にコピー")) {
            targetSelectPos_ = dbgPos;
            targetSelectRot_ = dbgRot;
        }
        ImGui::SameLine();
        if (ImGui::Button("クレジット目標値にコピー")) {
            targetCreditPos_ = dbgPos;
            targetCreditRot_ = dbgRot;
        }

        ImGui::Spacing();
        ImGui::Text("C++コード用形式 (ステージ選択カメラ用):");
        ImGui::TextDisabled("pos: { %.2ff, %.2ff, %.2ff }, rot: { %.3ff, %.3ff, %.3ff }",
            dbgPos.x, dbgPos.y, dbgPos.z, dbgRot.x, dbgRot.y, dbgRot.z);
    } else {
        ImGui::TextDisabled("※デバッグカメラを取得できませんでした。");
    }

    ImGui::Separator();
    ImGui::Text("【ステージ選択オブジェクト (select_1, 2, 3) 調整】");

    const char* stageNames[3] = { "ステージ 1 (select_1)", "ステージ 2 (select_2)", "ステージ 3 (select_3)" };
    ImGui::Text("現在の選択ステージ: %s", stageNames[selectedStageIndex_]);

    if (ImGui::Button("ステージ 1 選択")) { selectedStageIndex_ = 0; }
    ImGui::SameLine();
    if (ImGui::Button("ステージ 2 選択")) { selectedStageIndex_ = 1; }
    ImGui::SameLine();
    if (ImGui::Button("ステージ 3 選択")) { selectedStageIndex_ = 2; }

    ImGui::ColorEdit4("選択時カラー (Highlight)", &selectHighlightColor_.x);
    ImGui::ColorEdit4("非選択カラー (Unselected)", &unselectedColor_.x);
    ImGui::Checkbox("パルス明滅演出 (Pulse)", &enableStageSelectPulse_);

    ImGui::Separator();
    ImGui::Text("【予告状（callingCard）突き刺し調整】");
    bool cardParamsChanged = false;
    if (ImGui::DragFloat3("刺さり位置オフセット", &cardTargetOffset_.x, 0.1f)) {
        cardParamsChanged = true;
    }
    float cardRotDeg[3] = {
        cardTargetRot_.x * 180.0f / 3.14159265f,
        cardTargetRot_.y * 180.0f / 3.14159265f,
        cardTargetRot_.z * 180.0f / 3.14159265f
    };
    if (ImGui::DragFloat3("刺さり角度 (Deg)", cardRotDeg, 0.5f, -180.0f, 180.0f, "%.1f°")) {
        cardTargetRot_.x = cardRotDeg[0] * 3.14159265f / 180.0f;
        cardTargetRot_.y = cardRotDeg[1] * 3.14159265f / 180.0f;
        cardTargetRot_.z = cardRotDeg[2] * 3.14159265f / 180.0f;
        cardParamsChanged = true;
    }
    if (ImGui::DragFloat("刺さり時スケール (サイズ)", &cardTargetScale_, 0.02f, 0.1f, 2.0f, "%.2f")) {
        cardParamsChanged = true;
    }
    ImGui::DragFloat("飛翔開始スケール", &cardStartScale_, 0.02f, 0.1f, 3.0f, "%.2f");

    if (cardParamsChanged && callingCardObject_) {
        if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
            if (tc->GetScale().x > 0.001f && cardPhase_ == CardThrowPhase::kNone) {
                Vector3 targetWorldPos = { -18.5f, -8.8f, 22.94f };
                {
                    auto context = Model3DEditorContext::GetInstance();
                    std::string targetName = "select_" + std::to_string(selectedStageIndex_ + 1);
                    for (const auto& obj : context->GetObjects()) {
                        if (obj && obj->GetName() == targetName) {
                            targetWorldPos = obj->GetTranslation();
                            break;
                        }
                    }
                }
                tc->SetPosition({
                    targetWorldPos.x + cardTargetOffset_.x,
                    targetWorldPos.y + cardTargetOffset_.y,
                    targetWorldPos.z + cardTargetOffset_.z
                });
                tc->SetScale({ cardTargetScale_, cardTargetScale_, cardTargetScale_ });
                tc->SetRotation(cardTargetRot_);
            }
        }
    }

    if (ImGui::Button("刺さり位置に予告状を配置して確認")) {
        Vector3 targetWorldPos = { -18.5f, -8.8f, 22.94f };
        {
            auto context = Model3DEditorContext::GetInstance();
            std::string targetName = "select_" + std::to_string(selectedStageIndex_ + 1);
            for (const auto& obj : context->GetObjects()) {
                if (obj && obj->GetName() == targetName) {
                    targetWorldPos = obj->GetTranslation();
                    break;
                }
            }
        }
        if (callingCardObject_) {
            if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
                tc->SetPosition({
                    targetWorldPos.x + cardTargetOffset_.x,
                    targetWorldPos.y + cardTargetOffset_.y,
                    targetWorldPos.z + cardTargetOffset_.z
                });
                tc->SetScale({ cardTargetScale_, cardTargetScale_, cardTargetScale_ });
                tc->SetRotation(cardTargetRot_);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("予告状を非表示")) {
        if (callingCardObject_) {
            if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
                tc->SetScale({ 0.0f, 0.0f, 0.0f });
            }
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("▶ 決定演出 (予告状突き刺し＆暗転) をテスト再生")) {
        phase_ = Phase::kTransitionToGame;
        Vector3 targetWorldPos = { -18.5f, -8.8f, 22.94f };
        {
            auto context = Model3DEditorContext::GetInstance();
            std::string targetName = "select_" + std::to_string(selectedStageIndex_ + 1);
            for (const auto& obj : context->GetObjects()) {
                if (obj && obj->GetName() == targetName) {
                    targetWorldPos = obj->GetTranslation();
                    break;
                }
            }
        }
        StartCallingCardThrow(targetWorldPos);
    }

    ImGui::End();
#endif
}

Vector2 TitleScene::WorldToScreenUV(const Vector3& worldPos) const {
    Matrix4x4 viewProj;
    if (gameCamera_) {
        viewProj = gameCamera_->GetViewMatrix() * gameCamera_->GetProjectionMatrix();
    } else {
        Matrix4x4 vm = TransformFunctions::MakeViewMatrix(cameraTransform_.rotate, cameraTransform_.translate);
        Matrix4x4 pm = TransformFunctions::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 1000.0f);
        viewProj = vm * pm;
    }
    Vector3 ndc = TransformFunctions::EulerTransform(worldPos, viewProj);
    float uvX = (ndc.x + 1.0f) * 0.5f;
    float uvY = (1.0f - ndc.y) * 0.5f;
    if (uvX < 0.0f) uvX = 0.0f; else if (uvX > 1.0f) uvX = 1.0f;
    if (uvY < 0.0f) uvY = 0.0f; else if (uvY > 1.0f) uvY = 1.0f;
    return Vector2(uvX, uvY);
}

void TitleScene::StartIrisIn(const Vector2& centerUV, float duration) {
    isIrisInActive_ = true;
    irisInTimer_ = 0.0f;
    irisInDuration_ = (duration > 0.0f) ? duration : 0.7f;
    irisInCenterUV_ = centerUV;

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetIrisCenter(irisInCenterUV_.x, irisInCenterUV_.y);
        dxCommon->SetIrisRadius(0.0f); // 初期は完全に閉じた状態（黒画面）から開始
        dxCommon->SetIrisSmoothness(0.03f);
        dxCommon->SetIrisIn(true); // 1: Iris In (円が開く)
        dxCommon->SetIrisMaskColor(0.0f, 0.0f, 0.0f, 1.0f);
        dxCommon->SetCompositeIrisEnabled(true);
    }
}

void TitleScene::UpdateIrisIn(float dt) {
    if (!isIrisInActive_) return;

    irisInTimer_ += dt;
    float t = irisInTimer_ / irisInDuration_;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

    // スムーズステップで滑らかに開く (0.0 -> 1.0)
    float ease = t * t * (3.0f - 2.0f * t);
    float currentRadius = ease * irisMaxRadius_;

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetIrisCenter(irisInCenterUV_.x, irisInCenterUV_.y);
        dxCommon->SetIrisRadius(currentRadius);
        dxCommon->SetIrisSmoothness(0.03f);
        dxCommon->SetIrisIn(true);
        dxCommon->SetCompositeIrisEnabled(true);
    }

    if (t >= 1.0f) {
        isIrisInActive_ = false;
        if (dxCommon) {
            dxCommon->SetCompositeIrisEnabled(false); // 完全に開いたらポストプロセスを解除
        }
    }
}

void TitleScene::StartIrisOut(const Vector2& centerUV, float duration) {
    isIrisOutActive_ = true;
    gameTransitionTimer_ = 0.0f;
    gameTransitionDuration_ = (duration > 0.0f) ? duration : 0.85f;
    irisCenterUV_ = centerUV;

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetIrisCenter(irisCenterUV_.x, irisCenterUV_.y);
        dxCommon->SetIrisRadius(irisMaxRadius_);
        dxCommon->SetIrisSmoothness(0.03f);
        dxCommon->SetIrisIn(false); // 0: Iris Out (円が閉じる)
        dxCommon->SetIrisMaskColor(0.0f, 0.0f, 0.0f, 1.0f);
        dxCommon->SetCompositeIrisEnabled(true);
    }
}

void TitleScene::UpdateIrisOut(float dt, SceneManager* sceneManager) {
    if (!isIrisOutActive_) return;

    gameTransitionTimer_ += dt;
    float t = gameTransitionTimer_ / gameTransitionDuration_;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

    // スムーズに円が収縮 (Smoothstep)
    float ease = 1.0f - (t * t * (3.0f - 2.0f * t)); // 1.0 -> 0.0
    float currentRadius = ease * irisMaxRadius_;

    DirectXCommon* dxCommon = DirectXCommon::GetInstance();
    if (dxCommon) {
        dxCommon->SetIrisCenter(irisCenterUV_.x, irisCenterUV_.y);
        dxCommon->SetIrisRadius(currentRadius);
        dxCommon->SetIrisSmoothness(0.03f);
        dxCommon->SetIrisIn(false);
        dxCommon->SetCompositeIrisEnabled(true);
    }

    if (t >= 1.0f) {
        isIrisOutActive_ = false;
        if (sceneManager) {
            // 選択されたステージに応じたマップを設定
            std::string mapPath = "resources/json/shared/MapData/map1.txt";
            if (selectedStageIndex_ == 0) {
                mapPath = "resources/json/shared/MapData/map1.txt";
            } else if (selectedStageIndex_ == 1) {
                mapPath = "resources/json/shared/MapData/map2.txt";
            } else if (selectedStageIndex_ == 2) {
                mapPath = "resources/json/shared/MapData/map3.txt";
            }
            GameScene::s_TargetMapFilePath = mapPath;
            sceneManager->SetData("SelectedStagePath", mapPath);

#ifdef USE_IMGUI
            if (EditorManager::GetInstance()) {
                EditorManager::GetInstance()->SetCurrentSceneType(SceneType::kGame);
                EditorManager::SetPlaying(true);
                EditorManager::GetInstance()->SetUseDebugCamera(false);
            }
#endif
            sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
        }
    }
}

void TitleScene::StartCallingCardThrow(const Vector3& targetPos) {
    AudioManager::Play("resources/Sound/10Dyas/SE/ThrowCard.mp3", 0.75f);
    cardPhase_ = CardThrowPhase::kFlying;
    cardTimer_ = 0.0f;
    cardShakeTimer_ = 0.0f;
    cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };

    Vector3 camPos = cameraTransform_.translate;
    Vector3 camRot = cameraTransform_.rotate;

    Matrix4x4 rotMat = TransformFunctions::Multiply(
        TransformFunctions::MakeRoteXMatrix(camRot.x),
        TransformFunctions::MakeRoteYMatrix(camRot.y)
    );
    Vector3 forward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
    Vector3 right = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 up = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    cardStartPos_ = {
        camPos.x + forward.x * 6.0f + right.x * 2.5f - up.x * 1.5f,
        camPos.y + forward.y * 6.0f + right.y * 2.5f - up.y * 1.5f,
        camPos.z + forward.z * 6.0f + right.z * 2.5f - up.z * 1.5f
    };
    cardTargetPos_ = {
        targetPos.x + cardTargetOffset_.x,
        targetPos.y + cardTargetOffset_.y,
        targetPos.z + cardTargetOffset_.z
    };

    if (callingCardObject_) {
        if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
            tc->SetPosition(cardStartPos_);
            tc->SetScale({ cardStartScale_, cardStartScale_, cardStartScale_ });
            tc->SetRotation({ 0.0f, 0.0f, 0.0f });
        }
    }
}

void TitleScene::UpdateCallingCardThrow(float dt, SceneManager* sceneManager) {
    if (cardPhase_ == CardThrowPhase::kNone) return;

    cardTimer_ += dt;

    if (cardPhase_ == CardThrowPhase::kFlying) {
        float t = cardTimer_ / cardFlyDuration_;
        if (t > 1.0f) t = 1.0f;
        float ease = t * t * t; // EaseInCubic

        Vector3 curPos = {
            cardStartPos_.x + (cardTargetPos_.x - cardStartPos_.x) * ease,
            cardStartPos_.y + (cardTargetPos_.y - cardStartPos_.y) * ease,
            cardStartPos_.z + (cardTargetPos_.z - cardStartPos_.z) * ease
        };
        float curScale = cardStartScale_ + (cardTargetScale_ - cardStartScale_) * ease;
        float spinAngle = (1.0f - ease) * 12.0f;
        Vector3 curRot = {
            cardTargetRot_.x * ease,
            cardTargetRot_.y * ease + spinAngle * 1.5f,
            cardTargetRot_.z * ease + spinAngle * 2.0f
        };

        if (callingCardObject_) {
            if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
                tc->SetPosition(curPos);
                tc->SetScale({ curScale, curScale, curScale });
                tc->SetRotation(curRot);
            }
        }

        if (t >= 1.0f) {
            AudioManager::Play("resources/Sound/10Dyas/SE/CardStuck.mp3", 0.85f);
            cardPhase_ = CardThrowPhase::kStuckWobble;
            cardTimer_ = 0.0f;
            cardShakeTimer_ = 0.22f; // カメラシェイク開始

            if (callingCardObject_) {
                if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
                    tc->SetPosition(cardTargetPos_);
                    tc->SetScale({ cardTargetScale_, cardTargetScale_, cardTargetScale_ });
                    tc->SetRotation(cardTargetRot_);
                }
            }
        }
    } else if (cardPhase_ == CardThrowPhase::kStuckWobble) {
        float wobbleTime = cardTimer_;
        float decay = expf(-wobbleTime * 7.0f);
        float wobbleAngle = sinf(wobbleTime * 38.0f) * 0.18f * decay;

        Vector3 curRot = {
            cardTargetRot_.x + wobbleAngle * 0.5f,
            cardTargetRot_.y,
            cardTargetRot_.z + wobbleAngle
        };

        if (callingCardObject_) {
            if (auto tc = callingCardObject_->GetComponent<TransformComponent>()) {
                tc->SetRotation(curRot);
            }
        }

        if (cardTimer_ >= cardWobbleDuration_) {
            cardPhase_ = CardThrowPhase::kIrisOut;
            Vector2 cardUV = WorldToScreenUV(cardTargetPos_);
            StartIrisOut(cardUV, gameTransitionDuration_);
        }
    } else if (cardPhase_ == CardThrowPhase::kIrisOut) {
        UpdateIrisOut(dt, sceneManager);
    }

    if (cardShakeTimer_ > 0.0f) {
        cardShakeTimer_ -= dt;
        float strength = (cardShakeTimer_ / 0.22f) * 0.45f;
        cameraShakeOffset_ = {
            ((float)rand() / RAND_MAX * 2.0f - 1.0f) * strength,
            ((float)rand() / RAND_MAX * 2.0f - 1.0f) * strength,
            ((float)rand() / RAND_MAX * 2.0f - 1.0f) * strength * 0.5f
        };
    } else {
        cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };
    }
}