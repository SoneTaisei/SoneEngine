#ifdef USE_IMGUI
#include "EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Effect/ParticleManager.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "Input/KeyboardInput.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Game2D/MapChip2D.h"
#include "Renderer/SrvManager.h"
#include "Scene/IScene.h"
#include "Scene/SceneManager.h"
#include "ReplayManager.h"
#include "Core/TimeManager.h"

// ImGuiのヘッダー (パスは環境に合わせてください)
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>

// 枠を借りるための関数 (WindowsApplication.cppからお引越し)
static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    SrvManager::GetInstance()->Allocate(out_cpu_handle, out_gpu_handle);
}

bool EditorManager::isPlaying_ = false;
bool EditorManager::showObjects_ = true;
bool EditorManager::showEffects_ = true;
ImVec2 EditorManager::gameViewPos_ = ImVec2(0, 0);
ImVec2 EditorManager::gameViewSize_ = ImVec2(1280, 720);

// 枠を返すための関数
static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    // 空でOK
}

void EditorManager::Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue) {
    // 1. ImGuiコンテキストの作成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 2. フラグの設定
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // ドッキング有効化

    // =================================================================
    // 日本語フォントの読み込み設定
    // =================================================================
    // Windows標準の「メイリオ」フォントをサイズ18で読み込み、日本語の文字範囲（グリフ）を適用します。
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

    // 3. Win32バックエンドの初期化
    ImGui_ImplWin32_Init(hwnd);

    // 4. DirectX12バックエンドの初期化
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = device;
    init_info.CommandQueue = commandQueue;
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    init_info.SrvDescriptorHeap = SrvManager::GetInstance()->GetSrvDescriptorHeap();
    init_info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
    init_info.SrvDescriptorFreeFn = ImGuiSrvFree;

    ImGui_ImplDX12_Init(&init_info);

    // 5. スタイルの微調整
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}

void EditorManager::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EditorManager::UpdateUI(ModelCommon *modelCommon, GameCamera *gameCamera, DebugCamera *debugCamera, Camera **activeCamera, bool &isDebugCameraActive, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, SceneManager *sceneManager) {

    static bool resetLayout = false;

    // --- テイクオーバーのカウントダウン処理 ---
    if (takeoverCountdown_ > 0.0f) {
        takeoverCountdown_ -= TimeManager::GetInstance().GetDeltaTime();
        if (takeoverCountdown_ <= 0.0f) {
            isPlaying_ = true; // カウントダウン終了でプレイ開始
        } else {
            // カウントダウン表示UI
            auto dxCommon = DirectXCommon::GetInstance();
            ImGui::SetNextWindowPos(ImVec2(dxCommon->GetWindowWidth() / 2.0f, dxCommon->GetWindowHeight() / 3.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Takeover Countdown", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
            ImGui::SetWindowFontScale(6.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "READY... %.1f", takeoverCountdown_);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }
    }

    // 現在のマップファイル名をReplayManagerに教える（録画時に保存するため）
    ReplayManager::GetInstance()->SetCurrentStageFilename(stageFilename_);

    // --- シーンリセット直後のマップ復元処理 ---
    if (sceneJustReset_) {
        IScene* activeScene = sceneManager->GetCurrentScene();
        if (activeScene) {
            MapChip2D* mapChip = activeScene->GetMapChip();
            if (mapChip) {
                if (loadMapDataStrNextFrame_) {
                    mapChip->LoadFromString(mapDataStrToLoad_);
                    loadMapDataStrNextFrame_ = false;
                } else {
                    std::string name = stageFilename_;
                    bool hasExt = false;
                    if (name.length() >= 4) {
                        std::string ext = name.substr(name.length() - 4);
                        if (ext == ".txt" || ext == ".TXT") hasExt = true;
                    }
                    if (!hasExt) name += ".txt";
                    std::string filepath = "json/" + name;
                    mapChip->LoadFromFile(filepath);
                }
            }
        }
        sceneJustReset_ = false;
    }

    // --- リプレイ終了時の自動デバッグカメラ復帰処理 ---
    static bool wasReplayingLastFrame = false;
    bool isReplayingNow = ReplayManager::GetInstance()->IsPlaying();
    if (wasReplayingLastFrame && !isReplayingNow) {
        // TAKEOVERによる停止の時はデバッグカメラに強制復帰させない
        if (takeoverCountdown_ <= 0.0f) {
            useDebugCamera_ = true;
            if (gameCamera && debugCamera) {
                debugCamera->SetTranslation(gameCamera->GetTranslation());
                debugCamera->SetRotation(gameCamera->GetRotation());
            }
        }
    }
    wasReplayingLastFrame = isReplayingNow;

    // --- メインメニューバー ---
    if (ImGui::BeginMainMenuBar()) {
        // PLAY / STOP ボタン
        if (!isPlaying_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
            if (ImGui::Button("再生 (PLAY)")) {
                isPlaying_ = true;
                useDebugCamera_ = false;
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
            if (ImGui::Button("停止 (STOP)")) {
                isPlaying_ = false; // ← ここを修正しました
                useDebugCamera_ = true;
                debugCamera->SetTranslation(gameCamera->GetTranslation());
                debugCamera->SetRotation(gameCamera->GetRotation());

                // リプレイ再生中であればそれも同時に停止させる
                if (ReplayManager::GetInstance()->IsPlaying()) {
                    ReplayManager::GetInstance()->StopPlayback();
                }
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // リプレイ用 PLAY / STOP ボタン
        ReplayManager* replayMgr = ReplayManager::GetInstance();
        if (!replayMgr->IsPlaying()) {
            bool hasData = (replayMgr->GetCurrentReplay().totalFrames > 0) || !replayMgr->GetHistory().empty();
            if (hasData) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.45f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.55f, 0.9f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.35f, 0.7f, 1.0f));
                if (ImGui::Button("リプレイ再生 (REPLAY)")) {
                    // もしアクティブなデータがなければ履歴の先頭を再生
                    if (replayMgr->GetCurrentReplay().totalFrames == 0 && !replayMgr->GetHistory().empty()) {
                        replayMgr->StartPlayback(0);
                    } else {
                        replayMgr->StartPlayback();
                    }
                    useDebugCamera_ = false;
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("リプレイ再生 (REPLAY)");
                ImGui::EndDisabled();
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.4f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.5f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.3f, 0.0f, 1.0f));
            if (ImGui::Button("リプレイ停止 (STOP REPLAY)")) {
                replayMgr->StopPlayback();
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));
            if (ImGui::Button("操作切替 (TAKEOVER)")) {
                replayMgr->StopPlayback();
                takeoverCountdown_ = 1.0f; // 1秒間のカウントダウンを開始
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // Debug Camera チェックボックス
        ImGui::Checkbox("デバッグカメラ", &useDebugCamera_);

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // シーン切替コンボ
        ImGui::Text("シーン:");
        ImGui::SetNextItemWidth(130.0f);
        {
            int sceneIndex = static_cast<int>(currentSceneType_);
            if (ImGui::Combo("##Scene", &sceneIndex, kSceneTypeNames, static_cast<int>(SceneType::kCount))) {
                SceneType newType = static_cast<SceneType>(sceneIndex);
                if (newType != currentSceneType_) {
                    currentSceneType_ = newType;
                    sceneManager->ChangeScene(SceneFactory::CreateScene(currentSceneType_));
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                    SaveSceneConfig();
                }
            }
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        // Window開閉制御メニュー
        if (ImGui::BeginMenu("ウィンドウ")) {
            ImGui::MenuItem("インスペクター", nullptr, &showInspector_);
            ImGui::MenuItem("ヒエラルキー", nullptr, &showHierarchy_);
            ImGui::MenuItem("ゲームビュー", nullptr, &showGameView_);
            ImGui::MenuItem("ポストエフェクト", nullptr, &showPostEffect_);
            ImGui::MenuItem("マップチップ画面", nullptr, &showMapEditor_);
            ImGui::MenuItem("リプレイマネージャー", nullptr, &showReplayManager_);
            ImGui::Separator();
            if (ImGui::MenuItem("レイアウトをリセット")) {
                resetLayout = true;
            }
            ImGui::EndMenu();
        }

        // インスペクターが閉じている場合に表示する復元ボタン
        if (!showInspector_) {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.45f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.35f, 0.7f, 1.0f));
            if (ImGui::Button("インスペクターを開く")) {
                showInspector_ = true;
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::EndMainMenuBar();
    }

    // --- ドッキングの設定 ---
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // 初回起動時またはレイアウト初期化時に自動的に構成する
    static bool first_time = true;
    if (first_time || resetLayout) {
        first_time = false;
        
        // iniファイルが存在しない、もしくは手動リセットの場合のみレイアウトを再構築
        bool hasIniFile = false;
        if (ImGui::GetIO().IniFilename) {
            FILE* f = nullptr;
            fopen_s(&f, ImGui::GetIO().IniFilename, "r");
            if (f) {
                hasIniFile = true;
                fclose(f);
            }
        }

        if (resetLayout || !hasIniFile) {
            resetLayout = false;

            // 一度ノードをクリアして再構築
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID dock_id_main = dockspace_id;
            // 左側に「ヒエラルキー」
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, NULL, &dock_id_main);
            // 右側に「インスペクター」
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, NULL, &dock_id_main);
            // メインの下側に「マップチップ画面」「リプレイマネージャー」など
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.35f, NULL, &dock_id_main);

            // 各ウィンドウを各ノードに割り当てる（※ウィンドウのタイトル文字列と完全一致させる必要があります）
            ImGui::DockBuilderDockWindow("ゲームビュー", dock_id_main);
            ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_bottom);
            ImGui::DockBuilderDockWindow("リプレイマネージャー", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ヒエラルキー", dock_id_left);
            ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
            ImGui::DockBuilderDockWindow("ポストエフェクト", dock_id_right);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    // --- Game View ウィンドウ ---
    if (showGameView_) {
        if (ImGui::Begin("ゲームビュー", &showGameView_)) {
            isGameViewHovered_ = ImGui::IsWindowHovered();
            {
                // ウィンドウのサイズに合わせてアスペクト比を維持して描画
                ImVec2 contentSize = ImGui::GetContentRegionAvail();
                float aspect = 1280.0f / 720.0f; // 元のアスペクト比
                float windowAspect = contentSize.x / contentSize.y;
                ImVec2 imageSize;
                if (windowAspect > aspect) {
                    imageSize.y = contentSize.y;
                    imageSize.x = contentSize.y * aspect;
                } else {
                    imageSize.x = contentSize.x;
                    imageSize.y = contentSize.x / aspect;
                }
                // 中央寄せ
                ImVec2 currentPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));
                gameViewPos_ = ImGui::GetCursorScreenPos();
                gameViewSize_ = imageSize;
                ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);
            }
        }
        ImGui::End();
    }

    // --- Hierarchy ウィンドウ ---
    if (showHierarchy_) {
        if (ImGui::Begin("ヒエラルキー", &showHierarchy_)) {
            IScene *activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                if (ImGui::CollapsingHeader("オブジェクト (Objects)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *obj : activeScene->GetObjects()) {
                        bool isSelected = (selectedObject_ == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            selectedObject_ = obj;
                            selectedParticle_ = nullptr;
                            selectedPrimitive_ = nullptr;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("パーティクル (Particles)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *particle : activeScene->GetParticles()) {
                        bool isSelected = (selectedParticle_ == particle);
                        if (ImGui::Selectable(particle->GetName().c_str(), isSelected)) {
                            selectedParticle_ = particle;
                            selectedObject_ = nullptr;
                            selectedPrimitive_ = nullptr;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("プリミティブ (Primitives)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *primitive : activeScene->GetPrimitives()) {
                        bool isSelected = (selectedPrimitive_ == primitive);
                        if (ImGui::Selectable(primitive->GetName().c_str(), isSelected)) {
                            selectedPrimitive_ = primitive;
                            selectedObject_ = nullptr;
                            selectedParticle_ = nullptr;
                        }
                    }
                }
            }
        }
        ImGui::End();
    }

    // --- デバッグカメラの切り替え制御 ---
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) {
        useDebugCamera_ = !useDebugCamera_;
        if (useDebugCamera_) {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
        }
    }

    // --- Inspector ウィンドウ ---
    if (showInspector_) {
        if (ImGui::Begin("インスペクター", &showInspector_)) {
            if (selectedObject_ || selectedParticle_ || selectedPrimitive_) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.35f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
                if (ImGui::Button("グローバル設定を表示", ImVec2(-1, 0))) {
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                }
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            if (selectedObject_) {
                selectedObject_->DisplayImGui("Object Properties");
            } else if (selectedParticle_) {
                selectedParticle_->DrawImGui();
            } else if (selectedPrimitive_) {
                selectedPrimitive_->DisplayImGui("Primitive Properties");
            } else {
                ImGui::Text("グローバル表示設定");
                ImGui::Separator();
                ImGui::Checkbox("オブジェクトを表示 (モデル)", &showObjects_);
                ImGui::Checkbox("エフェクトを表示 (パーティクル/プリミティブ)", &showEffects_);
                ImGui::Spacing();

                ImGui::Text("アウトライン設定");
                ImGui::Separator();
                {
                    auto dxCommon = DirectXCommon::GetInstance();
                    bool outlineEnabled = dxCommon->IsOutlineEnabled();
                    if (ImGui::Checkbox("アウトラインを有効化", &outlineEnabled)) {
                        dxCommon->SetOutlineEnabled(outlineEnabled);
                        SaveSceneConfig();
                    }
                }
                ImGui::Spacing();

                ImGui::Text("グローバル設定 (ライティング)");
                ImGui::Separator();

                static int activeLightType = 2;
                static bool enableFog = false;
                static float dIntensity = 1.0f;
                static float pIntensity = 1.0f;
                static float sIntensity = 4.0f;
                static float spotAngleDeg = 30.0f;
                static float spotFalloffDeg = 20.0f;

                ImGui::Text("アクティブな光源");
                ImGui::RadioButton("平行光源 (Directional)", &activeLightType, 0);
                ImGui::SameLine();
                ImGui::RadioButton("点光源 (Point)", &activeLightType, 1);
                ImGui::SameLine();
                ImGui::RadioButton("スポットライト (Spot)", &activeLightType, 2);
                ImGui::Separator();

                DirectionalLight *dLight = modelCommon->GetDirectionalLight();
                PointLight *pLight = modelCommon->GetPointLight();
                SpotLight *sLight = modelCommon->GetSpotLight();

                if (activeLightType == 0) {
                    dLight->intensity = dIntensity;
                    pLight->intensity = 0.0f;
                    sLight->intensity = 0.0f;
                    ImGui::Text("平行光源設定");
                    ImGui::ColorEdit4("色", &dLight->color.x);
                    ImGui::DragFloat("輝度 (Intensity)", &dIntensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat3("方向", &dLight->direction.x, 0.01f, -1.0f, 1.0f);
                    dLight->direction = TransformFunctions::Normalize(dLight->direction);
                } else if (activeLightType == 1) {
                    pLight->intensity = pIntensity;
                    dLight->intensity = 0.0f;
                    sLight->intensity = 0.0f;
                    ImGui::Text("点光源設定");
                    ImGui::ColorEdit4("色", &pLight->color.x);
                    ImGui::DragFloat("輝度 (Intensity)", &pIntensity, 0.01f, 0.0f, 10.0f);
                    ImGui::DragFloat3("位置", &pLight->position.x, 0.1f);
                    ImGui::DragFloat("半径 (Radius)", &pLight->radius, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("減衰 (Decay)", &pLight->decay, 0.01f, 0.0f, 10.0f);
                } else if (activeLightType == 2) {
                    sLight->intensity = sIntensity;
                    dLight->intensity = 0.0f;
                    pLight->intensity = 0.0f;
                    ImGui::Text("スポットライト設定");
                    ImGui::ColorEdit4("色", &sLight->color.x);
                    ImGui::DragFloat("輝度 (Intensity)", &sIntensity, 0.01f, 0.0f, 20.0f);
                    ImGui::DragFloat3("位置", &sLight->position.x, 0.1f);
                    if (ImGui::DragFloat3("方向", &sLight->direction.x, 0.01f, -1.0f, 1.0f)) {
                        sLight->direction = TransformFunctions::Normalize(sLight->direction);
                    }
                    ImGui::DragFloat("距離", &sLight->distance, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("減衰 (Decay)", &sLight->decay, 0.01f, 0.0f, 10.0f);
                    ImGui::SliderFloat("全角 (Total Angle)", &spotAngleDeg, 0.0f, 90.0f);
                    ImGui::SliderFloat("フォールオフ開始角", &spotFalloffDeg, 0.0f, spotAngleDeg);
                    sLight->cosAngle = std::cos(spotAngleDeg * (std::numbers::pi_v<float> / 180.0f));
                    sLight->cosFalloffStart = std::cos(spotFalloffDeg * (std::numbers::pi_v<float> / 180.0f));
                }

                ImGui::Separator();
                ImGui::Checkbox("フォグエフェクトを有効化", &enableFog);
            }

            // アクティブなシーン特有のImGui描画（独立ウィンドウや追加インスペクターなど）を呼び出す
            IScene *activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                activeScene->DisplayImGui(selectedPrimitive_);
            }
        }
        ImGui::End();
    }

    // --- PostEffect ウィンドウ ---
    if (showPostEffect_) {
        if (ImGui::Begin("ポストエフェクト", &showPostEffect_)) {
            ImGui::Text("ポストエフェクト設定");
            ImGui::Separator();
            ImGui::Spacing();

            auto dxCommon = DirectXCommon::GetInstance();

            // ポストエフェクト選択Combo
            const char *effectNames[] = {
                "なし (None)",
                "コンポジット (統合エフェクト)",
                "深度ベース・アウトライン"};

            int currentComboIndex = 0;
            auto activeEffect = dxCommon->GetPostEffect();
            if (activeEffect == DirectXCommon::PostEffect::kComposite) {
                currentComboIndex = 1;
            } else if (activeEffect == DirectXCommon::PostEffect::kDepthBasedOutline) {
                currentComboIndex = 2;
            }

            ImGui::Text("アクティブなエフェクト");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##ActiveEffect", &currentComboIndex, effectNames, IM_ARRAYSIZE(effectNames))) {
                if (currentComboIndex == 0) {
                    dxCommon->SetPostEffect(DirectXCommon::PostEffect::kNone);
                } else if (currentComboIndex == 1) {
                    dxCommon->SetPostEffect(DirectXCommon::PostEffect::kComposite);
                } else if (currentComboIndex == 2) {
                    dxCommon->SetPostEffect(DirectXCommon::PostEffect::kDepthBasedOutline);
                }
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ドラッグとキーボード入力（Ctrl+クリック等）が一体化したfloat調整用ヘルパー関数
            auto DrawFloatControl = [](const char *label, float *val, float minVal, float maxVal, float speed = 0.005f) {
                ImGui::Text("%s", label); // ラベルの描画

                ImGui::PushID(label);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragFloat("##drag", val, speed, minVal, maxVal, "%.3f");
                ImGui::PopItemWidth();
                ImGui::PopID();
            };

            // ドラッグとキーボード入力が一体化したint調整用ヘルパー関数
            auto DrawIntControl = [](const char *label, int *val, int minVal, int maxVal, float speed = 0.05f) {
                ImGui::Text("%s", label);

                ImGui::PushID(label);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragInt("##drag", val, speed, minVal, maxVal);
                ImGui::PopItemWidth();
                ImGui::PopID();
            };

            // ラジアルブラー描画用ラムダ
            auto DrawRadialBlurCanvas = [&](float *center, float *blurWidth, int *sampleCount, int *enableFlag = nullptr) {
                if (enableFlag) {
                    bool enabled = (*enableFlag != 0);
                    if (ImGui::Checkbox("ラジアルブラーを有効化", &enabled)) {
                        *enableFlag = enabled ? 1 : 0;
                    }
                    if (!enabled)
                        return;
                }

                ImGui::Spacing();
                DrawFloatControl("ブラー幅 (Blur Width)", blurWidth, 0.0f, 0.1f, 0.001f);
                ImGui::Spacing();
                DrawIntControl("サンプル数 (ブラー品質)", sampleCount, 1, 30);
                ImGui::Spacing();

                ImGui::Text("中心位置 (クリック/ドラッグで調整)");
                ImGui::Spacing();

                float aspect = 1.0f;
                int32_t width = dxCommon->GetWindowWidth();
                int32_t height = dxCommon->GetWindowHeight();
                if (width > 0 && height > 0) {
                    aspect = (float)height / (float)width;
                }

                ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
                ImVec2 canvas_size = ImVec2(200.0f, 200.0f * aspect);

                ImGui::InvisibleButton("##canvas", canvas_size);
                bool is_active = ImGui::IsItemActive();

                if (is_active) {
                    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
                    float x_uv = (mouse_pos.x - canvas_pos.x) / canvas_size.x;
                    float y_uv = (mouse_pos.y - canvas_pos.y) / canvas_size.y;

                    x_uv = (std::max)(0.0f, (std::min)(1.0f, x_uv));
                    y_uv = (std::max)(0.0f, (std::min)(1.0f, y_uv));

                    center[0] = x_uv;
                    center[1] = y_uv;
                }

                ImDrawList *draw_list = ImGui::GetWindowDrawList();

                draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(35, 35, 35, 255));
                draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(80, 80, 80, 255));

                draw_list->AddLine(
                    ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y * 0.5f),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y * 0.5f),
                    IM_COL32(100, 100, 100, 100));
                draw_list->AddLine(
                    ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y),
                    IM_COL32(100, 100, 100, 100));

                ImVec2 dot_pos = ImVec2(canvas_pos.x + center[0] * canvas_size.x, canvas_pos.y + center[1] * canvas_size.y);
                draw_list->AddCircleFilled(dot_pos, 6.0f, IM_COL32(255, 100, 100, 255));
                draw_list->AddCircle(dot_pos, 6.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);

                ImGui::Spacing();
                ImGui::Text("中心 UV: (%.3f, %.3f)", center[0], center[1]);

                ImGui::Spacing();
                ImGui::Text("数値手動入力");
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragFloat2("##center_input", center, 0.002f, 0.0f, 1.0f, "%.3f");
                ImGui::PopItemWidth();
                ImGui::Spacing();
            };

            auto params = dxCommon->GetCompositeParamsData();
            if (params && dxCommon->GetPostEffect() == DirectXCommon::PostEffect::kComposite) {
                if (ImGui::CollapsingHeader("グレースケール設定 (Grayscale)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    DrawFloatControl("グレースケール強度", &params->grayscaleStrength, 0.0f, 1.0f);
                    ImGui::Spacing();
                }
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("セピア設定 (Sepia)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    DrawFloatControl("セピア強度", &params->sepiaStrength, 0.0f, 1.0f);
                    ImGui::Spacing();
                }
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ヴィニエット設定 (Vignette)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    bool enableVignette = (params->enableVignette != 0);
                    if (ImGui::Checkbox("ヴィニエットを有効化", &enableVignette)) {
                        params->enableVignette = enableVignette ? 1 : 0;
                    }
                    if (enableVignette) {
                        ImGui::Text("ヴィニエット色");
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::ColorEdit4("##vignetteColor", params->vignetteColor);
                        ImGui::PopItemWidth();

                        DrawFloatControl("ヴィニエットスケール", &params->vignetteScale, 0.0f, 100.0f, 0.1f);
                        DrawFloatControl("ヴィニエット強度 (Power)", &params->vignettePower, 0.0f, 10.0f, 0.01f);
                    }
                    ImGui::Spacing();
                }
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ブラー設定 (Blur)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    ImGui::Text("ブラータイプ");
                    ImGui::RadioButton("なし", &params->blurType, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("ボックスブラー", &params->blurType, 1);
                    ImGui::SameLine();
                    ImGui::RadioButton("ガウシアンブラー", &params->blurType, 2);

                    if (params->blurType == 1) {
                        ImGui::Spacing();
                        DrawIntControl("カーネルサイズ", &params->boxBlurKernelSize, 1, 5);
                        DrawFloatControl("ブラー強度", &params->boxBlurStrength, 0.0f, 1.0f);
                    } else if (params->blurType == 2) {
                        ImGui::Spacing();
                        DrawFloatControl("ガウシアンシグマ (Sigma)", &params->gaussianSigma, 0.1f, 10.0f, 0.1f);
                    }
                    ImGui::Spacing();
                }
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ラジアルブラー設定 (Radial Blur)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    DrawRadialBlurCanvas(params->radialBlurCenter, &params->radialBlurWidth, &params->radialBlurSamples, &params->enableRadialBlur);
                    ImGui::Spacing();
                }
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ディゾルブ設定 (Dissolve)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    bool enableDissolve = (params->enableDissolve != 0);
                    if (ImGui::Checkbox("ディゾルブを有効化", &enableDissolve)) {
                        params->enableDissolve = enableDissolve ? 1 : 0;
                    }
                    if (enableDissolve) {
                        ImGui::Spacing();
                        DrawFloatControl("しきい値 (Threshold)", &params->dissolveThreshold, 0.0f, 1.0f, 0.005f);
                        ImGui::Spacing();
                        DrawFloatControl("エッジ幅", &params->dissolveEdgeWidth, 0.0f, 0.2f, 0.002f);
                        ImGui::Spacing();

                        ImGui::Text("エッジ色");
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::ColorEdit3("##dissolveEdgeColor", params->dissolveEdgeColor);
                        ImGui::PopItemWidth();
                        ImGui::Spacing();

                        ImGui::Text("背景色 (本体)");
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::ColorEdit3("##dissolveBgColor", params->dissolveBgColor);
                        ImGui::PopItemWidth();
                        ImGui::Spacing();
                    }
                }
                ImGui::Spacing();

                if (ImGui::CollapsingHeader("ノイズ設定 (Noise)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    bool enableNoise = (params->enableNoise != 0);
                    if (ImGui::Checkbox("ノイズを有効化", &enableNoise)) {
                        params->enableNoise = enableNoise ? 1 : 0;
                    }
                    if (enableNoise) {
                        ImGui::Spacing();
                        DrawFloatControl("ノイズ強度", &params->noiseStrength, 0.0f, 1.0f, 0.005f);
                        ImGui::Spacing();
                        DrawFloatControl("ノイズスケール", &params->noiseScale, 1.0f, 1000.0f, 1.0f);
                        ImGui::Spacing();

                        ImGui::Text("ノイズ合成モード (Blend Mode)");
                        const char *blendModeNames[] = {
                            "通常 (Normal)",
                            "加算 (Add)",
                            "乗算 (Multiply)",
                            "スクリーン (Screen)",
                            "オーバーレイ (Overlay)"};
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::Combo("##NoiseBlendMode", &params->noiseBlendMode, blendModeNames, IM_ARRAYSIZE(blendModeNames));
                        ImGui::PopItemWidth();
                        ImGui::Spacing();
                    }
                }
            }
        }
        ImGui::End();
    }

    // --- Map Editor ウィンドウ ---
    if (showMapEditor_) {
        if (ImGui::Begin("マップチップ画面", &showMapEditor_)) {
            IScene *activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                MapChip2D* mapChip = activeScene->GetMapChip();
                if (mapChip) {
                    // 静的変数
                    static int inputWidth = -1;
                    static int inputHeight = -1;

                    if (inputWidth == -1) {
                        inputWidth = mapChip->GetWidth();
                    }
                    if (inputHeight == -1) {
                        inputHeight = mapChip->GetHeight();
                    }

                    // ペイントツール選択
                    static int selectedTool = 1; // 0 = None (Erase), 1 = Block (Paint), 2 = Death (DeathBlock), 3 = Goal, 4 = Coin, 5 = OneWay, 6 = Spawn
                    ImGui::Text("Paint Tool:");
                    ImGui::Spacing();

                    struct ToolIcon {
                        int id;
                        const char* name;
                        ImVec4 color;
                        float scale; // 実際のモデルに合わせたサイズ比率
                    };

                    ToolIcon tools[] = {
                        { 6, "Spawn", ImVec4(0.2f, 0.6f, 1.0f, 1.0f), 1.0f },
                        { 0, "Erase", ImVec4(0.2f, 0.2f, 0.2f, 1.0f), 1.0f },
                        { 1, "Block", ImVec4(0.3f, 0.7f, 0.3f, 1.0f), 1.0f },
                        { 2, "Death", ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 1.0f },
                        { 3, "Goal",  ImVec4(0.8f, 0.2f, 0.8f, 1.0f), 1.0f },
                        { 4, "Coin",  ImVec4(1.0f, 0.8f, 0.0f, 1.0f), 0.5f }, // コインは実際のモデルが0.5倍なので合わせる
                        { 5, "OneWay",ImVec4(0.4f, 0.8f, 0.8f, 1.0f), 1.0f }
                    };

                    int numTools = sizeof(tools) / sizeof(tools[0]);
                    float itemSize = 64.0f; // アイコン枠のサイズ
                    float padding = 8.0f;
                    float totalHeight = itemSize + 24.0f; // アイコン＋テキスト
                    float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
                    float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

                    for (int i = 0; i < numTools; i++) {
                        ImGui::PushID(i);
                        ToolIcon& tool = tools[i];

                        ImVec2 p = ImGui::GetCursorScreenPos();
                        bool isSelected = (selectedTool == tool.id);

                        // 当たり判定 (InvisibleButton)
                        if (ImGui::InvisibleButton("##Tool", ImVec2(itemSize, totalHeight))) {
                            selectedTool = tool.id;
                        }

                        bool isHovered = ImGui::IsItemHovered();
                        ImDrawList* drawList = ImGui::GetWindowDrawList();

                        // 背景 (ホバー時)
                        if (isHovered) {
                            drawList->AddRectFilled(p, ImVec2(p.x + itemSize, p.y + totalHeight), IM_COL32(80, 80, 80, 255), 4.0f);
                        }

                        // アイコン (四角形 - スケールを適用)
                        float iconPadding = 4.0f;
                        float actualIconSize = (itemSize - iconPadding * 2) * tool.scale;
                        // 中央揃えにするためのオフセット計算
                        float offset = (itemSize - actualIconSize) * 0.5f;

                        ImVec2 iconMin = ImVec2(p.x + offset, p.y + offset);
                        ImVec2 iconMax = ImVec2(p.x + offset + actualIconSize, p.y + offset + actualIconSize);
                        
                        // Erase(0) の場合は枠線だけにする、それ以外は塗りつぶし
                        if (tool.id == 0) {
                            drawList->AddRect(iconMin, iconMax, ImGui::ColorConvertFloat4ToU32(tool.color), 4.0f, 0, 2.0f);
                        } else {
                            drawList->AddRectFilled(iconMin, iconMax, ImGui::ColorConvertFloat4ToU32(tool.color), 4.0f);
                        }

                        // テキスト
                        ImVec2 textSize = ImGui::CalcTextSize(tool.name);
                        ImVec2 textPos = ImVec2(p.x + (itemSize - textSize.x) * 0.5f, p.y + itemSize + 2.0f);
                        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), tool.name);

                        // 選択中の黄色の枠線
                        if (isSelected) {
                            drawList->AddRect(p, ImVec2(p.x + itemSize, p.y + totalHeight), IM_COL32(255, 255, 0, 255), 4.0f, 0, 2.0f);
                        }

                        ImGui::PopID();

                        // 折り返し処理 (ウィンドウ幅を超える場合は次の行へ)
                        float lastButtonX2 = ImGui::GetItemRectMax().x;
                        float nextButtonX2 = lastButtonX2 + itemSpacing + itemSize;
                        if (i + 1 < numTools && nextButtonX2 < windowVisibleX) {
                            ImGui::SameLine();
                        }
                    }
                    ImGui::Spacing();

                    ImGui::Separator();

                    // json ディレクトリ内の .txt ファイルを自動走査
                    std::vector<std::string> stageFiles;
                    try {
                        if (std::filesystem::exists("json")) {
                            for (const auto& entry : std::filesystem::directory_iterator("json")) {
                                if (entry.is_regular_file()) {
                                    std::string filename = entry.path().filename().string();
                                    if (filename.length() >= 4) {
                                        std::string ext = filename.substr(filename.length() - 4);
                                        if (ext == ".txt" || ext == ".TXT") {
                                            stageFiles.push_back(filename);
                                        }
                                    }
                                }
                            }
                        }
                    } catch (...) {
                        // エラー時は無視
                    }

                    // 既存のマップファイルを選択するコンボボックス
                    if (!stageFiles.empty()) {
                        static int selectedFileIndex = -1;
                        std::string currentFile = stageFilename_;
                        if (currentFile.length() < 4 || (currentFile.compare(currentFile.length() - 4, 4, ".txt") != 0 && currentFile.compare(currentFile.length() - 4, 4, ".TXT") != 0)) {
                            currentFile += ".txt";
                        }
                        
                        selectedFileIndex = -1;
                        for (int i = 0; i < static_cast<int>(stageFiles.size()); ++i) {
                            if (stageFiles[i] == currentFile) {
                                selectedFileIndex = i;
                                break;
                            }
                        }

                        std::string comboPreview = (selectedFileIndex != -1) ? stageFiles[selectedFileIndex] : "Select existing map...";
                        if (ImGui::BeginCombo("Select Map File", comboPreview.c_str())) {
                            for (int i = 0; i < static_cast<int>(stageFiles.size()); ++i) {
                                bool isSelected = (selectedFileIndex == i);
                                if (ImGui::Selectable(stageFiles[i].c_str(), isSelected)) {
                                    strcpy_s(stageFilename_, sizeof(stageFilename_), stageFiles[i].c_str());
                                    selectedFileIndex = i;
                                }
                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    // ファイル名入力
                    ImGui::InputText("Filename", stageFilename_, sizeof(stageFilename_));

                    // マップサイズ入力と適用ボタン
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::InputInt("Width", &inputWidth);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::InputInt("Height", &inputHeight);
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Size")) {
                        if (inputWidth < 1) inputWidth = 1;
                        if (inputHeight < 1) inputHeight = 1;
                        mapChip->Resize(inputWidth, inputHeight);
                    }

                    ImGui::Spacing();

                    // ファイルパス取得用ラムダ（.txtの自動付与）
                    auto GetFullFilePath = [](const char* filename) {
                        std::string name = filename;
                        bool hasExt = false;
                        if (name.length() >= 4) {
                            std::string ext = name.substr(name.length() - 4);
                            if (ext == ".txt" || ext == ".TXT") {
                                hasExt = true;
                            }
                        }
                        if (!hasExt) {
                            name += ".txt";
                        }
                        return std::string("json/") + name;
                    };

                    // 操作ボタン
                    if (ImGui::Button("Save Map")) {
                        mapChip->SaveToFile(GetFullFilePath(stageFilename_));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load Map")) {
                        if (mapChip->LoadFromFile(GetFullFilePath(stageFilename_))) {
                            inputWidth = mapChip->GetWidth();
                            inputHeight = mapChip->GetHeight();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear Map")) {
                        mapChip->ClearMap();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset Default")) {
                        mapChip->ResetMap();
                        inputWidth = mapChip->GetWidth();
                        inputHeight = mapChip->GetHeight();
                    }

                    ImGui::Separator();

                    // 2Dグリッド描画
                    int mapWidth = mapChip->GetWidth();
                    int mapHeight = mapChip->GetHeight();
                    float buttonSize = 18.0f;

                    // ホバー色・アクティブ色を作成するラムダ
                    auto MakeHoverColor = [](ImVec4 c) {
                        return ImVec4((std::min)(c.x * 1.2f, 1.0f), (std::min)(c.y * 1.2f, 1.0f), (std::min)(c.z * 1.2f, 1.0f), c.w);
                    };
                    auto MakeActiveColor = [](ImVec4 c) {
                        return ImVec4(c.x * 0.8f, c.y * 0.8f, c.z * 0.8f, c.w);
                    };

                    // 横スクロールを可能にするために子ウィンドウを開始
                    if (ImGui::BeginChild("MapGridScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));

                        // Y軸は下側が0なので、画面上は上から下に向かって Y を逆順 (Height-1 から 0) で描画
                        for (int y = mapHeight - 1; y >= 0; --y) {
                            for (int x = 0; x < mapWidth; ++x) {
                                MapChip2D::ChipType cellType = mapChip->GetChip(x, y);
                                std::string btnId = "##cell_" + std::to_string(x) + "_" + std::to_string(y);

                                ImVec4 btnColor;
                                if (cellType == MapChip2D::ChipType::kDeathBlock) {
                                    btnColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);        // デスブロック: 赤色
                                } else if (cellType == MapChip2D::ChipType::kGoal) {
                                    btnColor = ImVec4(0.8f, 0.2f, 0.8f, 1.0f);        // ゴール: 紫色
                                } else if (cellType == MapChip2D::ChipType::kCoin) {
                                    btnColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);        // コイン: 金色
                                } else if (cellType == MapChip2D::ChipType::kOneWayBlock) {
                                    btnColor = ImVec4(0.4f, 0.8f, 0.8f, 1.0f);        // 一方向通行床: 水色
                                } else if (cellType == MapChip2D::ChipType::kPlayerSpawn) {
                                    btnColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);        // 初期座標: 青色
                                } else if (cellType == MapChip2D::ChipType::kBlock) {
                                    if (y <= 1) {
                                        btnColor = ImVec4(0.55f, 0.35f, 0.17f, 1.0f); // 地面: 茶色
                                    } else if (x == 0 || x == mapWidth - 1) {
                                        btnColor = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);   // 壁: 灰色
                                    } else {
                                        btnColor = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);    // その他: 緑
                                    }
                                } else {
                                    btnColor = ImVec4(0.15f, 0.15f, 0.15f, 0.5f);     // 空中: 暗い半透明
                                }

                                ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MakeHoverColor(btnColor));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, MakeActiveColor(btnColor));

                                if (x > 0) {
                                    ImGui::SameLine();
                                }

                                ImVec2 pos = ImGui::GetCursorScreenPos();

                                if (ImGui::Button(btnId.c_str(), ImVec2(buttonSize, buttonSize))) {
                                    mapChip->SetChip(x, y, static_cast<MapChip2D::ChipType>(selectedTool));
                                }

                                ImVec2 mousePos = ImGui::GetIO().MousePos;
                                if (ImGui::IsMouseDown(0) &&
                                    mousePos.x >= pos.x && mousePos.x <= pos.x + buttonSize &&
                                    mousePos.y >= pos.y && mousePos.y <= pos.y + buttonSize) {
                                    if (mapChip->GetChip(x, y) != static_cast<MapChip2D::ChipType>(selectedTool)) {
                                        mapChip->SetChip(x, y, static_cast<MapChip2D::ChipType>(selectedTool));
                                    }
                                }

                                ImGui::PopStyleColor(3);
                            }
                        }
                        ImGui::PopStyleVar();
                        ImGui::EndChild();
                    }
                } else {
                    ImGui::Text("Active scene does not support 2D map editing.");
                }
            } else {
                ImGui::Text("No active scene.");
            }
        }
        ImGui::End();
    }

    // --- Replay Manager ウィンドウ ---
    if (showReplayManager_) {
        if (ImGui::Begin("リプレイマネージャー", &showReplayManager_)) {
            auto replayMgr = ReplayManager::GetInstance();

            // 状態に応じたヘッダー表示
            if (replayMgr->IsRecording()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "● 録画中... (%d フレーム)", replayMgr->GetRecordedFrameCount());
            } else if (replayMgr->IsPlaying()) {
                if (replayMgr->IsPaused()) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "⏸ 一時停止中 (%d / %d F)", replayMgr->GetCurrentFrame(), replayMgr->GetCurrentReplay().totalFrames);
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "▶ 再生中 (%d / %d F)", replayMgr->GetCurrentFrame(), replayMgr->GetCurrentReplay().totalFrames);
                }
            } else {
                ImGui::Text("待機中 (PLAYすると自動で裏録画されます)");
            }
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::BeginTabBar("ReplayTabBar")) {
                // --- タブ1：履歴・保存リスト ---
                if (ImGui::BeginTabItem("履歴・保存リスト")) {
                    ImGui::Text("過去のプレイ履歴 (直近3回分)");
                    ImGui::Separator();
                    
                    const auto& history = replayMgr->GetHistory();
                    if (history.empty()) {
                        ImGui::Text("履歴データはありません。");
                    } else {
                        for (size_t i = 0; i < history.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::Text("履歴 #%d (%s) - %d F", static_cast<int>(i + 1), history[i].dateStr.c_str(), history[i].totalFrames);
                            
                            if (ImGui::Button("再生")) {
                                replayMgr->StartPlayback(static_cast<int>(i));
                                useDebugCamera_ = false;
                                
                                // リプレイのマップ情報があればロードする
                                std::string rMapStr = history[i].mapDataStr;
                                std::string rMap = history[i].stageFilename;
                                if (!rMapStr.empty()) {
                                    mapDataStrToLoad_ = rMapStr;
                                    loadMapDataStrNextFrame_ = true;
                                    sceneJustReset_ = true;
                                    if (!rMap.empty()) strcpy_s(stageFilename_, sizeof(stageFilename_), rMap.c_str());
                                } else if (!rMap.empty()) {
                                    strcpy_s(stageFilename_, sizeof(stageFilename_), rMap.c_str());
                                    // シーンリセットフラグを立てて次のフレームでマップを復元させる
                                    sceneJustReset_ = true;
                                }
                            }
                            
                            ImGui::SameLine();
                            if (ImGui::Button("選択(残像表示)")) {
                                replayMgr->SelectReplay(static_cast<int>(i));
                            }
                            
                            ImGui::SameLine();
                            static char fileNameBuf[3][64] = {};
                            if (fileNameBuf[i][0] == '\0') {
                                sprintf_s(fileNameBuf[i], "replay_history_%d", static_cast<int>(i + 1));
                            }
                            ImGui::SetNextItemWidth(150.0f);
                            ImGui::InputText("##Name", fileNameBuf[i], IM_ARRAYSIZE(fileNameBuf[i]));
                            
                            ImGui::SameLine();
                            if (ImGui::Button("★ 永久保存")) {
                                std::string fname = fileNameBuf[i];
                                if (fname.find(".mml") == std::string::npos) fname += ".mml";
                                replayMgr->SaveToFile(history[i], fname);
                            }
                            ImGui::PopID();
                            ImGui::Spacing();
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Text("永久保存済みリプレイ一覧");
                    ImGui::Separator();

                    const auto& saved = replayMgr->GetSavedList();
                    if (saved.empty()) {
                        ImGui::Text("保存済みのリプレイはありません。");
                    } else {
                        for (size_t i = 0; i < saved.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i + 100));
                            ImGui::Text("📁 %s", saved[i].c_str());
                            
                            if (ImGui::Button("ロード再生")) {
                                replayMgr->StartPlayback(-1, "json/saved_replays/" + saved[i]);
                                useDebugCamera_ = false;
                                
                                // リプレイのマップ情報があればロードする
                                std::string rMapStr = replayMgr->GetCurrentReplay().mapDataStr;
                                std::string rMap = replayMgr->GetCurrentReplay().stageFilename;
                                if (!rMapStr.empty()) {
                                    mapDataStrToLoad_ = rMapStr;
                                    loadMapDataStrNextFrame_ = true;
                                    sceneJustReset_ = true;
                                    if (!rMap.empty()) strcpy_s(stageFilename_, sizeof(stageFilename_), rMap.c_str());
                                } else if (!rMap.empty()) {
                                    strcpy_s(stageFilename_, sizeof(stageFilename_), rMap.c_str());
                                    // シーンリセットフラグを立てて次のフレームでマップを復元させる
                                    sceneJustReset_ = true;
                                }
                            }
                            
                            ImGui::SameLine();
                            if (ImGui::Button("選択(残像表示)")) {
                                replayMgr->SelectReplay(-1, "json/saved_replays/" + saved[i]);
                            }
                            
                            ImGui::SameLine();
                            if (ImGui::Button("削除")) {
                                replayMgr->DeleteSavedFile(saved[i]);
                            }
                            ImGui::PopID();
                            ImGui::Spacing();
                        }
                    }

                    ImGui::EndTabItem();
                }

                // --- タブ2：再生コントロール ---
                if (ImGui::BeginTabItem("再生コントロール")) {
                    if (!replayMgr->IsPlaying()) {
                        ImGui::Text("現在リプレイ再生中ではありません。");
                        ImGui::Text("『履歴・保存リスト』から再生を開始してください。");
                    } else {
                        ImGui::Text("ファイル: %s", replayMgr->GetCurrentReplay().filename.empty() ? "履歴データ" : replayMgr->GetCurrentReplay().filename.c_str());
                        ImGui::Spacing();

                        bool isLoop = replayMgr->IsLoopPlay();
                        if (ImGui::Checkbox("ループ再生 (Loop Play)", &isLoop)) {
                            replayMgr->SetLoopPlay(isLoop);
                        }

                        bool isSnap = replayMgr->IsSnapEnabled();
                        if (ImGui::Checkbox("座標補正 (TAS編集時はOFF推奨)", &isSnap)) {
                            replayMgr->SetSnapEnabled(isSnap);
                        }
                        ImGui::Spacing();

                        if (replayMgr->IsPaused()) {
                            if (ImGui::Button("再開 (Resume)", ImVec2(120, 30))) {
                                replayMgr->ResumePlayback();
                            }
                        } else {
                            if (ImGui::Button("一時停止 (Pause)", ImVec2(120, 30))) {
                                replayMgr->PausePlayback();
                            }
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("停止 (Stop)", ImVec2(120, 30))) {
                            replayMgr->StopPlayback();
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        int curFrame = replayMgr->GetCurrentFrame();
                        int maxFrame = replayMgr->GetCurrentReplay().totalFrames - 1;
                        ImGui::Text("再生位置シーク:");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::SliderInt("##Seek", &curFrame, 0, maxFrame, "Frame %d")) {
                            replayMgr->SetCurrentFrame(curFrame);
                        }
                    }
                    ImGui::EndTabItem();
                }

                // --- タブ3：タイムラインエディタ (TAS) ---
                if (ImGui::BeginTabItem("タイムラインエディタ")) {
                    auto& activeReplay = replayMgr->GetCurrentReplay();
                    if (activeReplay.totalFrames == 0) {
                        ImGui::Text("編集対象のリプレイデータがロードされていません。");
                        ImGui::Text("履歴から再生するか、ファイルをロード再生してください。");
                    } else {
                        ImGui::Text("リプレイ編集 (タイムライン / TAS)");
                        ImGui::Text("総フレーム: %d | 録画日時: %s", activeReplay.totalFrames, activeReplay.dateStr.c_str());
                        ImGui::Spacing();

                        static char saveNameBuf[64] = "edited_replay.mml";
                        ImGui::Text("編集後保存名:");
                        ImGui::SetNextItemWidth(200.0f);
                        ImGui::InputText("##SaveName", saveNameBuf, IM_ARRAYSIZE(saveNameBuf));
                        ImGui::SameLine();
                        if (ImGui::Button("編集内容を保存 (MML+STR)")) {
                            replayMgr->SaveToFile(activeReplay, saveNameBuf);
                        }
                        ImGui::SameLine();
                        ImGui::TextDisabled("※編集内容は自動保存されます");

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Text("▼ カスタムシークバー (右ドラッグで範囲選択 / Ctrl+ホイールでズーム)");
                        
                        // 1. 表示するキーの選択
                        static int selectedKeyToVisualize = 0; // Default to Left
                        const char* keyNames[] = { "Left (L)", "Right (R)", "Jump (J)", "Dash (D)", "Cling (C)", "Up (W)", "Down (S)" };
                        ImGui::Combo("対象キー", &selectedKeyToVisualize, keyNames, IM_ARRAYSIZE(keyNames));
                        
                        static float seekbarZoom = 1.0f;

                        // Ctrl + ホイールでズーム
                        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::GetIO().KeyCtrl) {
                            float wheel = ImGui::GetIO().MouseWheel;
                            if (wheel != 0.0f) {
                                // ズーム倍率の変更
                                float oldZoom = seekbarZoom;
                                seekbarZoom += wheel * 0.1f * seekbarZoom; // 倍率に応じたズーム速度
                                if (seekbarZoom < 0.1f) seekbarZoom = 0.1f;
                                if (seekbarZoom > 100.0f) seekbarZoom = 100.0f;
                            }
                        }

                        // 選択されたブロックの保持用（スコープを外に出す）
                        static int selectedBlockStart = -1;
                        static int selectedBlockEnd = -1;
                        static int selectedBlockKey = -1;

                        int k = selectedKeyToVisualize;
                        if (selectedBlockKey != k) {
                            selectedBlockStart = -1;
                            selectedBlockEnd = -1;
                            selectedBlockKey = k;
                        }

                        // (3) 範囲選択ロジック用の状態（スコープを外に出す）
                        static int rangeStart = -1;
                        static int rangeEnd = -1;
                        static bool isSelecting = false;

                        ImGui::BeginChild("SeekbarScrollRegion", ImVec2(0, 60), false, ImGuiWindowFlags_HorizontalScrollbar);
                        // カスタムシークバー描画
                        ImVec2 p_min = ImGui::GetCursorScreenPos();
                        // ImGui::GetWindowWidth() は BeginChild 内だと利用可能な幅（スクロールなし時）
                        float baseWidth = ImGui::GetWindowWidth(); 
                        float width = baseWidth * seekbarZoom;
                        if (width < 200.0f) width = 200.0f;
                        float height = 40.0f;
                        ImVec2 p_max = ImVec2(p_min.x + width, p_min.y + height);

                        ImGui::InvisibleButton("##CustomSeekbar", ImVec2(width, height));
                        bool isActive = ImGui::IsItemActive(); 
                        bool isHovered = ImGui::IsItemHovered();

                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        
                        int totalFrames = activeReplay.totalFrames;
                        if (totalFrames < 1) totalFrames = 1;
                        int maxFrame = totalFrames - 1;

                        // (1) 背景
                        drawList->AddRectFilled(p_min, p_max, IM_COL32(50, 50, 50, 255), 4.0f);

                        // (1.5) グリッド線の描画
                        float frameWidth = width / totalFrames;
                        float scrollX = ImGui::GetScrollX();
                        float viewWidth = ImGui::GetWindowWidth();
                        
                        int startIdx = (int)(scrollX / width * totalFrames);
                        int endIdx = (int)((scrollX + viewWidth) / width * totalFrames) + 1;
                        if (startIdx < 0) startIdx = 0;
                        if (endIdx > totalFrames) endIdx = totalFrames;

                        if (frameWidth > 2.0f) {
                            // 1フレームの幅が十分あるときは1フレームごとに描画
                            for (int i = startIdx; i <= endIdx; ++i) {
                                float x = p_min.x + (width * i / totalFrames);
                                ImU32 color;
                                if (i % 60 == 0) color = IM_COL32(150, 150, 150, 150);
                                else if (i % 10 == 0) color = IM_COL32(100, 100, 100, 100);
                                else color = IM_COL32(70, 70, 70, 100); // 目立ちすぎない色
                                drawList->AddLine(ImVec2(x, p_min.y), ImVec2(x, p_max.y), color, 1.0f);
                            }
                        } else if (frameWidth > 0.1f) {
                            // 縮小されているときは10フレームごとに描画
                            for (int i = startIdx - (startIdx % 10); i <= endIdx; i += 10) {
                                if (i < 0) continue;
                                float x = p_min.x + (width * i / totalFrames);
                                ImU32 color;
                                if (i % 60 == 0) color = IM_COL32(150, 150, 150, 150);
                                else color = IM_COL32(100, 100, 100, 100);
                                drawList->AddLine(ImVec2(x, p_min.y), ImVec2(x, p_max.y), color, 1.0f);
                            }
                        }

                        // (2) キーONの区間を描画
                        char target = (k==0)?'L':(k==1)?'R':(k==2)?'J':(k==3)?'D':(k==4)?'C':(k==5)?'W':'S';
                        int runStart = -1;
                        for (int i = 0; i <= maxFrame; ++i) {
                            bool on = (activeReplay.frames[i].keys[k] == target);
                            if (on && runStart == -1) runStart = i;
                            else if (!on && runStart != -1) {
                                int runEnd = i - 1;
                                float x1 = p_min.x + (width * runStart / totalFrames);
                                float x2 = p_min.x + (width * i / totalFrames);
                                
                                bool isThisSelected = (selectedBlockStart == runStart && selectedBlockEnd == runEnd && selectedBlockKey == k);
                                
                                // ブロックのクリック判定 (左クリックで選択)
                                if (isHovered && ImGui::IsMouseClicked(0)) {
                                    float mouseX = ImGui::GetIO().MousePos.x;
                                    float mouseY = ImGui::GetIO().MousePos.y;
                                    if (mouseX >= x1 && mouseX <= x2 && mouseY >= p_min.y && mouseY <= p_max.y) {
                                        selectedBlockStart = runStart;
                                        selectedBlockEnd = runEnd;
                                        isThisSelected = true;
                                    }
                                }

                                int jitterAmt = 0;
                                for (const auto& j : activeReplay.jitters) {
                                    if (j.keyIdx == k && j.startFrame == runStart && j.endFrame == runEnd && j.maxJitter > 0) {
                                        jitterAmt = j.maxJitter;
                                        break;
                                    }
                                }

                                // ブレ幅の範囲を青色で描画
                                if (jitterAmt > 0) {
                                    float jx1 = p_min.x + (width * (std::max)(0, runStart - jitterAmt) / totalFrames);
                                    float jx2 = p_min.x + (width * (std::min)(totalFrames, runEnd + 1 + jitterAmt) / totalFrames);
                                    drawList->AddRectFilled(ImVec2(jx1, p_min.y + 1.0f), ImVec2(jx2, p_max.y - 1.0f), IM_COL32(50, 150, 255, 150));
                                }

                                if (isThisSelected) {
                                    drawList->AddRectFilled(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(255, 255, 0, 255));
                                    drawList->AddRect(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
                                } else {
                                    // ブレ設定がされているブロックは色を少し変える
                                    if (jitterAmt > 0) {
                                        drawList->AddRectFilled(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(255, 180, 0, 200));
                                    } else {
                                        drawList->AddRectFilled(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(200, 200, 0, 150));
                                    }
                                }

                                runStart = -1;
                            }
                        }
                        if (runStart != -1) {
                            int runEnd = maxFrame;
                            float x1 = p_min.x + (width * runStart / totalFrames);
                            float x2 = p_min.x + width;
                            drawList->AddRectFilled(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(200, 200, 0, 150));
                        }

                        // 範囲選択ロジックの操作部分
                        float mouseX = ImGui::GetIO().MousePos.x;
                        float t = (std::max)(0.0f, (std::min)(1.0f, (mouseX - p_min.x) / width));
                        int hoverFrame = (int)(t * totalFrames);
                        if (hoverFrame >= totalFrames) hoverFrame = totalFrames - 1;

                        if (ImGui::IsMouseClicked(1) && isHovered) {
                            isSelecting = true;
                            rangeStart = hoverFrame;
                            rangeEnd = hoverFrame;
                        }
                        if (isSelecting) {
                            if (ImGui::IsMouseDown(1)) {
                                rangeEnd = hoverFrame;
                            } else {
                                isSelecting = false;
                            }
                        }

                        // 選択範囲の描画
                        if (rangeStart != -1 && rangeEnd != -1) {
                            int r0 = (std::min)(rangeStart, rangeEnd);
                            int r1 = (std::max)(rangeStart, rangeEnd);
                            float x1 = p_min.x + (width * r0 / totalFrames);
                            float x2 = p_min.x + (width * (r1 + 1) / totalFrames);
                            drawList->AddRectFilled(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(100, 150, 255, 100));
                            drawList->AddRect(ImVec2(x1, p_min.y), ImVec2(x2, p_max.y), IM_COL32(100, 150, 255, 255));
                        }

                        // (4) 現在位置ライン
                        int curFrame = replayMgr->GetCurrentFrame();
                        float curX = p_min.x + (width * curFrame / totalFrames);
                        drawList->AddLine(ImVec2(curX, p_min.y - 2), ImVec2(curX, p_max.y + 2), IM_COL32(255, 50, 50, 255), 2.0f);

                        // 左クリックでシーク
                        if (isActive && ImGui::IsMouseDown(0)) {
                            replayMgr->SetCurrentFrame(hoverFrame);
                        }
                        
                        ImGui::EndChild();

                        ImGui::Spacing();
                        if (rangeStart != -1 && rangeEnd != -1) {
                            int r0 = (std::min)(rangeStart, rangeEnd);
                            int r1 = (std::max)(rangeStart, rangeEnd);
                            ImGui::Text("選択範囲: F%04d ～ F%04d", r0, r1);
                            
                            auto DoRangeEditAndSave = [&](bool isOn) {
                                for(int i=r0; i<=r1; ++i){
                                    replayMgr->ApplyTimelineEdit(i, k, isOn);
                                }
                                if (!activeReplay.filename.empty()) {
                                    replayMgr->SaveToFile(activeReplay, activeReplay.filename);
                                } else {
                                    replayMgr->SaveToFile(activeReplay, saveNameBuf);
                                }
                            };

                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                            if (ImGui::Button("選択範囲を ON にする")) {
                                DoRangeEditAndSave(true);
                            }
                            ImGui::PopStyleColor();

                            ImGui::SameLine();
                            
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                            if (ImGui::Button("選択範囲を OFF にする")) {
                                DoRangeEditAndSave(false);
                            }
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::Text("※右クリック+ドラッグで範囲を選択し、手動でON/OFFを書き換えできます");
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        if (selectedBlockStart != -1 && selectedBlockEnd != -1) {
                            ImGui::Text("選択中のブロック: F%04d ～ F%04d", selectedBlockStart, selectedBlockEnd);
                            
                            // 既存のJitterSettingを探す
                            JitterSetting* currentJitter = nullptr;
                            for (auto& j : activeReplay.jitters) {
                                if (j.keyIdx == k && j.startFrame == selectedBlockStart && j.endFrame == selectedBlockEnd) {
                                    currentJitter = &j;
                                    break;
                                }
                            }
                            
                            int jitterVal = currentJitter ? currentJitter->maxJitter : 0;
                            
                            ImGui::Text("動的なブレ (Jitter) 設定");
                            if (ImGui::SliderInt("ブレの強さ (±フレーム)##Jitter", &jitterVal, 0, 15)) {
                                if (currentJitter) {
                                    currentJitter->maxJitter = jitterVal;
                                } else if (jitterVal > 0) {
                                    JitterSetting j;
                                    j.keyIdx = k;
                                    j.startFrame = selectedBlockStart;
                                    j.endFrame = selectedBlockEnd;
                                    j.maxJitter = jitterVal;
                                    activeReplay.jitters.push_back(j);
                                }
                                
                                // Jitterが0になったら削除する
                                if (jitterVal == 0 && currentJitter) {
                                    auto it = std::remove_if(activeReplay.jitters.begin(), activeReplay.jitters.end(),
                                        [&](const JitterSetting& js) {
                                            return js.keyIdx == k && js.startFrame == selectedBlockStart && js.endFrame == selectedBlockEnd;
                                        });
                                    activeReplay.jitters.erase(it, activeReplay.jitters.end());
                                }

                                if (!activeReplay.filename.empty()) {
                                    replayMgr->SaveToFile(activeReplay, activeReplay.filename);
                                } else {
                                    replayMgr->SaveToFile(activeReplay, saveNameBuf);
                                }
                            }
                            ImGui::TextDisabled("※再生・ループのたびに、このブロックのタイミングがランダムに変化します");
                        } else {
                            ImGui::TextDisabled("※黄色いブロックをクリックすると、動的なブレを設定できます");
                        }


                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
}

void EditorManager::Draw(ID3D12GraphicsCommandList *commandList) {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void EditorManager::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorManager::SaveSceneConfig() {
    std::filesystem::create_directories("json");

    std::ofstream ofs("json/editor_config.json");
    if (ofs.is_open()) {
        auto dxCommon = DirectXCommon::GetInstance();
        ofs << "{" << std::endl;
        ofs << "  \"currentScene\": \"" << SceneFactory::GetSceneTypeName(currentSceneType_) << "\"," << std::endl;
        ofs << "  \"isOutlineEnabled\": " << (dxCommon->IsOutlineEnabled() ? "true" : "false") << "," << std::endl;
        ofs << "  \"outlineThickness\": " << dxCommon->GetOutlineThickness() << std::endl;
        ofs << "}" << std::endl;
        ofs.close();
    }
}

void EditorManager::LoadSceneConfig() {
    std::ifstream ifs("json/editor_config.json");
    if (!ifs.is_open()) {
        return;
    }

    std::string content;
    std::string line;
    while (std::getline(ifs, line)) {
        content += line;
    }
    ifs.close();

    {
        const std::string key = "\"currentScene\"";
        size_t keyPos = content.find(key);
        if (keyPos != std::string::npos) {
            size_t colonPos = content.find(':', keyPos + key.length());
            if (colonPos != std::string::npos) {
                size_t valueStart = content.find('"', colonPos + 1);
                if (valueStart != std::string::npos) {
                    valueStart++;
                    size_t valueEnd = content.find('"', valueStart);
                    if (valueEnd != std::string::npos) {
                        std::string sceneName = content.substr(valueStart, valueEnd - valueStart);
                        currentSceneType_ = SceneFactory::GetSceneTypeFromName(sceneName);
                    }
                }
            }
        }
    }

    {
        const std::string key = "\"isOutlineEnabled\"";
        size_t keyPos = content.find(key);
        if (keyPos != std::string::npos) {
            size_t colonPos = content.find(':', keyPos + key.length());
            if (colonPos != std::string::npos) {
                size_t valPos = content.find_first_not_of(" \t\r\n", colonPos + 1);
                if (valPos != std::string::npos) {
                    if (content.compare(valPos, 4, "true") == 0) {
                        DirectXCommon::GetInstance()->SetOutlineEnabled(true);
                    } else if (content.compare(valPos, 5, "false") == 0) {
                        DirectXCommon::GetInstance()->SetOutlineEnabled(false);
                    }
                }
            }
        }
    }

    {
        const std::string key = "\"outlineThickness\"";
        size_t keyPos = content.find(key);
        if (keyPos != std::string::npos) {
            size_t colonPos = content.find(':', keyPos + key.length());
            if (colonPos != std::string::npos) {
                size_t valPos = content.find_first_not_of(" \t\r\n", colonPos + 1);
                if (valPos != std::string::npos) {
                    size_t endPos = content.find_first_of(",}", valPos);
                    if (endPos != std::string::npos) {
                        std::string numStr = content.substr(valPos, endPos - valPos);
                        try {
                            float thickness = std::stof(numStr);
                            DirectXCommon::GetInstance()->SetOutlineThickness(thickness);
                        } catch (...) {
                        }
                    }
                }
            }
        }
    }
}
#endif