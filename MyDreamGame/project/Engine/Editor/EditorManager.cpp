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
#include "Replay/ReplayManager.h"
#include "Replay/PhysicsAStar.h"
#include "Replay/LevelEvolutionAI.h"
#include "Core/TimeManager.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/LogManager.h"
#include "GameObject/MapObject2D.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Resource/Model/ModelManager.h"
#include "Game2D/Player/Player2D.h"
#include "Component/TransformComponent.h"
#include "Animation/AnimationPreviewScene.h"

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
#include <functional>
#include <nlohmann/json.hpp>

// 枠を借りるための関数 (WindowsApplication.cppからお引越し)
static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    SrvManager::GetInstance()->Allocate(out_cpu_handle, out_gpu_handle);
}

bool EditorManager::isPlaying_ = false;
bool EditorManager::showObjects_ = true;
bool EditorManager::showEffects_ = true;
EditorManager* EditorManager::s_Instance = nullptr;
ImVec2 EditorManager::gameViewPos_ = ImVec2(0, 0);
ImVec2 EditorManager::gameViewSize_ = ImVec2(1280, 720);

// 枠を返すための関数
static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    // 空でOK
}

void EditorManager::Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue) {
    s_Instance = this;
    animationEditor_ = std::make_unique<AnimationEditor>();
    mapEditor_ = std::make_unique<MapEditor>();
    mapEditor_->Initialize();
    model3DEditor_ = std::make_unique<Model3DEditor>();
    model3DEditor_->Initialize(device);
    lightEditor_ = std::make_unique<LightEditor>();
    lightEditor_->Initialize(nullptr);
    ScanLayoutPresets();

    // 1. ImGuiコンテキストの作成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // =================================================================
    // Custom Mid Dark (Mocha と 漆黒の中間くらいのダークテーマ)
    // =================================================================
    ImGuiStyle* style_ptr = &ImGui::GetStyle();
    ImVec4* colors = style_ptr->Colors;

    auto ImVec4FromHex = [](uint32_t hex, float alpha = 1.0f) {
        return ImVec4(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            ((hex >> 0) & 0xFF) / 255.0f,
            alpha
        );
    };

    // Mochaのほのかな青紫みを残しつつ、かなり暗めに設定
    ImVec4 base = ImVec4FromHex(0x14141E);
    ImVec4 mantle = ImVec4FromHex(0x0F0F16);
    ImVec4 crust = ImVec4FromHex(0x08080C);
    ImVec4 surface0 = ImVec4FromHex(0x232333);
    ImVec4 surface1 = ImVec4FromHex(0x333547);
    ImVec4 surface2 = ImVec4FromHex(0x45475A);
    ImVec4 text = ImVec4FromHex(0xCDD6F4);
    ImVec4 subtext0 = ImVec4FromHex(0xA6ADC8);
    ImVec4 mauve = ImVec4FromHex(0xCBA6F7);
    ImVec4 pink = ImVec4FromHex(0xF5C2E7);

    colors[ImGuiCol_Text]                   = text;
    colors[ImGuiCol_TextDisabled]           = subtext0;
    colors[ImGuiCol_WindowBg]               = base;
    colors[ImGuiCol_ChildBg]                = mantle;
    colors[ImGuiCol_PopupBg]                = mantle;
    colors[ImGuiCol_Border]                 = surface1;
    colors[ImGuiCol_BorderShadow]           = crust;
    colors[ImGuiCol_FrameBg]                = surface0;
    colors[ImGuiCol_FrameBgHovered]         = surface1;
    colors[ImGuiCol_FrameBgActive]          = surface2;
    colors[ImGuiCol_TitleBg]                = mantle;
    colors[ImGuiCol_TitleBgActive]          = base;
    colors[ImGuiCol_TitleBgCollapsed]       = crust;
    colors[ImGuiCol_MenuBarBg]              = mantle;
    colors[ImGuiCol_ScrollbarBg]            = mantle;
    colors[ImGuiCol_ScrollbarGrab]          = surface0;
    colors[ImGuiCol_ScrollbarGrabHovered]   = surface1;
    colors[ImGuiCol_ScrollbarGrabActive]    = surface2;
    colors[ImGuiCol_CheckMark]              = mauve;
    colors[ImGuiCol_SliderGrab]             = mauve;
    colors[ImGuiCol_SliderGrabActive]       = pink;
    colors[ImGuiCol_Button]                 = surface0;
    colors[ImGuiCol_ButtonHovered]          = surface1;
    colors[ImGuiCol_ButtonActive]           = surface2;
    colors[ImGuiCol_Header]                 = surface1;
    colors[ImGuiCol_HeaderHovered]          = ImVec4(mauve.x, mauve.y, mauve.z, 0.6f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(mauve.x, mauve.y, mauve.z, 0.8f);
    colors[ImGuiCol_Separator]              = surface1;
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(mauve.x, mauve.y, mauve.z, 0.6f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(mauve.x, mauve.y, mauve.z, 0.8f);
    colors[ImGuiCol_ResizeGrip]             = surface0;
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(mauve.x, mauve.y, mauve.z, 0.6f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(mauve.x, mauve.y, mauve.z, 0.8f);
    colors[ImGuiCol_Tab]                    = mantle;
    colors[ImGuiCol_TabHovered]             = surface1;
    colors[ImGuiCol_TabActive]              = surface0;
    colors[ImGuiCol_TabUnfocused]           = mantle;
    colors[ImGuiCol_TabUnfocusedActive]     = base;
    colors[ImGuiCol_DockingPreview]         = ImVec4(mauve.x, mauve.y, mauve.z, 0.3f);
    colors[ImGuiCol_DockingEmptyBg]         = crust;
    colors[ImGuiCol_PlotLines]              = mauve;
    colors[ImGuiCol_PlotLinesHovered]       = pink;
    colors[ImGuiCol_PlotHistogram]          = mauve;
    colors[ImGuiCol_PlotHistogramHovered]   = pink;
    colors[ImGuiCol_TableHeaderBg]          = surface0;
    colors[ImGuiCol_TableBorderStrong]      = surface1;
    colors[ImGuiCol_TableBorderLight]       = surface0;
    colors[ImGuiCol_TableRowBg]             = base;
    colors[ImGuiCol_TableRowBgAlt]          = mantle;
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(mauve.x, mauve.y, mauve.z, 0.3f);
    colors[ImGuiCol_DragDropTarget]         = mauve;
    colors[ImGuiCol_NavHighlight]           = mauve;
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(subtext0.x, subtext0.y, subtext0.z, 0.7f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(crust.x, crust.y, crust.z, 0.5f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(crust.x, crust.y, crust.z, 0.5f);

    style_ptr->WindowRounding    = 4.0f;
    style_ptr->FrameRounding     = 4.0f;
    style_ptr->GrabRounding      = 4.0f;
    style_ptr->PopupRounding     = 4.0f;
    style_ptr->ScrollbarRounding = 4.0f;
    style_ptr->TabRounding       = 4.0f;

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
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

    // --- 初回起動時 / マップロード時のA*座標初期化および前回マップ読み込み ---
    if (!isAStarPosInitialized_) {
        // アニメーションプレビューシーンが積まれていたら元のシーンに戻す
        if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
            sceneManager->PopScene();
            animationEditor_->SetAnimScenePushed(false);
        }

        IScene* activeScene = sceneManager->GetCurrentScene();
        if (!activeScene || !activeScene->GetMapChip()) {
            if (currentSceneType_ == SceneType::kGame) {
                sceneManager->ChangeScene(SceneFactory::CreateScene(SceneType::kGame));
                sceneManager->ProcessSceneTransition();
                activeScene = sceneManager->GetCurrentScene();
            }
        }

        if (activeScene && activeScene->GetMapChip()) {
            MapChip2D* mapChip = activeScene->GetMapChip();
            // 前回読み込んでいたマップをロード
            const char* currentStageName = mapEditor_ ? mapEditor_->GetStageFilename() : "map_data.txt";
            bool loaded = mapChip->LoadFromStageName(currentStageName);
            if (!loaded) {
                // 前回のマップが存在しない場合はデフォルトマップを表示
                if (mapEditor_) mapEditor_->SetStageFilename("map_data.txt");
                loaded = mapChip->LoadFromStageName("map_data.txt");
                if (!loaded) {
                    if (!mapChip->LoadFromFile("resources/json/shared/Map/map_data.json")) {
                        mapChip->BuildMap();
                        mapChip->GenerateDefaultRooms();
                    }
                }
                SaveSceneConfig();
            }
            if (mapEditor_) {
                mapEditor_->GetContext()->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                mapEditor_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
            }
            isAStarPosInitialized_ = true;
        }
    }

    // --- シーンリセット直後のマップ復元処理 ---
    if (sceneJustReset_) {
        IScene* activeScene = sceneManager->GetCurrentScene();
        if (activeScene) {
            MapChip2D* mapChip = activeScene->GetMapChip();
            if (mapChip) {
                if (loadMapDataStrNextFrame_) {
                    mapChip->LoadFromString(mapDataStrToLoad_);
                    mapChip->GetRooms() = savedRoomsForPlay_;
                    loadMapDataStrNextFrame_ = false;
                } else {
                    const char* curStage = mapEditor_ ? mapEditor_->GetStageFilename() : "map_data.txt";
                    if (!mapChip->LoadFromStageName(curStage)) {
                        if (mapEditor_) mapEditor_->SetStageFilename("map_data.txt");
                        if (!mapChip->LoadFromStageName("map_data.txt")) {
                            if (!mapChip->LoadFromFile("resources/json/shared/Map/map_data.json")) {
                                mapChip->BuildMap();
                                mapChip->GenerateDefaultRooms();
                            }
                        }
                    }
                }
                if (mapEditor_) {
                    mapEditor_->GetContext()->SetInputSize(mapChip->GetWidth(), mapChip->GetHeight());
                    mapEditor_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
                }
            }
        }
        sceneJustReset_ = false;
    }

    auto TrackDragFloat3 = [&](const char* label, Vector3* vec, float speed = 0.1f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", std::function<void()> onUpdate = nullptr) {
        static Vector3 oldVal;
        bool changed = ImGui::DragFloat3(label, &vec->x, speed, v_min, v_max, format);
        if (ImGui::IsItemActivated()) oldVal = *vec;
        if (ImGui::IsItemDeactivatedAfterEdit()) PushCommand(vec, oldVal, *vec, onUpdate);
        return changed;
    };
    auto TrackColorEdit4 = [&](const char* label, Vector4* vec, std::function<void()> onUpdate = nullptr) {
        static Vector4 oldVal;
        bool changed = ImGui::ColorEdit4(label, &vec->x);
        if (ImGui::IsItemActivated()) oldVal = *vec;
        if (ImGui::IsItemDeactivatedAfterEdit()) PushCommand(vec, oldVal, *vec, onUpdate);
        return changed;
    };
    auto TrackDragFloat = [&](const char* label, float* v, float speed = 0.1f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", std::function<void()> onUpdate = nullptr) {
        static float oldVal;
        bool changed = ImGui::DragFloat(label, v, speed, v_min, v_max, format);
        if (ImGui::IsItemActivated()) oldVal = *v;
        if (ImGui::IsItemDeactivatedAfterEdit()) PushCommand(v, oldVal, *v, onUpdate);
        return changed;
    };
    auto TrackSliderFloat = [&](const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", std::function<void()> onUpdate = nullptr) {
        static float oldVal;
        bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format);
        if (ImGui::IsItemActivated()) oldVal = *v;
        if (ImGui::IsItemDeactivatedAfterEdit()) PushCommand(v, oldVal, *v, onUpdate);
        return changed;
    };
    auto TrackActionDragFloat = [&](const char* label, float* v, float speed, float v_min, float v_max, const char* format, std::function<void(float)> setter) {
        static float oldVal;
        bool changed = ImGui::DragFloat(label, v, speed, v_min, v_max, format);
        if (ImGui::IsItemActivated()) oldVal = *v;
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            float newVal = *v;
            PushActionCommand([=](){ setter(oldVal); }, [=](){ setter(newVal); });
        }
        return changed;
    };
    auto TrackActionDragInt = [&](const char* label, int* v, float speed, int v_min, int v_max, std::function<void(int)> setter) {
        static int oldVal;
        bool changed = ImGui::DragInt(label, v, speed, v_min, v_max);
        if (ImGui::IsItemActivated()) oldVal = *v;
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            int newVal = *v;
            PushActionCommand([=](){ setter(oldVal); }, [=](){ setter(newVal); });
        }
        return changed;
    };
    auto TrackActionDragFloat3 = [&](const char* label, float* v, float speed, float v_min, float v_max, const char* format, std::function<void(const Vector3&)> setter) {
        static Vector3 oldVal;
        bool changed = ImGui::DragFloat3(label, v, speed, v_min, v_max, format);
        if (ImGui::IsItemActivated()) oldVal = {v[0], v[1], v[2]};
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Vector3 newVal = {v[0], v[1], v[2]};
            PushActionCommand([=](){ setter(oldVal); }, [=](){ setter(newVal); });
        }
        return changed;
    };

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

    if (mapEditor_) mapEditor_->SetVisible(false);

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
                ImGui::SetWindowFocus("ゲームビュー");

                // --- マップを一時保存（Stop時に復元するため） ---
                if (sceneManager && sceneManager->GetCurrentScene()) {
                    MapChip2D* mapChip = sceneManager->GetCurrentScene()->GetMapChip();
                    if (mapChip) {
                        mapDataStrToLoad_ = mapChip->GetMapDataAsString();
                        savedRoomsForPlay_ = mapChip->GetRooms();
                        mapChip->ResetBlocks();
                    }
                }
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

                if (sceneManager && sceneManager->GetCurrentScene()) {
                    MapChip2D* mapChip = sceneManager->GetCurrentScene()->GetMapChip();
                    if (mapChip) {
                        mapChip->ResetBlocks();
                    }
                }
                
                ImGui::SetWindowFocus("マップチップ画面");

                // --- Stop時に一時保存したマップを復元するフラグを立てる ---
                loadMapDataStrNextFrame_ = true;
                sceneJustReset_ = true;
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
                replayMgr->TakeoverPlayback(); // 乗っ取り用の停止処理を呼ぶ
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
                    selectedGameObject_ = nullptr;
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
            if (ImGui::MenuItem("インスペクター", nullptr, &showInspector_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("ヒエラルキー", nullptr, &showHierarchy_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("ゲームビュー", nullptr, &showGameView_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("ポストエフェクト", nullptr, &showPostEffect_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("マップチップ画面", nullptr, &showMapEditor_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("マップ設定", nullptr, &showMapSettings_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("リプレイエディター", nullptr, &showReplayEditor_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("アニメーションエディター", nullptr, &showAnimEditor_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("ライトエディター", nullptr, &showLightEditor_)) {
                if (showLightEditor_) {
                    activeMainTab_ = "ライトエディター";
                    currentMode_ = EditorMode::Light;
                    showSpotLightPanel_ = true;
                    focusActiveTabCountdown_ = 5;
                    focusSpotLightTabCountdown_ = 5;
                }
                SaveSceneConfig();
            }
            if (ImGui::MenuItem("スポットライト", nullptr, &showSpotLightPanel_)) {
                if (showSpotLightPanel_) {
                    focusSpotLightTabCountdown_ = 5;
                }
                SaveSceneConfig();
            }
            if (ImGui::MenuItem("3Dモデル配置", nullptr, &showModelPlacementEditor_)) { SaveSceneConfig(); }
            if (ImGui::MenuItem("3Dモデルパレット", nullptr, &showModelPalette_)) { SaveSceneConfig(); }
            ImGui::Separator();
            if (ImGui::BeginMenu("レイアウトプリセット")) {
                if (layoutPresets_.empty()) {
                    ImGui::TextDisabled("  (保存されたプリセットはありません)");
                } else {
                    for (const auto& preset : layoutPresets_) {
                        if (ImGui::MenuItem(preset.name.c_str())) {
                            ApplyLayoutPreset(preset.name);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("現在のレイアウトを保存...")) {
                    showSavePresetWindow_ = true;
                    strcpy_s(newPresetNameBuf_, "");
                }
                if (ImGui::BeginMenu("プリセットを削除")) {
                    if (layoutPresets_.empty()) {
                        ImGui::TextDisabled("  (削除できるプリセットはありません)");
                    } else {
                        std::string toDeleteFromMenu = "";
                        for (const auto& preset : layoutPresets_) {
                            std::string delLabel = "削除: " + preset.name;
                            if (ImGui::MenuItem(delLabel.c_str())) {
                                toDeleteFromMenu = preset.name;
                            }
                        }
                        if (!toDeleteFromMenu.empty()) {
                            DeleteLayoutPreset(toDeleteFromMenu);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
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

        // ステータスメッセージ表示（あれば右側に通知）
        if (presetStatusMessageTimer_ > 0.0f && !presetStatusMessage_.empty()) {
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", presetStatusMessage_.c_str());
            float dt = ImGui::GetIO().DeltaTime > 0.0f ? ImGui::GetIO().DeltaTime : 0.016f;
            presetStatusMessageTimer_ -= dt;
            if (presetStatusMessageTimer_ <= 0.0f) {
                presetStatusMessage_.clear();
            }
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
                // 古いiniファイルからレイアウトが崩れるのを防ぐため、新しいウィンドウが無い場合は強制リセット
                bool hasReplayEditor = false;
                bool hasStageSelectEditor = false;
                bool hasDopeSheet = false;
                bool hasLightEditor = false;
                bool hasSpotLight = false;
                bool hasModelPlacement = false;
                bool hasModelPalette = false;
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), f)) {
                    if (strstr(buffer, "リプレイエディター")) {
                        hasReplayEditor = true;
                    }
                    if (strstr(buffer, "ステージセレクトエディター")) {
                        hasStageSelectEditor = true;
                    }
                    if (strstr(buffer, "ドープシート")) {
                        hasDopeSheet = true;
                    }
                    if (strstr(buffer, "ライトエディター")) {
                        hasLightEditor = true;
                    }
                    if (strstr(buffer, "スポットライト")) {
                        hasSpotLight = true;
                    }
                }
                if (!hasReplayEditor || !hasStageSelectEditor || !hasDopeSheet || !hasLightEditor || !hasSpotLight) {
                    if (strstr(buffer, "3Dモデル配置")) {
                        hasModelPlacement = true;
                    }
                    if (strstr(buffer, "3Dモデルパレット")) {
                        hasModelPalette = true;
                    }
                }
                if (!hasReplayEditor || !hasStageSelectEditor || !hasDopeSheet || !hasModelPlacement || !hasModelPalette) {
                    resetLayout = true;
                }
                fclose(f);
            }
        }

        if (resetLayout || !hasIniFile) {
            if (resetLayout) {
                showInspector_ = true;
                showHierarchy_ = true;
                showGameView_ = true;
                showPostEffect_ = true;
                showMapEditor_ = true;
                showMapSettings_ = true;
                showReplayEditor_ = true;
                showAnimEditor_ = true;
                showLightEditor_ = true;
                showSpotLightPanel_ = true;
                showModelPlacementEditor_ = true;
                showModelPalette_ = true;
            }
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
            ImGui::DockBuilderDockWindow("リプレイエディター", dock_id_main);
            ImGui::DockBuilderDockWindow("アニメーションエディター", dock_id_main);
            ImGui::DockBuilderDockWindow("ライトエディター", dock_id_main);
            ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_main);
            ImGui::DockBuilderDockWindow("3Dモデル配置", dock_id_main);

            // 左側
            ImGui::DockBuilderDockWindow("ヒエラルキー", dock_id_left);
            ImGui::DockBuilderDockWindow("マイメディア (リプレイ履歴)", dock_id_left);

            // 右側
            ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
            ImGui::DockBuilderDockWindow("ポストエフェクト", dock_id_right);

            // 下側
            ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);
            ImGui::DockBuilderDockWindow("マップ設定", dock_id_bottom);
            ImGui::DockBuilderDockWindow("3Dモデルパレット", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ステージセレクトエディター", dock_id_bottom);
            ImGui::DockBuilderDockWindow("タイムライン", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ドープシート (タイムライン)", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);
            ImGui::DockBuilderDockWindow("スポットライト", dock_id_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    // --- Game View ウィンドウ ---
    if (showGameView_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "ゲームビュー") {
            ImGui::SetNextWindowFocus();
        }
        if (ImGui::Begin("ゲームビュー", &showGameView_)) {
            bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            if (isFocused && activeMainTab_ != "ゲームビュー") {
                activeMainTab_ = "ゲームビュー";
                currentMode_ = EditorMode::Normal;
                SaveSceneConfig();
            }
            if (activeMainTab_ == "ゲームビュー") {
                currentMode_ = EditorMode::Normal;
                selectedReplaySeekbar_ = false;
                if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
                    sceneManager->PopScene();
                    if (animationEditor_) animationEditor_->SetAnimScenePushed(false);
                }
            }
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

    // --- Replay Editor ウィンドウ (dock_id_main) ---
    isReplayEditorHovered_ = false;
    if (showReplayEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "リプレイエディター") {
            ImGui::SetNextWindowFocus();
        }
        if (ImGui::Begin("リプレイエディター", &showReplayEditor_)) {
            bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            if (isFocused && activeMainTab_ != "リプレイエディター") {
                activeMainTab_ = "リプレイエディター";
                currentMode_ = EditorMode::Replay;
                SaveSceneConfig();
            }
            if (activeMainTab_ == "リプレイエディター") {
                currentMode_ = EditorMode::Replay;
                if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
                    sceneManager->PopScene();
                    if (animationEditor_) animationEditor_->SetAnimScenePushed(false);
                }
            }
            isReplayEditorHovered_ = ImGui::IsWindowHovered();
            
            // プレビュー表示 (ゲーム画面)
            ImVec2 contentSize = ImGui::GetContentRegionAvail();
            float aspect = 1280.0f / 720.0f;
            float windowAspect = contentSize.x / contentSize.y;
            ImVec2 imageSize;
            if (windowAspect > aspect) {
                imageSize.y = contentSize.y;
                imageSize.x = contentSize.y * aspect;
            } else {
                imageSize.x = contentSize.x;
                imageSize.y = contentSize.x / aspect;
            }
            ImVec2 currentPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));
            gameViewPos_ = ImGui::GetCursorScreenPos();
            gameViewSize_ = imageSize;
            ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

            // 2D軌跡・AI探索ルートのオーバーレイ描画
            IScene* activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                Camera* camera = *activeCamera;
                if (camera) {
                    Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                    activeScene->DrawEditorOverlay(viewProj);
                }
            }
        }
        ImGui::End();
    }

    // --- Animation Editor メインウィンドウ (dock_id_main) ---
    if (animationEditor_) animationEditor_->SetHovered(false);
    if (showAnimEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "アニメーションエディター") {
            ImGui::SetNextWindowFocus();
        }
        if (ImGui::Begin("アニメーションエディター", &showAnimEditor_)) {
            bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            if (isFocused && activeMainTab_ != "アニメーションエディター") {
                activeMainTab_ = "アニメーションエディター";
                currentMode_ = EditorMode::Animation;
                SaveSceneConfig();
            }
            if (activeMainTab_ == "アニメーションエディター") {
                currentMode_ = EditorMode::Animation;
                if (animationEditor_ && !animationEditor_->IsAnimScenePushed()) {
                    sceneManager->PushScene(std::make_unique<AnimationPreviewScene>());
                    animationEditor_->SetAnimScenePushed(true);
                    animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_);
                    animationEditor_->RefreshAnimationJointList(sceneManager);
                }
            }
            if (animationEditor_) animationEditor_->SetHovered(ImGui::IsWindowHovered());
            if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->DrawMainView(sceneManager, activeCamera, renderTextureSrvHandle); }
        }
        ImGui::End();
    }

    // --- Light Editor の定数バッファ同期（常時実行） ---
    if (lightEditor_) {
        float dt = TimeManager::GetInstance().GetDeltaTime();
        const Vector3* playerPos = nullptr;
        if (IScene* curScene = sceneManager->GetCurrentScene()) {
            if (Player2D* player = curScene->GetPlayer()) {
                playerPos = &player->GetPosition();
            }
        }
        lightEditor_->Update(dt, modelCommon, playerPos);
    }

    // --- Light Editor メインウィンドウ (dock_id_main) ---
    if (showLightEditor_ && lightEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "ライトエディター") {
            ImGui::SetNextWindowFocus();
        }
        if (ImGui::Begin("ライトエディター", &showLightEditor_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            if (isFocused || ImGui::IsWindowAppearing() || (focusActiveTabCountdown_ > 0 && activeMainTab_ == "ライトエディター")) {
                if (activeMainTab_ != "ライトエディター") {
                    activeMainTab_ = "ライトエディター";
                    focusSpotLightTabCountdown_ = 5;
                    SaveSceneConfig();
                }
                currentMode_ = EditorMode::Light;
                if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
                    sceneManager->PopScene();
                    if (animationEditor_) animationEditor_->SetAnimScenePushed(false);
                }
            }
            if (activeMainTab_ == "ライトエディター") {
                currentMode_ = EditorMode::Light;
            }
            Matrix4x4 vpMatrix = (activeCamera && *activeCamera) ? 
                TransformFunctions::Multiply((*activeCamera)->GetViewMatrix(), (*activeCamera)->GetProjectionMatrix()) : 
                TransformFunctions::MakeIdentity4x4();
            lightEditor_->DrawViewportContent(renderTextureSrvHandle, &vpMatrix);
        }
        ImGui::End();
    }

    // --- 3D Model Placement Editor メインウィンドウ (dock_id_main) ---
    if (model3DEditor_) {
        model3DEditor_->Update();
        if (showModelPlacementEditor_) {
            if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "3Dモデル配置") {
                ImGui::SetNextWindowFocus();
            }
            model3DEditor_->DrawMainViewport(showModelPlacementEditor_, sceneManager, activeCamera, renderTextureSrvHandle, [&]() {
                currentMode_ = EditorMode::ModelPlacement;
                if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
                    sceneManager->PopScene();
                    if (animationEditor_) animationEditor_->SetAnimScenePushed(false);
                }
                if (focusActiveTabCountdown_ == 0 && activeMainTab_ != "3Dモデル配置") {
                    activeMainTab_ = "3Dモデル配置";
                    SaveSceneConfig();
                }
            });
        }
    }

    // --- 左ペイン (通常: ヒエラルキー / リプレイ時: マイメディア) ---
    if (currentMode_ == EditorMode::Replay) {
        // リプレイモード時: 「マイメディア (リプレイ履歴)」を表示
        if (ImGui::Begin("マイメディア (リプレイ履歴)", &showHierarchy_)) {
            auto replayMgr = ReplayManager::GetInstance();
            if (replayMgr->IsRecording()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[録画中...] (%d フレーム)", replayMgr->GetRecordedFrameCount());
            } else if (replayMgr->IsPlaying()) {
                if (replayMgr->IsPaused()) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "[一時停止中] (%d / %d F)", replayMgr->GetCurrentFrame(), replayMgr->GetCurrentReplay().totalFrames);
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[再生中] (%d / %d F)", replayMgr->GetCurrentFrame(), replayMgr->GetCurrentReplay().totalFrames);
                }
            } else {
                ImGui::Text("待機中");
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader("一時履歴データ (未保存)", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto& history = replayMgr->GetHistory();
                if (history.empty()) {
                    ImGui::TextDisabled(" (履歴データなし)");
                } else {
                    static int selectedHistoryIdx = -1;
                    for (int i = 0; i < (int)history.size(); i++) {
                        char label[128];
                        snprintf(label, sizeof(label), "履歴 #%d (%s, %d F)", history[i].id, history[i].dateStr.c_str(), history[i].totalFrames);
                        if (ImGui::Selectable(label, selectedHistoryIdx == i)) {
                            selectedHistoryIdx = i;
                        }
                    }
                    if (selectedHistoryIdx >= 0 && selectedHistoryIdx < (int)history.size()) {
                        ImGui::Spacing();
                        if (ImGui::Button("再生", ImVec2(70, 0))) {
                            replayMgr->StartPlayback(selectedHistoryIdx);
                            if (history[selectedHistoryIdx].mapDataStr != "") {
                                this->mapDataStrToLoad_ = history[selectedHistoryIdx].mapDataStr;
                                this->loadMapDataStrNextFrame_ = true;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("保存", ImVec2(70, 0))) {
                            saveTargetHistoryIdx_ = selectedHistoryIdx;
                            snprintf(saveFileNameBuf_, sizeof(saveFileNameBuf_), "replay_%d", history[selectedHistoryIdx].id);
                            ImGui::OpenPopup("リプレイの保存");
                        }
                    }
                }
            }

            ImGui::Spacing();

            if (ImGui::CollapsingHeader("保存済みリプレイデータ", ImGuiTreeNodeFlags_DefaultOpen)) {
                replayMgr->LoadSavedList();
                const auto& savedFiles = replayMgr->GetSavedList();

                if (savedFiles.empty()) {
                    ImGui::TextDisabled(" (保存データなし)");
                } else {
                    static int selectedFileIdx = -1;
                    for (int i = 0; i < (int)savedFiles.size(); i++) {
                        if (ImGui::Selectable(savedFiles[i].c_str(), selectedFileIdx == i)) {
                            selectedFileIdx = i;
                        }
                    }

                    if (selectedFileIdx >= 0 && selectedFileIdx < (int)savedFiles.size()) {
                        ImGui::Spacing();
                        if (ImGui::Button("ファイル再生", ImVec2(90, 0))) {
                            replayMgr->StartPlayback(-1, savedFiles[selectedFileIdx]);
                            const auto& curReplay = replayMgr->GetCurrentReplay();
                            if (!curReplay.mapDataStr.empty()) {
                                this->mapDataStrToLoad_ = curReplay.mapDataStr;
                                this->loadMapDataStrNextFrame_ = true;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("ファイル削除", ImVec2(90, 0))) {
                            replayMgr->DeleteSavedFile(savedFiles[selectedFileIdx]);
                            selectedFileIdx = -1;
                        }
                    }
                }
            }

            // リプレイ保存時のファイル名入力ダイアログ
            if (ImGui::BeginPopupModal("リプレイの保存", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("保存するファイル名を入力してください (.mml):");
                ImGui::Spacing();
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputText("##SaveFileNameInput", saveFileNameBuf_, sizeof(saveFileNameBuf_));

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("保存", ImVec2(100, 26))) {
                    if (strlen(saveFileNameBuf_) > 0) {
                        const auto& historyList = replayMgr->GetHistory();
                        if (saveTargetHistoryIdx_ >= 0 && saveTargetHistoryIdx_ < (int)historyList.size()) {
                            replayMgr->SaveToFile(historyList[saveTargetHistoryIdx_], std::string(saveFileNameBuf_));
                        }
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("キャンセル", ImVec2(100, 26))) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
    } else {
        // 通常・アニメーションモード時: 「ヒエラルキー」を表示
        if (showHierarchy_) {
            if (ImGui::Begin("ヒエラルキー", &showHierarchy_)) {
                IScene *activeScene = sceneManager->GetCurrentScene();
                if (activeScene) {
                    bool isModelPlacementActive = (activeMainTab_ == "3Dモデル配置") || showModelPlacementEditor_;

                    if (isModelPlacementActive) {
                        // 3Dモデル配置モード時: GameObjects のみ（および3D配置モデル）をヒエラルキーに表示
                        if (ImGui::CollapsingHeader("GameObjects", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (auto &obj : activeScene->GetGameObjects()) {
                                bool isSelected = (selectedGameObject_ == obj);
                                if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                                    selectedGameObject_ = obj;
                                    selectedObject_ = nullptr;
                                    selectedParticle_ = nullptr;
                                    selectedPrimitive_ = nullptr;
                                    if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                                }
                            }
                        }

                        if (model3DEditor_ && !model3DEditor_->GetPlacedObjects().empty()) {
                            if (ImGui::CollapsingHeader("3Dモデル配置 (Placed Models)", ImGuiTreeNodeFlags_DefaultOpen)) {
                                PlacedObject3D* objToDelete = nullptr;
                                for (const auto& placedObj : model3DEditor_->GetPlacedObjects()) {
                                    if (!placedObj) continue;
                                    bool isSelected = (model3DEditor_->GetSelectedObject() == placedObj.get());
                                    std::string label = "  " + placedObj->GetName() + "##Placed_" + std::to_string((uintptr_t)placedObj.get());
                                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                                        model3DEditor_->SetSelectedObject(placedObj.get());
                                        selectedGameObject_ = nullptr;
                                        selectedObject_ = nullptr;
                                        selectedParticle_ = nullptr;
                                        selectedPrimitive_ = nullptr;
                                    }

                                    if (ImGui::BeginPopupContextItem()) {
                                        if (ImGui::MenuItem("複製 (Duplicate)")) {
                                            model3DEditor_->GetContext()->DuplicateObject(placedObj.get());
                                        }
                                        if (ImGui::MenuItem("削除 (Delete)")) {
                                            objToDelete = placedObj.get();
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                                if (objToDelete) {
                                    model3DEditor_->GetContext()->RemoveObject(objToDelete);
                                }
                            }
                        }
                    } else {
                        // 通常・アニメーションモード時: 全カテゴリを表示
                        // 1. プレイヤー（存在する場合）
                        if (activeScene->GetPlayer()) {
                            auto* player = activeScene->GetPlayer();
                            bool isSelected = (selectedPrimitive_ == player->GetPrimitiveObject() || (selectedObject_ && selectedObject_ == player->GetModelObject()));
                            if (ImGui::Selectable("[Player] プレイヤー", isSelected)) {
                                selectedGameObject_ = nullptr;
                                selectedParticle_ = nullptr;
                                selectedPrimitive_ = player->GetPrimitiveObject();
                                selectedObject_ = player->GetModelObject();
                                if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                                if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->RefreshAnimationJointList(sceneManager); }
                            }
                        }

                        // 2. 3Dモデル配置オブジェクト
                        if (model3DEditor_ && !model3DEditor_->GetPlacedObjects().empty()) {
                            if (ImGui::CollapsingHeader("3Dモデル配置 (Placed Models)", ImGuiTreeNodeFlags_DefaultOpen)) {
                                PlacedObject3D* objToDelete = nullptr;
                                for (const auto& placedObj : model3DEditor_->GetPlacedObjects()) {
                                    if (!placedObj) continue;
                                    bool isSelected = (model3DEditor_->GetSelectedObject() == placedObj.get());
                                    std::string label = "  " + placedObj->GetName() + "##Placed_" + std::to_string((uintptr_t)placedObj.get());
                                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                                        model3DEditor_->SetSelectedObject(placedObj.get());
                                        selectedGameObject_ = nullptr;
                                        selectedObject_ = nullptr;
                                        selectedParticle_ = nullptr;
                                        selectedPrimitive_ = nullptr;
                                    }

                                    if (ImGui::BeginPopupContextItem()) {
                                        if (ImGui::MenuItem("複製 (Duplicate)")) {
                                            model3DEditor_->GetContext()->DuplicateObject(placedObj.get());
                                        }
                                        if (ImGui::MenuItem("削除 (Delete)")) {
                                            objToDelete = placedObj.get();
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                                if (objToDelete) {
                                    model3DEditor_->GetContext()->RemoveObject(objToDelete);
                                }
                            }
                        }

                        if (ImGui::CollapsingHeader("GameObjects", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (auto &obj : activeScene->GetGameObjects()) {
                                bool isSelected = (selectedGameObject_ == obj);
                                if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                                    selectedGameObject_ = obj;
                                    selectedObject_ = nullptr;
                                    selectedParticle_ = nullptr;
                                    selectedPrimitive_ = nullptr;
                                    if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                                    if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->RefreshAnimationJointList(sceneManager); }
                                }
                            }
                        }
                        if (ImGui::CollapsingHeader("オブジェクト (Objects)", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (auto *obj : activeScene->GetObjects()) {
                                bool isSelected = (selectedObject_ == obj);
                                if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                                    selectedGameObject_ = nullptr;
                                    selectedObject_ = obj;
                                    selectedParticle_ = nullptr;
                                    selectedPrimitive_ = nullptr;
                                    if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                                    if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->RefreshAnimationJointList(sceneManager); }
                                }
                            }
                        }
                        if (ImGui::CollapsingHeader("パーティクル (Particles)", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (auto *particle : activeScene->GetParticles()) {
                                bool isSelected = (selectedParticle_ == particle);
                                if (ImGui::Selectable(particle->GetName().c_str(), isSelected)) {
                                    selectedGameObject_ = nullptr;
                                    selectedObject_ = nullptr;
                                    selectedParticle_ = particle;
                                    selectedPrimitive_ = nullptr;
                                    if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                                }
                            }
                        }
                        if (ImGui::CollapsingHeader("プリミティブ (Primitives)", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (auto *primitive : activeScene->GetPrimitives()) {
                                bool isSelected = (selectedPrimitive_ == primitive);
                                if (ImGui::Selectable(primitive->GetName().c_str(), isSelected)) {
                                    selectedGameObject_ = nullptr;
                                    selectedObject_ = nullptr;
                                    selectedParticle_ = nullptr;
                                    selectedPrimitive_ = primitive;
                                    if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                                }
                            }
                        }

                        // 選択中オブジェクトのスケルトンボーン一覧
                        if (animationEditor_ && !animationEditor_->GetCurrentJointList().empty()) {
                            ImGui::Spacing();
                            ImGui::Separator();
                            if (ImGui::TreeNodeEx("[Bones] ボーン / 関節", ImGuiTreeNodeFlags_DefaultOpen)) {
                                for (const auto& jointName : animationEditor_->GetCurrentJointList()) {
                                    bool isJointSelected = (animationEditor_->GetSelectedJointName() == jointName);
                                    if (ImGui::Selectable(("  " + jointName).c_str(), isJointSelected)) {
                                        animationEditor_->SetSelectedJointName(jointName);
                                    }
                                }
                                ImGui::TreePop();
                            }
                        }
                    }
                }
            }
            ImGui::End();
        }
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
            int mapTool = mapEditor_ ? mapEditor_->GetContext()->GetSelectedTool() : 0;
            bool isMapChipSelected = (mapTool >= 100 || (mapTool >= 1 && mapTool <= 12));
            bool isPlacedModelSelected = (model3DEditor_ && model3DEditor_->GetSelectedObject() != nullptr);
            if (selectedGameObject_ || selectedObject_ || selectedParticle_ || selectedPrimitive_ || isMapChipSelected || selectedReplayBlock_.IsValid() || isPlacedModelSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.35f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
                if (ImGui::Button("グローバル設定を表示", ImVec2(-1, 0))) {
                    selectedGameObject_ = nullptr;
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                    selectedReplayBlock_.Clear();
                    if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
                    if (mapEditor_) mapEditor_->GetContext()->SetSelectedTool(0);
                    selectedReplaySeekbar_ = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            if (currentMode_ == EditorMode::Animation) {
                if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->DrawInspectorUI(sceneManager); }
            } else if (selectedReplayBlock_.IsValid()) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "キー入力ノード プロパティ");
                ImGui::Separator();
                ImGui::Spacing();

                const char* trackNames[7] = {
                    "左移動 (L)", "右移動 (R)", "上移動 (W)", "下移動 (S)",
                    "ジャンプ (J)", "ダッシュ (D)", "壁張り付き (C)"
                };
                ImGui::Text("トラック: %s", trackNames[selectedReplayBlock_.trackIdx]);
                ImGui::Spacing();

                int startF = selectedReplayBlock_.startFrame;
                int endF = selectedReplayBlock_.endFrame;
                int duration = endF - startF;
                float startSec = startF / 60.0f;
                float durationSec = duration / 60.0f;

                static ReplayData inspectorOldData;
                static SelectedReplayBlock inspectorOldBlock;

                bool changed = false;

                auto CheckEditStart = [&]() {
                    if (ImGui::IsItemActivated()) {
                        inspectorOldData = ReplayManager::GetInstance()->GetCurrentReplay();
                        inspectorOldBlock = selectedReplayBlock_;
                    }
                };

                auto CheckEditEnd = [&]() {
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
                        SelectedReplayBlock newBlock = selectedReplayBlock_;
                        ReplayData oldD = inspectorOldData;
                        SelectedReplayBlock oldB = inspectorOldBlock;

                        PushActionCommand(
                            [oldD, oldB, this]() {
                                auto replayMgr = ReplayManager::GetInstance();
                                replayMgr->GetCurrentReplay() = oldD;
                                replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                                this->selectedReplayBlock_ = oldB;
                            },
                            [newData, newBlock, this]() {
                                auto replayMgr = ReplayManager::GetInstance();
                                replayMgr->GetCurrentReplay() = newData;
                                replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                                this->selectedReplayBlock_ = newBlock;
                            }
                        );
                    }
                };

                if (ImGui::DragInt("開始フレーム", &startF, 1.0f, 0, 99999)) {
                    if (startF < 0) startF = 0;
                    endF = startF + duration;
                    changed = true;
                }
                CheckEditStart();
                CheckEditEnd();
                ImGui::TextDisabled(" (開始時刻: %.2f 秒)", startSec);

                if (ImGui::DragInt("長さ (フレーム数)", &duration, 1.0f, 1, 99999)) {
                    if (duration < 1) duration = 1;
                    endF = startF + duration;
                    changed = true;
                }
                CheckEditStart();
                CheckEditEnd();
                ImGui::TextDisabled(" (継続時間: %.2f 秒)", durationSec);

                if (ImGui::DragInt("終了フレーム", &endF, 1.0f, startF + 1, 99999)) {
                    if (endF <= startF) endF = startF + 1;
                    duration = endF - startF;
                    changed = true;
                }
                CheckEditStart();
                CheckEditEnd();

                if (changed) {
                    ReplayManager::GetInstance()->ModifyBlockRange(
                        selectedReplayBlock_.trackIdx,
                        selectedReplayBlock_.startFrame,
                        selectedReplayBlock_.endFrame,
                        startF,
                        endF
                    );
                    selectedReplayBlock_.startFrame = startF;
                    selectedReplayBlock_.endFrame = endF;
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ノードごとの手振れ/Jitter補正強度編集
                auto replayMgrInstance = ReplayManager::GetInstance();
                auto& currentJitters = replayMgrInstance->GetCurrentReplay().jitters;
                int currentJitterVal = 0;
                for (const auto& j : currentJitters) {
                    if (j.keyIdx == selectedReplayBlock_.trackIdx && j.startFrame == selectedReplayBlock_.startFrame) {
                        currentJitterVal = j.maxJitter;
                        break;
                    }
                }
                if (ImGui::DragInt("手振れ/Jitter強度 (±F)", &currentJitterVal, 1.0f, 0, 30)) {
                    if (currentJitterVal < 0) currentJitterVal = 0;
                    bool found = false;
                    for (auto& j : currentJitters) {
                        if (j.keyIdx == selectedReplayBlock_.trackIdx && j.startFrame == selectedReplayBlock_.startFrame) {
                            j.maxJitter = currentJitterVal;
                            found = true;
                            break;
                        }
                    }
                    if (!found && currentJitterVal > 0) {
                        JitterSetting newJitter;
                        newJitter.keyIdx = selectedReplayBlock_.trackIdx;
                        newJitter.startFrame = selectedReplayBlock_.startFrame;
                        newJitter.endFrame = selectedReplayBlock_.endFrame;
                        newJitter.maxJitter = currentJitterVal;
                        currentJitters.push_back(newJitter);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("再生時に設定フレーム数の範囲内でキー入力をブレ（揺らぎ）させる強度を設定します");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("再生位置をここに合わせる", ImVec2(-1, 28))) {
                    ReplayManager::GetInstance()->SetCurrentFrame(selectedReplayBlock_.startFrame);
                }

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("ノードを削除", ImVec2(-1, 28))) {
                    ReplayData oldData = ReplayManager::GetInstance()->GetCurrentReplay();
                    SelectedReplayBlock oldBlock = selectedReplayBlock_;

                    ReplayManager::GetInstance()->DeleteBlockRange(
                        selectedReplayBlock_.trackIdx,
                        selectedReplayBlock_.startFrame,
                        selectedReplayBlock_.endFrame
                    );
                    selectedReplayBlock_.Clear();

                    ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
                    SelectedReplayBlock newBlock = selectedReplayBlock_;

                    PushActionCommand(
                        [oldData, oldBlock, this]() {
                            auto replayMgr = ReplayManager::GetInstance();
                            replayMgr->GetCurrentReplay() = oldData;
                            replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                            this->selectedReplayBlock_ = oldBlock;
                        },
                        [newData, newBlock, this]() {
                            auto replayMgr = ReplayManager::GetInstance();
                            replayMgr->GetCurrentReplay() = newData;
                            replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                            this->selectedReplayBlock_ = newBlock;
                        }
                    );
                }
                ImGui::PopStyleColor(2);
            } else if (model3DEditor_ && model3DEditor_->GetSelectedObject()) {
                model3DEditor_->DrawInspectorUI(sceneManager);
            } else if (selectedGameObject_) {
                selectedGameObject_->DisplayImGui();
            } else if (selectedObject_) {
                selectedObject_->DisplayImGui("Object Properties");
            } else if (selectedParticle_) {
                selectedParticle_->DrawImGui();
            } else if (selectedPrimitive_) {
                selectedPrimitive_->DisplayImGui("Primitive Properties");
            } else {
                if (currentMode_ == EditorMode::Replay) {
                    ImGui::TextColored(ImVec4(0.55f, 0.4f, 1.0f, 1.0f), "リプレイ グローバル設定 (手振れ補正 & 高速デバッグ)");
                    ImGui::Separator();
                    ImGui::Spacing();

                    auto replayMgrInst = ReplayManager::GetInstance();

                    // --- 1. 再生・補正設定 ---
                    if (ImGui::CollapsingHeader("再生・補正設定", ImGuiTreeNodeFlags_DefaultOpen)) {
                        bool isSnap = replayMgrInst->IsSnapEnabled();
                        if (ImGui::Checkbox("位置補正 (手振れ補正/スナップ)", &isSnap)) {
                            replayMgrInst->SetSnapEnabled(isSnap);
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生時の位置ズレを強制スナップ補正するON/OFF");

                        bool isLoop = replayMgrInst->IsLoopPlay();
                        if (ImGui::Checkbox("ループ再生", &isLoop)) {
                            replayMgrInst->SetLoopPlay(isLoop);
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("リプレイ再生をループさせるON/OFF");

                        bool isInterpolation = replayMgrInst->IsInterpolationEnabled();
                        if (ImGui::Checkbox("座標補間 (スムーズ再生)", &isInterpolation)) {
                            replayMgrInst->SetInterpolationEnabled(isInterpolation);
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("フレーム間のプレイヤー座標を補間してスムーズに描画するON/OFF");
                    }

                    // --- 2. 第1章 完全決定論的デバッグ & 高速自動モンキーテスト ---
                    if (ImGui::CollapsingHeader("完全決定論デバッグ & 高速自動モンキーテスト", ImGuiTreeNodeFlags_DefaultOpen)) {
                        int currentSeed = static_cast<int>(replayMgrInst->GetRandomSeed());
                        if (ImGui::DragInt("乱数シード (Seed)", &currentSeed, 1.0f, 0, 999999)) {
                            if (currentSeed < 0) currentSeed = 0;
                            replayMgrInst->SetRandomSeed(static_cast<uint32_t>(currentSeed));
                        }

                        static int testIterations = 20;
                        static int testJitterChance = 5;
                        ImGui::DragInt("テスト試行回数", &testIterations, 1.0f, 1, 500);
                        ImGui::SliderInt("Jitter発生確率 (%)", &testJitterChance, 0, 50);

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
                        if (ImGui::Button("高速自動モンキーテストを実行", ImVec2(-1, 30))) {
                            IScene* activeScene = sceneManager->GetCurrentScene();
                            MapChip2D* mapChip = activeScene ? activeScene->GetMapChip() : nullptr;
                            replayMgrInst->ExecuteFastMonkeyTest(mapChip, testIterations, testJitterChance);
                        }
                        ImGui::PopStyleColor();

                        const auto& logs = replayMgrInst->GetMonkeyTestLogs();
                        if (!logs.empty()) {
                            ImGui::Spacing();
                            ImGui::Text("テスト実行ログ (%d 件)", (int)logs.size());
                            ImGui::BeginChild("MonkeyTestLogArea", ImVec2(0, 120), true);
                            for (const auto& logLine : logs) {
                                if (logLine.find("[BUG") != std::string::npos) {
                                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", logLine.c_str());
                                } else if (logLine.find("[SUCCESS]") != std::string::npos) {
                                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", logLine.c_str());
                                } else {
                                    ImGui::TextUnformatted(logLine.c_str());
                                }
                            }
                            ImGui::EndChild();
                            if (ImGui::Button("ログをクリア")) {
                                replayMgrInst->ClearMonkeyTestLogs();
                            }
                        }
                    }

                    // --- 3. 第2章 ステージ難易度自動スコアリング ---
                    if (ImGui::CollapsingHeader("ステージ難易度自動スコアリング", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::Button("リプレイ難易度を解析", ImVec2(-1, 28))) {
                            IScene* activeScene = sceneManager->GetCurrentScene();
                            MapChip2D* mapChip = activeScene ? activeScene->GetMapChip() : nullptr;
                            replayMgrInst->AnalyzeReplayDifficulty(replayMgrInst->GetCurrentReplay().frames, mapChip);
                        }

                        const auto& score = replayMgrInst->GetLastAnalyzedScore();
                        ImGui::Text("操作速度 (APM): %.1f", score.averageAPM);
                        ImGui::Text("崖際精度 (Precision): %.1f 点", score.maxPrecisionScore);
                        ImGui::Text("停滞時間 (Stagnation): %.2f 秒", score.stagnationDuration);
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "総合推定難易度スコア: %.1f", score.finalCalculatedDifficulty);
                    }

                    // --- 3.5 第3章 遺伝的アルゴリズムによる自動進化 ---
                    if (ImGui::CollapsingHeader("ギミック自動進化AI (遺伝的アルゴリズム)", ImGuiTreeNodeFlags_DefaultOpen)) {
                        static float targetDifficulty = 50.0f;
                        static int generations = 20;

                        ImGui::SliderFloat("目標難易度", &targetDifficulty, 10.0f, 100.0f, "%.1f");
                        ImGui::SliderInt("進化世代数", &generations, 5, 100);

                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.7f, 1.0f));
                        if (ImGui::Button("自動進化を実行", ImVec2(-1, 30))) {
                            auto* evAI = replayMgrInst->GetLevelEvolutionAI();
                            if (evAI) {
                                IScene* activeScene = sceneManager->GetCurrentScene();
                                MapChip2D* mapChip = activeScene ? activeScene->GetMapChip() : nullptr;
                                evAI->RunEvolution(mapChip, replayMgrInst, targetDifficulty, generations);
                            }
                        }
                        ImGui::PopStyleColor();

                        auto* evAI = replayMgrInst->GetLevelEvolutionAI();
                        if (evAI && !evAI->GetEvolutionLogs().empty()) {
                            ImGui::Spacing();
                            ImGui::Text("進化実行ログ");
                            ImGui::BeginChild("EvolutionLogArea", ImVec2(0, 150), true);
                            for (const auto& logLine : evAI->GetEvolutionLogs()) {
                                if (logLine.find("[SUCCESS]") != std::string::npos) {
                                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", logLine.c_str());
                                } else if (logLine.find("[WARN]") != std::string::npos || logLine.find("[ERROR]") != std::string::npos) {
                                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", logLine.c_str());
                                } else {
                                    ImGui::TextUnformatted(logLine.c_str());
                                }
                            }
                            ImGui::EndChild();
                            if (ImGui::Button("ログをクリア##GA")) {
                                evAI->ClearLogs();
                            }
                        }
                    }

                    // --- 4. 第4章 物理ベースA* (詰みチェック & AIルート表示) ---
                    if (ImGui::CollapsingHeader("物理ベースA* (詰みチェック & AIルート表示)", ImGuiTreeNodeFlags_DefaultOpen)) {
                        static int maxAStarNodes = 30000;

                        ImGui::DragFloat2("スタート座標 (X, Y)", aStarStartPos_, 0.5f);
                        if (ImGui::Button("マップのスタート位置から自動取得", ImVec2(-1, 0))) {
                            IScene* activeScene = sceneManager->GetCurrentScene();
                            if (activeScene && activeScene->GetMapChip()) {
                                UpdateAStarPositionsFromMap(activeScene->GetMapChip(), sceneManager);
                            }
                        }
                        ImGui::Spacing();

                        ImGui::DragFloat2("ゴール座標 (X, Y)", aStarGoalPos_, 0.5f);
                        if (ImGui::Button("マップのゴールブロック位置から自動取得", ImVec2(-1, 0))) {
                            IScene* activeScene = sceneManager->GetCurrentScene();
                            if (activeScene && activeScene->GetMapChip()) {
                                UpdateAStarPositionsFromMap(activeScene->GetMapChip(), sceneManager);
                            }
                        }
                        ImGui::Spacing();

                        ImGui::DragInt("探索上限ノード数", &maxAStarNodes, 1000, 1000, 150000);

                        bool showAI = replayMgrInst->IsShowAIGhost();
                        if (ImGui::Checkbox("AIルートをゴースト表示", &showAI)) {
                            replayMgrInst->SetShowAIGhost(showAI);
                        }

                        bool isSearching = replayMgrInst->IsAISearching();
                        if (isSearching) {
                            ImGui::BeginDisabled();
                        }
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
                        std::string btnText = isSearching ? "AI探索中... (バックグラウンド計算中)" : "物理A* AI探索を実行 (詰みチェック)";
                        if (ImGui::Button(btnText.c_str(), ImVec2(-1, 30))) {
                            IScene* activeScene = sceneManager->GetCurrentScene();
                            MapChip2D* mapChip = activeScene ? activeScene->GetMapChip() : nullptr;
                            Vector3 sPos = { aStarStartPos_[0], aStarStartPos_[1], 0.0f };
                            Vector3 gPos = { aStarGoalPos_[0], aStarGoalPos_[1], 0.0f };

                            PlayerParams params{};
                            if (activeScene && activeScene->GetPlayer()) {
                                params = activeScene->GetPlayer()->GetParams();
                            }
                            replayMgrInst->ExecuteAStarAsync(sPos, gPos, mapChip, params, maxAStarNodes);
                        }
                        ImGui::PopStyleColor();
                        if (isSearching) {
                            ImGui::EndDisabled();
                        }

                        const auto& statusMsg = replayMgrInst->GetAIPathStatusMsg();
                        if (!statusMsg.empty()) {
                            ImGui::Spacing();
                            if (statusMsg.find("[詰み]") != std::string::npos) {
                                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", statusMsg.c_str());
                            } else {
                                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", statusMsg.c_str());
                            }
                        }
                    }
                } else {
                    bool handled = false;
                    if (mapEditor_) {
                        handled = mapEditor_->DrawInspectorUI(sceneManager);
                    }
                    
                    if (!handled) {
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
                    bool oldOutline = outlineEnabled;
                    if (ImGui::Checkbox("アウトラインを有効化", &outlineEnabled)) {
                        dxCommon->SetOutlineEnabled(outlineEnabled);
                        SaveSceneConfig();
                        PushActionCommand([=](){ dxCommon->SetOutlineEnabled(oldOutline); SaveSceneConfig(); }, 
                                          [=](){ dxCommon->SetOutlineEnabled(outlineEnabled); SaveSceneConfig(); });
                    }
                }
                ImGui::Spacing();

                ImGui::Text("グローバル設定 (ライティング)");
                ImGui::Separator();

                if (lightEditor_) {
                    ImGui::Text("現在の設定: %s", lightEditor_->GetCurrentFileName().c_str());
                    ImGui::Text("環境光 (暗闇): %.2f", lightEditor_->GetAmbientIntensity());
                    ImGui::Text("スポットライト数: %zu / %d", lightEditor_->GetSpotLights().size(), kMaxSpotLights);
                    
                    if (ImGui::Button("ライトエディターを開く", ImVec2(-1, 30))) {
                        showLightEditor_ = true;
                        showSpotLightPanel_ = true;
                        activeMainTab_ = "ライトエディター";
                        currentMode_ = EditorMode::Light;
                        focusActiveTabCountdown_ = 5;
                        focusSpotLightTabCountdown_ = 5;
                        SaveSceneConfig();
                    }
                }

                ImGui::Separator();
                bool oldFog = enableFog_;
                if (ImGui::Checkbox("フォグエフェクトを有効化", &enableFog_)) {
                    SaveLightingConfig(modelCommon);
                    bool newFog = enableFog_;
                    PushActionCommand([=](){ enableFog_ = oldFog; SaveLightingConfig(modelCommon); }, 
                                      [=](){ enableFog_ = newFog; SaveLightingConfig(modelCommon); });
                }

                ImGui::Spacing();
                ImGui::Text("グローバル設定 (ゲームカメラ)");
                ImGui::Separator();
                if (gameCamera) {
                    float scale = gameCamera->GetScale();
                    Vector3 rot = gameCamera->GetRotation();
                    float follow = gameCamera->GetFollowLerp();
                    float trans = gameCamera->GetTransitionLerp();

                    // カメラスケール (Zoom)
                    if (TrackActionDragFloat("カメラスケール (Zoom)", &scale, 0.01f, 0.1f, 10.0f, "%.2f", [=](float v){ gameCamera->SetScale(v); SaveSceneConfig(); })) {
                        gameCamera->SetScale(scale);
                    }

                    // カメラ角度 (Rotation) - ラジアンを度数法で表示・編集
                    float rotDeg[3] = { rot.x * 180.0f / 3.14159265f, rot.y * 180.0f / 3.14159265f, rot.z * 180.0f / 3.14159265f };
                    if (TrackActionDragFloat3("カメラ角度 (Rotation)", rotDeg, 0.5f, -180.0f, 180.0f, "%.1f", [=](const Vector3& v){ 
                        Vector3 r = { v.x * 3.14159265f / 180.0f, v.y * 3.14159265f / 180.0f, v.z * 3.14159265f / 180.0f };
                        gameCamera->SetRotation(r); SaveSceneConfig(); 
                    })) {
                        rot.x = rotDeg[0] * 3.14159265f / 180.0f;
                        rot.y = rotDeg[1] * 3.14159265f / 180.0f;
                        rot.z = rotDeg[2] * 3.14159265f / 180.0f;
                        gameCamera->SetRotation(rot);
                    }

                    if (TrackActionDragFloat("追従速度 (FollowLerp)", &follow, 0.005f, 0.0f, 1.0f, "%.3f", [=](float v){ gameCamera->SetFollowLerp(v); SaveSceneConfig(); })) {
                        gameCamera->SetFollowLerp(follow);
                    }
                    if (TrackActionDragFloat("遷移速度 (TransitionLerp)", &trans, 0.005f, 0.0f, 1.0f, "%.3f", [=](float v){ gameCamera->SetTransitionLerp(v); SaveSceneConfig(); })) {
                        gameCamera->SetTransitionLerp(trans);
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("カメラ設定をJSON保存 (Save)", ImVec2(-1, 0))) {
                        gameCamera->SaveConfig();
                    }
                    if (ImGui::Button("カメラ設定をJSON読込 (Load)", ImVec2(-1, 0))) {
                        gameCamera->LoadConfig();
                    }
                } else {
                    ImGui::TextDisabled("※ゲームカメラが有効ではありません。");
                }
                }
                }
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

            bool enablePost = dxCommon->IsPostEffectEnabled();
            if (ImGui::Checkbox("ポストエフェクトを有効化", &enablePost)) {
                dxCommon->SetPostEffectEnabled(enablePost);
                SaveSceneConfig();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ドラッグとキーボード入力（Ctrl+クリック等）が一体化したfloat調整用ヘルパー関数
            auto DrawFloatControl = [&](const char *label, float *val, float minVal, float maxVal, float speed = 0.005f) {
                ImGui::Text("%s", label); // ラベルの描画

                ImGui::PushID(label);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                TrackActionDragFloat("##drag", val, speed, minVal, maxVal, "%.3f", [=](float v){ *val = v; SaveSceneConfig(); });
                ImGui::PopItemWidth();
                ImGui::PopID();
            };

            // ドラッグとキーボード入力が一体化したint調整用ヘルパー関数
            auto DrawIntControl = [&](const char *label, int *val, int minVal, int maxVal, float speed = 0.05f) {
                ImGui::Text("%s", label);

                ImGui::PushID(label);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                TrackActionDragInt("##drag", val, speed, minVal, maxVal, [=](int v){ *val = v; SaveSceneConfig(); });
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
            if (params && dxCommon->IsPostEffectEnabled()) {
                if (ImGui::CollapsingHeader("深度ベース・アウトライン設定 (Outline)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing();
                    bool enableOutline = dxCommon->IsDepthBasedOutlineEnabled();
                    if (ImGui::Checkbox("アウトラインを有効化", &enableOutline)) {
                        dxCommon->SetDepthBasedOutlineEnabled(enableOutline);
                        SaveSceneConfig();
                    }
                    ImGui::Spacing();
                }
                ImGui::Spacing();

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
    if (showMapEditor_ && mapEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "マップチップ画面") {
            ImGui::SetNextWindowFocus();
        }
        mapEditor_->SetVisible(showMapEditor_);
        mapEditor_->DrawCanvas(sceneManager, activeCamera, renderTextureSrvHandle, [&]() {
            currentMode_ = EditorMode::Normal;
            if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
                sceneManager->PopScene();
                if (animationEditor_) animationEditor_->SetAnimScenePushed(false);
            }
            if (focusActiveTabCountdown_ == 0 && activeMainTab_ != "マップチップ画面") {
                activeMainTab_ = "マップチップ画面";
                SaveSceneConfig();
            }
        });
        showMapEditor_ = mapEditor_->IsVisible();
    }

    // --- 下部ペイン (通常: マップ設定 / リプレイ時: タイムライン / アニメーション時: ドープシート / ライト時: スポットライトとログのみ) ---
    if (currentMode_ == EditorMode::Light || activeMainTab_ == "ライトエディター") {
        // ライトエディター時は下部に「スポットライト」を表示
        if (lightEditor_) {
            if (focusSpotLightTabCountdown_ > 0) {
                ImGui::SetNextWindowFocus();
                focusSpotLightTabCountdown_--;
            }
            if (ImGui::Begin("スポットライト", &showSpotLightPanel_)) {
                lightEditor_->DrawLightEditorUI(modelCommon);
            }
            ImGui::End();
        }
    } else if (currentMode_ == EditorMode::Animation) {
        if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->DrawDopeSheetUI(sceneManager); }
    } else if (currentMode_ == EditorMode::Replay) {
        if (ImGui::Begin("タイムライン", &showMapSettings_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                auto replayMgr = ReplayManager::GetInstance();
                const auto& currentReplay = replayMgr->GetCurrentReplay();

                int currentFrame = replayMgr->GetCurrentFrame();
                int totalFrames = currentReplay.totalFrames;
                if (totalFrames <= 0 && replayMgr->IsRecording()) {
                    totalFrames = replayMgr->GetRecordedFrameCount();
                }
                if (totalFrames <= 0) totalFrames = 1;

                // 1. コントロールバー (ヘッダー)
                // コマ戻し (1F)
                if (ImGui::Button("|<", ImVec2(32, 28))) {
                    if (currentFrame > 0) {
                        replayMgr->SetCurrentFrame(currentFrame - 1);
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム戻る");
                ImGui::SameLine();

                // 再生 / 一時停止 (Clipchamp風パープルボタン: 保存されている元データを再生)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.22f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.32f, 1.00f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.15f, 0.80f, 1.0f));
                if (replayMgr->IsPlaying() && !replayMgr->IsPaused()) {
                    if (ImGui::Button("一時停止", ImVec2(90, 28))) {
                        replayMgr->PausePlayback();
                    }
                } else {
                    if (ImGui::Button("再生", ImVec2(90, 28))) {
                        replayMgr->SetSnapEnabled(true);
                        replayMgr->SetInterpolationEnabled(true);
                        const auto& history = replayMgr->GetHistory();
                        if (!history.empty()) {
                            replayMgr->StartPlayback(0);
                            if (history[0].mapDataStr != "") {
                                this->mapDataStrToLoad_ = history[0].mapDataStr;
                                this->loadMapDataStrNextFrame_ = true;
                            }
                        } else {
                            const auto& saved = replayMgr->GetSavedList();
                            if (!saved.empty()) {
                                replayMgr->StartPlayback(-1, saved[0]);
                            }
                        }
                    }
                }
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("保存されている元のリプレイデータを再生します (位置補正・補間ON)");
                ImGui::SameLine();

                // コマ送り (1F)
                if (ImGui::Button(">|", ImVec2(32, 28))) {
                    if (currentFrame < totalFrames) {
                        replayMgr->SetCurrentFrame(currentFrame + 1);
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム進む");
                ImGui::SameLine();

                // 停止
                if (ImGui::Button("停止", ImVec2(65, 28))) {
                    replayMgr->StopPlayback();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生を停止します");
                ImGui::SameLine();

                // 変更内容を再生 (グリーン系ボタン: 編集されたcurrentReplay_を再生)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.34f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.68f, 0.42f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.45f, 0.27f, 1.0f));
                if (ImGui::Button("変更内容を再生", ImVec2(130, 28))) {
                    replayMgr->SetSnapEnabled(false);
                    replayMgr->SetInterpolationEnabled(false);
                    if (replayMgr->IsPaused()) {
                        replayMgr->ResumePlayback();
                    } else {
                        replayMgr->StartPlayback(-1, "");
                        const auto& currentReplay = replayMgr->GetCurrentReplay();
                        if (currentReplay.mapDataStr != "") {
                            this->mapDataStrToLoad_ = currentReplay.mapDataStr;
                            this->loadMapDataStrNextFrame_ = true;
                        }
                    }
                }
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("タイムラインで変更した内容（移動の長さ変更等）を適用して再生します（位置補正・補間OFF）");
                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();

                // デジタルタイム表示 (Clipchampスタイル)
                float currentSec = currentFrame / 60.0f;
                float totalSec = totalFrames / 60.0f;
                int curMin = (int)currentSec / 60;
                float curS = fmod(currentSec, 60.0f);
                int totMin = (int)totalSec / 60;
                float totS = fmod(totalSec, 60.0f);

                ImGui::TextColored(ImVec4(0.88f, 0.80f, 1.0f, 1.0f), "%02d:%05.2f / %02d:%05.2f  (%d / %d F)",
                    curMin, curS, totMin, totS, currentFrame, totalFrames);

                ImGui::SameLine();
                // ズーム倍率テキスト表示 & 引き継ぎボタン
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 240.0f);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 1.0f, 1.0f), "ズーム: %.1fx", timelineZoom_);

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button("操作引き継ぎ", ImVec2(110, 28))) {
                    replayMgr->TakeoverPlayback();
                    takeoverCountdown_ = 3.0f;
                }
                ImGui::PopStyleColor();

                ImGui::Separator();

                // 2. タイムラインルーラー & ビジュアルトラック描画エリア
                const float trackHeaderWidth = 130.0f; // トラック名表示エリアの幅
                const float rulerHeight = 28.0f;       // ルーラーエリアの高さ
                const float trackHeight = 22.0f;      // 1トラックあたりの高さ
                const float trackPadding = 4.0f;
                const int numTracks = 7;
                const char* trackNames[7] = {
                    "左移動 (L)",
                    "右移動 (R)",
                    "上移動 (W)",
                    "下移動 (S)",
                    "ジャンプ (J)",
                    "ダッシュ (D)",
                    "壁張り付き (C)"
                };
                const ImU32 trackColors[7] = {
                    IM_COL32(76, 175, 80, 220),   // 左: グリーン
                    IM_COL32(139, 195, 74, 220),  // 右: ライトグリーン
                    IM_COL32(255, 235, 59, 220),  // 上: イエロー
                    IM_COL32(0, 188, 212, 220),   // 下: シアン
                    IM_COL32(33, 150, 243, 220),  // ジャンプ: ブルー
                    IM_COL32(255, 152, 0, 220),   // ダッシュ: オレンジ
                    IM_COL32(156, 39, 176, 220)   // 壁張り付き: パープル
                };

                float timelineWidth = (float)totalFrames * timelineZoom_;
                float contentWidth = trackHeaderWidth + timelineWidth + 50.0f;
                float totalCanvasHeight = rulerHeight + (trackHeight + trackPadding) * numTracks + 6.0f;

                // スクロール可能なChild Windowとしてキャンバスを作成（縦スクロールバーを完全に除去）
                ImGui::SetNextWindowContentSize(ImVec2(contentWidth, totalCanvasHeight));
                ImGui::BeginChild("TimelineScrollChild", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);

                // マウスホイールでのズーム & Ctrl+ホイールでの左右スクロール
                ImGuiIO& io = ImGui::GetIO();
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                    if (io.MouseWheel != 0.0f) {
                        if (io.KeyCtrl) {
                            // Ctrl + マウスホイール -> 左右スクロール移動
                            float scrollX = ImGui::GetScrollX();
                            ImGui::SetScrollX(scrollX - io.MouseWheel * 60.0f);
                        } else {
                            // 通常のマウスホイール -> ズーム倍率変更
                            timelineZoom_ += io.MouseWheel * 0.5f;
                            if (timelineZoom_ < 1.0f) timelineZoom_ = 1.0f;
                            if (timelineZoom_ > 30.0f) timelineZoom_ = 30.0f;
                        }
                    }
                }

                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                float scrollX = ImGui::GetScrollX();
                float headerLeftX = p0.x + scrollX; // 画面左端に固定表示するためのX座標

                // 全体背景
                drawList->AddRectFilled(p0, ImVec2(p0.x + contentWidth, p0.y + totalCanvasHeight), IM_COL32(20, 18, 28, 255));

                // 2-a. ルーラーを描画 (Top 0 ~ rulerHeight)
                ImVec2 rulerTopLeft = ImVec2(p0.x + trackHeaderWidth, p0.y);
                ImVec2 rulerBottomRight = ImVec2(p0.x + trackHeaderWidth + timelineWidth, p0.y + rulerHeight);
                ImVec2 mousePos = ImGui::GetMousePos();
                bool isChildHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
                bool mouseInRuler = (mousePos.x >= rulerTopLeft.x && mousePos.x <= rulerBottomRight.x &&
                                     mousePos.y >= rulerTopLeft.y && mousePos.y <= rulerBottomRight.y);

                // マウスがルーラー領域上にある場合のインタラクション
                if (isChildHovered && mouseInRuler && replayBlockDragMode_ == ReplayBlockDragMode::None) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::SetTooltip("クリック/ドラッグでシーク位置変更");
                    if (ImGui::IsMouseClicked(0)) {
                        isRulerScrubbing_ = true;
                        selectedGameObject_ = nullptr;
                        selectedObject_ = nullptr;
                        selectedParticle_ = nullptr;
                        selectedPrimitive_ = nullptr;
                        selectedReplayBlock_.Clear();
                        if (mapEditor_) mapEditor_->GetContext()->SetSelectedTool(0);
                        selectedReplaySeekbar_ = true;
                    }
                }

                // ルーラードラッグ中のシーク処理
                if (isRulerScrubbing_) {
                    if (ImGui::IsMouseDown(0)) {
                        float relativeX = mousePos.x - (p0.x + trackHeaderWidth);
                        int targetFrame = (int)(relativeX / timelineZoom_ + 0.5f);
                        if (targetFrame < 0) targetFrame = 0;
                        if (targetFrame > totalFrames) targetFrame = totalFrames;
                        replayMgr->SetCurrentFrame(targetFrame);

                        float sec = targetFrame / 60.0f;
                        ImGui::SetTooltip("シーク位置: %02d:%05.2f (%d F)", (int)sec / 60, fmod(sec, 60.0f), targetFrame);
                    } else {
                        isRulerScrubbing_ = false;
                    }
                }

                // ルーラー背景 (ホバー・ドラッグ時は明るくハイライト)
                ImU32 rulerBgColor = (mouseInRuler || isRulerScrubbing_) ? IM_COL32(55, 45, 75, 255) : IM_COL32(35, 30, 50, 255);
                drawList->AddRectFilled(rulerTopLeft, rulerBottomRight, rulerBgColor);
                drawList->AddLine(ImVec2(p0.x, p0.y + rulerHeight), ImVec2(p0.x + contentWidth, p0.y + rulerHeight), IM_COL32(70, 60, 90, 255), 1.5f);

                // ルーラーの目盛り描画 (60F = 1秒ごとにテキスト・長目盛り、ステップごとに短目盛り)
                int stepFrame = (timelineZoom_ >= 8.0f) ? 10 : (timelineZoom_ >= 3.0f ? 30 : 60);
                for (int f = 0; f <= totalFrames; f += stepFrame) {
                    float x = rulerTopLeft.x + f * timelineZoom_;
                    if (f % 60 == 0 || f % stepFrame == 0) {
                        bool isMajor = (f % 60 == 0);
                        float lineH = isMajor ? 12.0f : 7.0f;
                        drawList->AddLine(ImVec2(x, rulerBottomRight.y - lineH), ImVec2(x, rulerBottomRight.y), IM_COL32(180, 170, 210, 200), isMajor ? 1.5f : 1.0f);

                        if (isMajor || timelineZoom_ >= 4.0f) {
                            float sec = f / 60.0f;
                            char timeStr[32];
                            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", (int)sec / 60, (int)sec % 60);
                            drawList->AddText(ImVec2(x + 3.0f, p0.y + 3.0f), IM_COL32(200, 190, 230, 230), timeStr);
                        }
                    }
                }

                // 2-b. トラック本体背景 & MMLキー入力ブロックの描画
                const auto& frames = currentReplay.frames;
                for (int t = 0; t < numTracks; t++) {
                    float trackY = p0.y + rulerHeight + t * (trackHeight + trackPadding) + 4.0f;

                    // トラック本体背景
                    ImVec2 trackBgMin = ImVec2(p0.x + trackHeaderWidth, trackY);
                    ImVec2 trackBgMax = ImVec2(p0.x + trackHeaderWidth + timelineWidth, trackY + trackHeight);
                    drawList->AddRectFilled(trackBgMin, trackBgMax, IM_COL32(28, 25, 38, 255), 3.0f);
                    drawList->AddRect(trackBgMin, trackBgMax, IM_COL32(50, 45, 65, 180), 3.0f);

                    // フレームデータが存在する場合、アクティブ区間をビジュアルブロックとして描画
                    if (!frames.empty()) {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        bool isChildHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

                        auto RenderAndHandleBlock = [&](int trackIdx, int blockStart, int blockEnd) {
                            float bMinX = trackBgMin.x + blockStart * timelineZoom_;
                            float bMaxX = trackBgMin.x + blockEnd * timelineZoom_;
                            if (bMaxX - bMinX < 4.0f) bMaxX = bMinX + 4.0f; // 最小幅確保

                            float blockMinY = trackY + 2.0f;
                            float blockMaxY = trackY + trackHeight - 2.0f;

                            bool isSelected = selectedReplayBlock_.Equals(trackIdx, blockStart, blockEnd);

                            // ヒットテスト (マウス位置判定)
                            bool mouseInY = (mousePos.y >= blockMinY && mousePos.y <= blockMaxY);
                            bool mouseInX = (mousePos.x >= bMinX && mousePos.x <= bMaxX);
                            bool mouseHoverBlock = mouseInY && mouseInX;

                            // 端の判定 (左右6pxの範囲)
                            bool nearLeft = mouseInY && (std::abs(mousePos.x - bMinX) <= 6.0f);
                            bool nearRight = mouseInY && (std::abs(mousePos.x - bMaxX) <= 6.0f);

                            // ホバー時のインタラクション・カーソル変更
                            if (isChildHovered && replayBlockDragMode_ == ReplayBlockDragMode::None) {
                                if (nearLeft || nearRight) {
                                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                                    if (nearLeft) ImGui::SetTooltip("左端ドラッグ: 開始位置を変更");
                                    else if (nearRight) ImGui::SetTooltip("右端ドラッグ: 長さを変更");
                                } else if (mouseHoverBlock) {
                                    ImGui::SetTooltip("トラック: %s\n開始: %df ~ 終了: %df\n長さ: %df (%.2f秒)",
                                        trackNames[trackIdx], blockStart, blockEnd, blockEnd - blockStart, (blockEnd - blockStart) / 60.0f);
                                }

                                // クリック検出 -> 選択 & ドラッグ予備状態設定
                                if ((nearLeft || nearRight || mouseHoverBlock) && ImGui::IsMouseClicked(0)) {
                                    selectedReplayBlock_ = { trackIdx, blockStart, blockEnd };
                                    draggingBlockOriginal_ = selectedReplayBlock_;
                                    dragStartMouseFrame_ = (int)((mousePos.x - (p0.x + trackHeaderWidth)) / timelineZoom_);
                                    dragStartMousePos_ = mousePos;
                                    replayDragOldReplayData_ = ReplayManager::GetInstance()->GetCurrentReplay();
                                    replayDragOldSelectedBlock_ = selectedReplayBlock_;
                                    replayBlockDragMode_ = ReplayBlockDragMode::None;

                                    if (nearLeft) pendingBlockDragMode_ = ReplayBlockDragMode::ResizeLeft;
                                    else if (nearRight) pendingBlockDragMode_ = ReplayBlockDragMode::ResizeRight;
                                    else pendingBlockDragMode_ = ReplayBlockDragMode::Move;
                                }
                            }

                            // 描画: ノード本体
                            drawList->AddRectFilled(ImVec2(bMinX, blockMinY), ImVec2(bMaxX, blockMaxY), trackColors[trackIdx], 3.0f);
                            drawList->AddRect(ImVec2(bMinX, blockMinY), ImVec2(bMaxX, blockMaxY), IM_COL32(255, 255, 255, mouseHoverBlock ? 220 : 120), 3.0f);

                            // 選択時 (Clipchamp風白枠ハイライト & 左右ハンドルつまみ)
                            if (isSelected) {
                                // 太い白枠ハイライト
                                drawList->AddRect(ImVec2(bMinX - 1.0f, blockMinY - 1.0f), ImVec2(bMaxX + 1.0f, blockMaxY + 1.0f), IM_COL32(255, 255, 255, 255), 3.0f, 0, 2.0f);
                                drawList->AddRectFilled(ImVec2(bMinX, blockMinY), ImVec2(bMaxX, blockMinY + 4.0f), IM_COL32(255, 255, 255, 90), 3.0f, ImDrawFlags_RoundCornersTop);

                                // 左端白いハンドル
                                drawList->AddRectFilled(ImVec2(bMinX - 4.0f, blockMinY - 1.0f), ImVec2(bMinX + 3.0f, blockMaxY + 1.0f), IM_COL32(255, 255, 255, 255), 3.0f);
                                drawList->AddRect(ImVec2(bMinX - 4.0f, blockMinY - 1.0f), ImVec2(bMinX + 3.0f, blockMaxY + 1.0f), IM_COL32(40, 40, 60, 255), 3.0f);
                                drawList->AddLine(ImVec2(bMinX - 1.0f, blockMinY + 4.0f), ImVec2(bMinX - 1.0f, blockMaxY - 4.0f), IM_COL32(100, 100, 120, 255), 1.0f);

                                // 右端白いハンドル
                                drawList->AddRectFilled(ImVec2(bMaxX - 3.0f, blockMinY - 1.0f), ImVec2(bMaxX + 4.0f, blockMaxY + 1.0f), IM_COL32(255, 255, 255, 255), 3.0f);
                                drawList->AddRect(ImVec2(bMaxX - 3.0f, blockMinY - 1.0f), ImVec2(bMaxX + 4.0f, blockMaxY + 1.0f), IM_COL32(40, 40, 60, 255), 3.0f);
                                drawList->AddLine(ImVec2(bMaxX + 1.0f, blockMinY + 4.0f), ImVec2(bMaxX + 1.0f, blockMaxY - 4.0f), IM_COL32(100, 100, 120, 255), 1.0f);
                            }
                        };

                        int blockStart = -1;
                        for (int f = 0; f < (int)frames.size() && f <= totalFrames; f++) {
                            const char* keys = frames[f].keys;
                            bool isActive = false;
                            if (t == 0) isActive = (keys[0] == 'L');
                            else if (t == 1) isActive = (keys[1] == 'R');
                            else if (t == 2) isActive = (keys[5] == 'W');
                            else if (t == 3) isActive = (keys[6] == 'S');
                            else if (t == 4) isActive = (keys[2] == 'J');
                            else if (t == 5) isActive = (keys[3] == 'D');
                            else if (t == 6) isActive = (keys[4] == 'C');

                            if (isActive) {
                                if (blockStart == -1) blockStart = f;
                            } else {
                                if (blockStart != -1) {
                                    RenderAndHandleBlock(t, blockStart, f);
                                    blockStart = -1;
                                }
                            }
                        }
                        if (blockStart != -1) {
                            RenderAndHandleBlock(t, blockStart, (int)frames.size());
                        }
                    }
                }

                // 2-c. シーク指示線（プレイヘッド垂直線 & 紫丸ノブ）
                float playheadX = p0.x + trackHeaderWidth + currentFrame * timelineZoom_;

                // 垂直ライン (全トラックを縦断)
                drawList->AddLine(
                    ImVec2(playheadX, p0.y),
                    ImVec2(playheadX, p0.y + totalCanvasHeight),
                    IM_COL32(255, 255, 255, 240),
                    2.0f
                );
                // 紫色のシャドウ・光彩
                drawList->AddLine(
                    ImVec2(playheadX - 1.0f, p0.y),
                    ImVec2(playheadX - 1.0f, p0.y + totalCanvasHeight),
                    IM_COL32(160, 32, 240, 120),
                    4.0f
                );

                // ルーラー上部の丸型ピン（Clipchampヘッド）
                drawList->AddCircleFilled(ImVec2(playheadX, p0.y + 10.0f), 8.0f, IM_COL32(160, 32, 240, 255));
                drawList->AddCircleFilled(ImVec2(playheadX, p0.y + 10.0f), 5.0f, IM_COL32(255, 255, 255, 255));
                drawList->AddCircle(ImVec2(playheadX, p0.y + 10.0f), 8.5f, IM_COL32(255, 255, 255, 200), 0, 1.5f);

                // 2-d. トラックヘッダーの画面左端固定描画 (Sticky Column)
                // 左上ルーラー交差部のマスク背景
                drawList->AddRectFilled(ImVec2(headerLeftX, p0.y), ImVec2(headerLeftX + trackHeaderWidth, p0.y + rulerHeight), IM_COL32(30, 25, 45, 255));
                drawList->AddLine(ImVec2(headerLeftX + trackHeaderWidth - 1.0f, p0.y), ImVec2(headerLeftX + trackHeaderWidth - 1.0f, p0.y + totalCanvasHeight), IM_COL32(60, 50, 80, 255), 1.5f);

                // 各トラック名ラベルをスクロール固定位置に最前面描画
                for (int t = 0; t < numTracks; t++) {
                    float trackY = p0.y + rulerHeight + t * (trackHeight + trackPadding) + 4.0f;
                    drawList->AddRectFilled(ImVec2(headerLeftX + 2.0f, trackY), ImVec2(headerLeftX + trackHeaderWidth - 5.0f, trackY + trackHeight), IM_COL32(40, 35, 55, 255), 4.0f);
                    drawList->AddRect(ImVec2(headerLeftX + 2.0f, trackY), ImVec2(headerLeftX + trackHeaderWidth - 5.0f, trackY + trackHeight), IM_COL32(65, 55, 85, 200), 4.0f);
                    drawList->AddText(ImVec2(headerLeftX + 10.0f, trackY + 3.0f), IM_COL32(230, 225, 250, 255), trackNames[t]);
                }

                // 2-d. ドラッグ閾値判定 & ドラッグ中のノード更新
                if (pendingBlockDragMode_ != ReplayBlockDragMode::None && replayBlockDragMode_ == ReplayBlockDragMode::None) {
                    if (ImGui::IsMouseDown(0)) {
                        float dist = std::hypot(mousePos.x - dragStartMousePos_.x, mousePos.y - dragStartMousePos_.y);
                        if (dist >= 4.0f) { // 4ピクセル以上でドラッグモードへ昇格
                            replayBlockDragMode_ = pendingBlockDragMode_;
                            pendingBlockDragMode_ = ReplayBlockDragMode::None;
                        }
                    } else {
                        pendingBlockDragMode_ = ReplayBlockDragMode::None;
                    }
                }
                if (replayBlockDragMode_ != ReplayBlockDragMode::None) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    int currentMouseFrame = (int)((mousePos.x - (p0.x + trackHeaderWidth)) / timelineZoom_ + 0.5f);
                    if (currentMouseFrame < 0) currentMouseFrame = 0;

                    int frameDelta = currentMouseFrame - dragStartMouseFrame_;

                    int pStart = draggingBlockOriginal_.startFrame;
                    int pEnd = draggingBlockOriginal_.endFrame;

                    if (replayBlockDragMode_ == ReplayBlockDragMode::Move) {
                        int dur = draggingBlockOriginal_.endFrame - draggingBlockOriginal_.startFrame;
                        pStart = (std::max)(0, draggingBlockOriginal_.startFrame + frameDelta);
                        pEnd = pStart + dur;
                    } else if (replayBlockDragMode_ == ReplayBlockDragMode::ResizeLeft) {
                        pStart = std::clamp(draggingBlockOriginal_.startFrame + frameDelta, 0, draggingBlockOriginal_.endFrame - 1);
                        pEnd = draggingBlockOriginal_.endFrame;
                    } else if (replayBlockDragMode_ == ReplayBlockDragMode::ResizeRight) {
                        pStart = draggingBlockOriginal_.startFrame;
                        pEnd = (std::max)(draggingBlockOriginal_.startFrame + 1, draggingBlockOriginal_.endFrame + frameDelta);
                    }

                    // ドラッグ中のプレビュー描画
                    int t = draggingBlockOriginal_.trackIdx;
                    float trackY = p0.y + rulerHeight + t * (trackHeight + trackPadding) + 4.0f;
                    float prevMinX = p0.x + trackHeaderWidth + pStart * timelineZoom_;
                    float prevMaxX = p0.x + trackHeaderWidth + pEnd * timelineZoom_;
                    if (prevMaxX - prevMinX < 4.0f) prevMaxX = prevMinX + 4.0f;

                    drawList->AddRectFilled(ImVec2(prevMinX, trackY + 2.0f), ImVec2(prevMaxX, trackY + trackHeight - 2.0f), IM_COL32(255, 255, 255, 90), 3.0f);
                    drawList->AddRect(ImVec2(prevMinX - 1.0f, trackY + 1.0f), ImVec2(prevMaxX + 1.0f, trackY + trackHeight - 1.0f), IM_COL32(255, 235, 59, 255), 3.0f, 0, 2.0f);

                    ImGui::SetTooltip("移動/長さ変更中\n開始: %df ~ 終了: %df\n長さ: %df (%.2f秒)",
                        pStart, pEnd, pEnd - pStart, (pEnd - pStart) / 60.0f);

                    if (ImGui::IsMouseReleased(0)) {
                        ReplayData oldData = replayDragOldReplayData_;
                        SelectedReplayBlock oldBlock = replayDragOldSelectedBlock_;

                        ReplayManager::GetInstance()->ModifyBlockRange(
                            draggingBlockOriginal_.trackIdx,
                            draggingBlockOriginal_.startFrame,
                            draggingBlockOriginal_.endFrame,
                            pStart,
                            pEnd
                        );
                        selectedReplayBlock_ = { draggingBlockOriginal_.trackIdx, pStart, pEnd };

                        ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
                        SelectedReplayBlock newBlock = selectedReplayBlock_;

                        PushActionCommand(
                            [oldData, oldBlock, this]() {
                                auto replayMgr = ReplayManager::GetInstance();
                                replayMgr->GetCurrentReplay() = oldData;
                                replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                                this->selectedReplayBlock_ = oldBlock;
                            },
                            [newData, newBlock, this]() {
                                auto replayMgr = ReplayManager::GetInstance();
                                replayMgr->GetCurrentReplay() = newData;
                                replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                                this->selectedReplayBlock_ = newBlock;
                            }
                        );

                        replayBlockDragMode_ = ReplayBlockDragMode::None;
                    }
                }

                // Deleteキーによる選択ノード削除ショートカット
                if (selectedReplayBlock_.IsValid() && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                    ReplayData oldData = ReplayManager::GetInstance()->GetCurrentReplay();
                    SelectedReplayBlock oldBlock = selectedReplayBlock_;

                    ReplayManager::GetInstance()->DeleteBlockRange(
                        selectedReplayBlock_.trackIdx,
                        selectedReplayBlock_.startFrame,
                        selectedReplayBlock_.endFrame
                    );
                    selectedReplayBlock_.Clear();

                    ReplayData newData = ReplayManager::GetInstance()->GetCurrentReplay();
                    SelectedReplayBlock newBlock = selectedReplayBlock_;

                    PushActionCommand(
                        [oldData, oldBlock, this]() {
                            auto replayMgr = ReplayManager::GetInstance();
                            replayMgr->GetCurrentReplay() = oldData;
                            replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                            this->selectedReplayBlock_ = oldBlock;
                        },
                        [newData, newBlock, this]() {
                            auto replayMgr = ReplayManager::GetInstance();
                            replayMgr->GetCurrentReplay() = newData;
                            replayMgr->RebuildMmlFromFrames(replayMgr->GetCurrentReplay());
                            this->selectedReplayBlock_ = newBlock;
                        }
                    );
                }

                // リプレイエディター上での Undo (Ctrl+Z) / Redo (Ctrl+Y or Ctrl+Shift+Z)
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                    bool ctrl = io.KeyCtrl;
                    bool shift = io.KeyShift;
                    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !shift) {
                        Undo();
                    }
                    if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) {
                        Redo();
                    }
                }

                // 2-e. Ctrlドラッグによる横スクロール & 空き領域クリックでのノード選択解除
                ImGui::SetCursorScreenPos(p0);
                ImGui::InvisibleButton("##TimelineCanvasButton", ImVec2(contentWidth, totalCanvasHeight));
                if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    if (io.KeyCtrl) {
                        // Ctrl + ドラッグで左右移動 (スクロール)
                        float scrollX = ImGui::GetScrollX();
                        ImGui::SetScrollX(scrollX - io.MouseDelta.x);
                    }
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyCtrl) {
                    if (replayBlockDragMode_ == ReplayBlockDragMode::None && pendingBlockDragMode_ == ReplayBlockDragMode::None) {
                        selectedReplayBlock_.Clear();
                    }
                }

                ImGui::EndChild();
            }
            ImGui::End();
        } else if (currentMode_ == EditorMode::ModelPlacement || activeMainTab_ == "3Dモデル配置") {
            if (model3DEditor_) {
                model3DEditor_->DrawPalette(showModelPalette_, sceneManager);
            }
        } else {
            if (showMapSettings_ && mapEditor_) {
                mapEditor_->DrawSettingsUI(sceneManager, showMapSettings_, [&](){ SaveSceneConfig(); }, [&](){
                    selectedGameObject_ = nullptr;
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                });
            }
        }

    if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->UpdateAnimationPosePreview(sceneManager); }

    LogManager::GetInstance()->Draw();

    // --- レイアウトプリセット保存ウィンドウ ---
    if (showSavePresetWindow_) {
        ImGui::SetNextWindowSize(ImVec2(360, 150), ImGuiCond_Appearing);
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (vp) {
            ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f - 180.0f, vp->Pos.y + vp->Size.y * 0.5f - 75.0f), ImGuiCond_Appearing);
        }
        ImGui::SetNextWindowFocus();
        if (ImGui::Begin("レイアウトプリセットの保存", &showSavePresetWindow_, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("現在の配置をプリセットとして保存します。");
            ImGui::Spacing();
            ImGui::Text("プリセット名:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.0f);
            bool enterPressed = ImGui::InputText("##PresetNameDialogInput", newPresetNameBuf_, sizeof(newPresetNameBuf_), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("保存", ImVec2(100, 0)) || (enterPressed && strlen(newPresetNameBuf_) > 0)) {
                if (strlen(newPresetNameBuf_) > 0) {
                    SaveLayoutPreset(newPresetNameBuf_);
                    newPresetNameBuf_[0] = '\0';
                    showSavePresetWindow_ = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(100, 0))) {
                showSavePresetWindow_ = false;
            }
        }
        ImGui::End();
    }

    // 3Dモデル配置エディター用 Undo (Ctrl+Z) / Redo (Ctrl+Y or Ctrl+Shift+Z)
    if (activeMainTab_ == "3Dモデル配置" || showModelPlacementEditor_) {
        ImGuiIO& io = ImGui::GetIO();
        if (model3DEditor_ && !io.WantTextInput) {
            bool ctrl = io.KeyCtrl;
            bool shift = io.KeyShift;
            if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                model3DEditor_->GetContext()->Undo();
            }
            if (((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))) {
                model3DEditor_->GetContext()->Redo();
            }
        }
    }

    // 起動時のアクティブタブ復元
    if (focusActiveTabCountdown_ > 0) {
        if (!activeMainTab_.empty()) {
            ImGui::SetWindowFocus(activeMainTab_.c_str());
        }
        focusActiveTabCountdown_--;
    }
}

void EditorManager::Draw() {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectXCommon::GetInstance()->GetCommandList());
}

void EditorManager::Draw3D() {
    if (model3DEditor_) {
        model3DEditor_->Draw();
    }
}

void EditorManager::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorManager::SaveSceneConfig() {
    std::filesystem::create_directories("resources/json/local");
    std::ofstream ofs("resources/json/local/editor_config.json");
    if (ofs.is_open()) {
        auto dxCommon = DirectXCommon::GetInstance();
        nlohmann::json j;
        j["currentScene"] = SceneFactory::GetSceneTypeName(currentSceneType_);
        j["isOutlineEnabled"] = dxCommon ? dxCommon->IsOutlineEnabled() : false;
        j["outlineThickness"] = dxCommon ? dxCommon->GetOutlineThickness() : 0.015f;

        // アクティブメインタブ
        j["activeMainTab"] = activeMainTab_;

        // 現在のマップファイル名
        j["currentMapFile"] = stageFilename_;

        // 現在の3Dモデル配置JSONファイルパス
        if (model3DEditor_ && model3DEditor_->GetContext()) {
            j["currentPlacedModelsFile"] = model3DEditor_->GetContext()->GetCurrentFilePath();
        }

        // 現在のライティングJSONファイルパス
        if (lightEditor_) {
            j["currentLightingFile"] = lightEditor_->GetCurrentFilePath();
        }

        // 各ウィンドウの開閉状態
        nlohmann::json winObj;
        winObj["showInspector"] = showInspector_;
        winObj["showHierarchy"] = showHierarchy_;
        winObj["showGameView"] = showGameView_;
        winObj["showPostEffect"] = showPostEffect_;
        winObj["showMapEditor"] = showMapEditor_;
        winObj["showMapSettings"] = showMapSettings_;
        winObj["showReplayEditor"] = showReplayEditor_;
        winObj["showAnimEditor"] = showAnimEditor_;
        winObj["showLightEditor"] = showLightEditor_;
        winObj["showSpotLightPanel"] = showSpotLightPanel_;
        winObj["showModelPlacementEditor"] = showModelPlacementEditor_;
        winObj["showModelPalette"] = showModelPalette_;
        j["windows"] = winObj;

        ofs << j.dump(4) << std::endl;
        ofs.close();
    }
}

void EditorManager::LoadSceneConfig() {
    std::ifstream ifs("resources/json/local/editor_config.json");
    if (!ifs.is_open()) {
        return;
    }

    try {
        nlohmann::json j;
        ifs >> j;
        ifs.close();

        if (j.contains("currentScene") && j["currentScene"].is_string()) {
            std::string sceneName = j["currentScene"].get<std::string>();
            currentSceneType_ = SceneFactory::GetSceneTypeFromName(sceneName);
        }

        auto dxCommon = DirectXCommon::GetInstance();
        if (dxCommon) {
            if (j.contains("isOutlineEnabled") && j["isOutlineEnabled"].is_boolean()) {
                dxCommon->SetOutlineEnabled(j["isOutlineEnabled"].get<bool>());
            }
            if (j.contains("outlineThickness") && j["outlineThickness"].is_number()) {
                dxCommon->SetOutlineThickness(j["outlineThickness"].get<float>());
            }
        }

        if (j.contains("activeMainTab") && j["activeMainTab"].is_string()) {
            activeMainTab_ = j["activeMainTab"].get<std::string>();
            focusActiveTabCountdown_ = 5; // 起動後最初の数フレームで確実にフォーカスを当てる
            if (activeMainTab_ == "リプレイエディター") {
                currentMode_ = EditorMode::Replay;
            } else if (activeMainTab_ == "アニメーションエディター") {
                currentMode_ = EditorMode::Animation;
            } else if (activeMainTab_ == "ライトエディター") {
                currentMode_ = EditorMode::Light;
            } else if (activeMainTab_ == "3Dモデル配置") {
                currentMode_ = EditorMode::ModelPlacement;
            } else {
                currentMode_ = EditorMode::Normal;
            }
        }

        if (j.contains("currentMapFile") && j["currentMapFile"].is_string()) {
            std::string mapFile = j["currentMapFile"].get<std::string>();
            if (!mapFile.empty()) {
                strcpy_s(stageFilename_, sizeof(stageFilename_), mapFile.c_str());
            }
        }

        if (j.contains("currentPlacedModelsFile") && j["currentPlacedModelsFile"].is_string()) {
            std::string modelFile = j["currentPlacedModelsFile"].get<std::string>();
            if (!modelFile.empty() && model3DEditor_ && model3DEditor_->GetContext()) {
                model3DEditor_->GetContext()->SetCurrentFilePath(modelFile);
                if (std::filesystem::exists(model3DEditor_->GetContext()->GetCurrentFilePath())) {
                    model3DEditor_->GetContext()->LoadFromFile(model3DEditor_->GetContext()->GetCurrentFilePath());
                }
            }
        }

        if (j.contains("currentLightingFile") && j["currentLightingFile"].is_string()) {
            std::string lightFile = j["currentLightingFile"].get<std::string>();
            if (!lightFile.empty() && lightEditor_) {
                lightEditor_->SetCurrentFilePath(lightFile);
                if (std::filesystem::exists(lightEditor_->GetCurrentFilePath())) {
                    lightEditor_->LoadFromFile(lightEditor_->GetCurrentFilePath());
                }
            }
        }

        if (j.contains("windows") && j["windows"].is_object()) {
            const auto& winObj = j["windows"];
            if (winObj.contains("showInspector") && winObj["showInspector"].is_boolean()) showInspector_ = winObj["showInspector"].get<bool>();
            if (winObj.contains("showHierarchy") && winObj["showHierarchy"].is_boolean()) showHierarchy_ = winObj["showHierarchy"].get<bool>();
            if (winObj.contains("showGameView") && winObj["showGameView"].is_boolean()) showGameView_ = winObj["showGameView"].get<bool>();
            if (winObj.contains("showPostEffect") && winObj["showPostEffect"].is_boolean()) showPostEffect_ = winObj["showPostEffect"].get<bool>();
            if (winObj.contains("showMapEditor") && winObj["showMapEditor"].is_boolean()) showMapEditor_ = winObj["showMapEditor"].get<bool>();
            if (winObj.contains("showMapSettings") && winObj["showMapSettings"].is_boolean()) showMapSettings_ = winObj["showMapSettings"].get<bool>();
            if (winObj.contains("showReplayEditor") && winObj["showReplayEditor"].is_boolean()) showReplayEditor_ = winObj["showReplayEditor"].get<bool>();
            if (winObj.contains("showAnimEditor") && winObj["showAnimEditor"].is_boolean()) showAnimEditor_ = winObj["showAnimEditor"].get<bool>();
            if (winObj.contains("showLightEditor") && winObj["showLightEditor"].is_boolean()) showLightEditor_ = winObj["showLightEditor"].get<bool>();
            if (winObj.contains("showSpotLightPanel") && winObj["showSpotLightPanel"].is_boolean()) showSpotLightPanel_ = winObj["showSpotLightPanel"].get<bool>();
            if (winObj.contains("showModelPlacementEditor") && winObj["showModelPlacementEditor"].is_boolean()) showModelPlacementEditor_ = winObj["showModelPlacementEditor"].get<bool>();
            if (winObj.contains("showModelPalette") && winObj["showModelPalette"].is_boolean()) showModelPalette_ = winObj["showModelPalette"].get<bool>();
        }
    } catch (...) {
        // パースエラー時は何もしない
    }
}

void EditorManager::SaveLightingConfig(ModelCommon* modelCommon) {
    if (lightEditor_) {
        lightEditor_->SaveLightingConfig(modelCommon);
    }
}

void EditorManager::LoadLightingConfig(ModelCommon* modelCommon) {
    if (lightEditor_) {
        lightEditor_->LoadLightingConfig(modelCommon);
    }
}

void EditorManager::Undo() {
    if (undoStack_.empty()) return;
    auto cmd = undoStack_.back();
    undoStack_.pop_back();
    cmd->Undo();
    redoStack_.push_back(cmd);
}

void EditorManager::Redo() {
    if (redoStack_.empty()) return;
    auto cmd = redoStack_.back();
    redoStack_.pop_back();
    cmd->Redo();
    undoStack_.push_back(cmd);
}

void EditorManager::UpdateAStarPositionsFromMap(MapChip2D* mapChip, SceneManager* sceneManager) {
    if (mapEditor_) {
        mapEditor_->UpdateAStarPositionsFromMap(mapChip, sceneManager);
    }
}

void EditorManager::ApplyDefaultLayout() {
    showInspector_ = true;
    showHierarchy_ = true;
    showGameView_ = true;
    showPostEffect_ = true;
    showMapEditor_ = true;
    showMapSettings_ = true;
    showReplayEditor_ = true;
    showAnimEditor_ = true;
    showLightEditor_ = true;
    showSpotLightPanel_ = true;
    showModelPlacementEditor_ = true;
    showModelPalette_ = true;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;

    ImGuiID dockspace_id = ImGui::GetID("##DockSpaceOverViewport_0");
    if (dockspace_id == 0) {
        dockspace_id = ImGui::DockSpaceOverViewport(0, viewport);
    }

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID dock_id_main = dockspace_id;
    // 左側に「ヒエラルキー」
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, NULL, &dock_id_main);
    // 右側に「インスペクター」
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, NULL, &dock_id_main);
    // メインの下側に「マップチップ画面」「リプレイマネージャー」など
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.35f, NULL, &dock_id_main);

    // 各ウィンドウを各ノードに割り当てる
    ImGui::DockBuilderDockWindow("ゲームビュー", dock_id_main);
    ImGui::DockBuilderDockWindow("リプレイエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("アニメーションエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("ライトエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_main);
    ImGui::DockBuilderDockWindow("3Dモデル配置", dock_id_main);

    // 左側
    ImGui::DockBuilderDockWindow("ヒエラルキー", dock_id_left);
    ImGui::DockBuilderDockWindow("マイメディア (リプレイ履歴)", dock_id_left);

    // 右側
    ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
    ImGui::DockBuilderDockWindow("ポストエフェクト", dock_id_right);

    // 下側
    ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);
    ImGui::DockBuilderDockWindow("マップ設定", dock_id_bottom);
    ImGui::DockBuilderDockWindow("3Dモデルパレット", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ステージセレクトエディター", dock_id_bottom);
    ImGui::DockBuilderDockWindow("タイムライン", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ドープシート (タイムライン)", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);
    ImGui::DockBuilderDockWindow("スポットライト", dock_id_bottom);

    ImGui::DockBuilderFinish(dockspace_id);

    presetStatusMessage_ = "標準レイアウトにリセットしました";
    presetStatusMessageTimer_ = 3.0f;
}

void EditorManager::ScanLayoutPresets() {
    layoutPresets_.clear();
    std::filesystem::path dirPath("resources/json/local/layout_presets");
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::ifstream ifs(entry.path());
            if (!ifs.is_open()) continue;
            try {
                nlohmann::json j;
                ifs >> j;
                WindowLayoutPreset preset;
                preset.name = j.value("name", entry.path().stem().string());
                preset.iniData = j.value("iniData", "");
                preset.showInspector = j.value("showInspector", true);
                preset.showHierarchy = j.value("showHierarchy", true);
                preset.showGameView = j.value("showGameView", true);
                preset.showPostEffect = j.value("showPostEffect", true);
                preset.showMapEditor = j.value("showMapEditor", true);
                preset.showMapSettings = j.value("showMapSettings", true);
                preset.showReplayEditor = j.value("showReplayEditor", true);
                preset.showAnimEditor = j.value("showAnimEditor", true);
                preset.showLightEditor = j.value("showLightEditor", true);
                preset.showSpotLightPanel = j.value("showSpotLightPanel", true);
                preset.showModelPlacement = j.value("showModelPlacement", true);
                preset.showModelPalette = j.value("showModelPalette", true);

                layoutPresets_.push_back(preset);
            } catch (...) {
                // パース失敗時はスキップ
            }
        }
    }
}

void EditorManager::SaveLayoutPreset(const std::string& name) {
    if (name.empty()) return;

    std::filesystem::path dirPath("resources/json/local/layout_presets");
    if (!std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    size_t iniSize = 0;
    const char* iniStr = ImGui::SaveIniSettingsToMemory(&iniSize);

    WindowLayoutPreset preset;
    preset.name = name;
    preset.iniData = (iniStr && iniSize > 0) ? std::string(iniStr, iniSize) : "";
    preset.showInspector = showInspector_;
    preset.showHierarchy = showHierarchy_;
    preset.showGameView = showGameView_;
    preset.showPostEffect = showPostEffect_;
    preset.showMapEditor = showMapEditor_;
    preset.showMapSettings = showMapSettings_;
    preset.showReplayEditor = showReplayEditor_;
    preset.showAnimEditor = showAnimEditor_;
    preset.showLightEditor = showLightEditor_;
    preset.showSpotLightPanel = showSpotLightPanel_;
    preset.showModelPlacement = showModelPlacementEditor_;
    preset.showModelPalette = showModelPalette_;

    nlohmann::json j;
    j["name"] = preset.name;
    j["iniData"] = preset.iniData;
    j["showInspector"] = preset.showInspector;
    j["showHierarchy"] = preset.showHierarchy;
    j["showGameView"] = preset.showGameView;
    j["showPostEffect"] = preset.showPostEffect;
    j["showMapEditor"] = preset.showMapEditor;
    j["showMapSettings"] = preset.showMapSettings;
    j["showReplayEditor"] = preset.showReplayEditor;
    j["showAnimEditor"] = preset.showAnimEditor;
    j["showLightEditor"] = preset.showLightEditor;
    j["showSpotLightPanel"] = preset.showSpotLightPanel;
    j["showModelPlacement"] = preset.showModelPlacement;
    j["showModelPalette"] = preset.showModelPalette;

    std::filesystem::path filePath = dirPath / (name + ".json");
    std::ofstream ofs(filePath);
    if (ofs.is_open()) {
        ofs << j.dump(4);
    }

    bool found = false;
    for (auto& p : layoutPresets_) {
        if (p.name == name) {
            p = preset;
            found = true;
            break;
        }
    }
    if (!found) {
        layoutPresets_.push_back(preset);
    }

    presetStatusMessage_ = "プリセット「" + name + "」を保存しました";
    presetStatusMessageTimer_ = 3.0f;
}

bool EditorManager::ApplyLayoutPreset(const std::string& name) {
    for (const auto& preset : layoutPresets_) {
        if (preset.name == name) {
            showInspector_ = preset.showInspector;
            showHierarchy_ = preset.showHierarchy;
            showGameView_ = preset.showGameView;
            showPostEffect_ = preset.showPostEffect;
            showMapEditor_ = preset.showMapEditor;
            showMapSettings_ = preset.showMapSettings;
            showReplayEditor_ = preset.showReplayEditor;
            showAnimEditor_ = preset.showAnimEditor;
            showLightEditor_ = preset.showLightEditor;
            showSpotLightPanel_ = preset.showSpotLightPanel;
            showModelPlacementEditor_ = preset.showModelPlacement;
            showModelPalette_ = preset.showModelPalette;

            if (!preset.iniData.empty()) {
                ImGui::LoadIniSettingsFromMemory(preset.iniData.c_str(), preset.iniData.size());
            }
            presetStatusMessage_ = "プリセット「" + name + "」を適用しました";
            presetStatusMessageTimer_ = 3.0f;
            return true;
        }
    }
    return false;
}

bool EditorManager::DeleteLayoutPreset(const std::string& name) {
    std::filesystem::path filePath = std::filesystem::path("resources/json/local/layout_presets") / (name + ".json");
    if (std::filesystem::exists(filePath)) {
        std::filesystem::remove(filePath);
    }

    auto it = std::remove_if(layoutPresets_.begin(), layoutPresets_.end(), [&](const WindowLayoutPreset& p) {
        return p.name == name;
    });
    if (it != layoutPresets_.end()) {
        layoutPresets_.erase(it, layoutPresets_.end());
        presetStatusMessage_ = "プリセット「" + name + "」を削除しました";
        presetStatusMessageTimer_ = 3.0f;
        return true;
    }
    return false;
}

bool EditorManager::ExportLayoutPresetToFile(const std::string& name, const std::string& filePath) {
    for (const auto& preset : layoutPresets_) {
        if (preset.name == name) {
            nlohmann::json j;
            j["name"] = preset.name;
            j["iniData"] = preset.iniData;
            j["showInspector"] = preset.showInspector;
            j["showHierarchy"] = preset.showHierarchy;
            j["showGameView"] = preset.showGameView;
            j["showPostEffect"] = preset.showPostEffect;
            j["showMapEditor"] = preset.showMapEditor;
            j["showMapSettings"] = preset.showMapSettings;
            j["showReplayEditor"] = preset.showReplayEditor;
            j["showAnimEditor"] = preset.showAnimEditor;
            j["showLightEditor"] = preset.showLightEditor;
            j["showSpotLightPanel"] = preset.showSpotLightPanel;

            std::filesystem::path outPath(filePath);
            if (outPath.has_parent_path()) {
                std::filesystem::create_directories(outPath.parent_path());
            }
            std::ofstream ofs(outPath);
            if (ofs.is_open()) {
                ofs << j.dump(4);
                presetStatusMessage_ = "ファイル「" + outPath.string() + "」へ出力しました";
                presetStatusMessageTimer_ = 3.0f;
                return true;
            }
            break;
        }
    }
    presetStatusMessage_ = "出力に失敗しました";
    presetStatusMessageTimer_ = 3.0f;
    return false;
}

bool EditorManager::ImportLayoutPresetFromFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        presetStatusMessage_ = "ファイルが開けませんでした: " + filePath;
        presetStatusMessageTimer_ = 3.0f;
        return false;
    }
    try {
        nlohmann::json j;
        ifs >> j;
        WindowLayoutPreset preset;
        preset.name = j.value("name", std::filesystem::path(filePath).stem().string());
        preset.iniData = j.value("iniData", "");
        preset.showInspector = j.value("showInspector", true);
        preset.showHierarchy = j.value("showHierarchy", true);
        preset.showGameView = j.value("showGameView", true);
        preset.showPostEffect = j.value("showPostEffect", true);
        preset.showMapEditor = j.value("showMapEditor", true);
        preset.showMapSettings = j.value("showMapSettings", true);
        preset.showReplayEditor = j.value("showReplayEditor", true);
        preset.showAnimEditor = j.value("showAnimEditor", true);
        preset.showLightEditor = j.value("showLightEditor", true);
        preset.showSpotLightPanel = j.value("showSpotLightPanel", true);

        std::filesystem::path dirPath("resources/json/local/layout_presets");
        std::filesystem::create_directories(dirPath);
        std::ofstream ofs(dirPath / (preset.name + ".json"));
        if (ofs.is_open()) {
            ofs << j.dump(4);
        }

        bool found = false;
        for (auto& p : layoutPresets_) {
            if (p.name == preset.name) {
                p = preset;
                found = true;
                break;
            }
        }
        if (!found) {
            layoutPresets_.push_back(preset);
        }
        presetStatusMessage_ = "プリセット「" + preset.name + "」をインポートしました";
        presetStatusMessageTimer_ = 3.0f;
        return true;
    } catch (...) {
        presetStatusMessage_ = "JSONの解析に失敗しました";
        presetStatusMessageTimer_ = 3.0f;
        return false;
    }
}
#endif