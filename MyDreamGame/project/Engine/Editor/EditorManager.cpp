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
#include "Game2D/Player/Player2D.h"
#include "Component/TransformComponent.h"
#include "Scenes/AnimationPreviewScene.h"

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
ImVec2 EditorManager::gameViewPos_ = ImVec2(0, 0);
ImVec2 EditorManager::gameViewSize_ = ImVec2(1280, 720);

// 枠を返すための関数
static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    // 空でOK
}

void EditorManager::Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue) {
    animationEditor_ = std::make_unique<AnimationEditor>();
    ScanAvailableModels();
    ScanAvailableTextures();
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

void EditorManager::ScanAvailableModels() {
    availableModels_.clear();
    std::filesystem::path basePath("resources/Object");
    if (!std::filesystem::exists(basePath) || !std::filesystem::is_directory(basePath)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".obj") {
            std::string relativePath = std::filesystem::relative(entry.path(), "resources").string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
            availableModels_.push_back(relativePath);
        }
    }
}

void EditorManager::ScanAvailableTextures() {
    availableTextures_.clear();
    std::vector<std::string> pathsToScan = { "resources/Sprite", "resources/Object" };
    for (const auto& pathStr : pathsToScan) {
        std::filesystem::path basePath(pathStr);
        if (!std::filesystem::exists(basePath) || !std::filesystem::is_directory(basePath)) continue;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".jpeg") {
                    std::string relativePath = std::filesystem::relative(entry.path(), "resources").string();
                    std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                    availableTextures_.push_back(relativePath);
                }
            }
        }
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

    // --- 初回起動時 / マップロード時のA*座標初期化 ---
    if (!isAStarPosInitialized_) {
        IScene* activeScene = sceneManager->GetCurrentScene();
        if (activeScene && activeScene->GetMapChip()) {
            UpdateAStarPositionsFromMap(activeScene->GetMapChip(), sceneManager);
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
                    mapChip->LoadFromStageName(stageFilename_);
                }
                UpdateAStarPositionsFromMap(mapChip, sceneManager);
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

    isMapEditorVisible_ = false;

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
            ImGui::MenuItem("インスペクター", nullptr, &showInspector_);
            ImGui::MenuItem("ヒエラルキー", nullptr, &showHierarchy_);
            ImGui::MenuItem("ゲームビュー", nullptr, &showGameView_);
            ImGui::MenuItem("ポストエフェクト", nullptr, &showPostEffect_);
            ImGui::MenuItem("マップチップ画面", nullptr, &showMapEditor_);
            ImGui::MenuItem("マップ設定", nullptr, &showMapSettings_);
            ImGui::MenuItem("リプレイエディター", nullptr, &showReplayEditor_);
            ImGui::MenuItem("アニメーションエディター", nullptr, &showAnimEditor_);
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
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), f)) {
                    if (strstr(buffer, "マイメディア")) {
                        hasReplayEditor = true;
                    }
                    if (strstr(buffer, "ステージセレクトエディター")) {
                        hasStageSelectEditor = true;
                    }
                    if (strstr(buffer, "ドープシート")) {
                        hasDopeSheet = true;
                    }
                }
                if (!hasReplayEditor || !hasStageSelectEditor || !hasDopeSheet) {
                    resetLayout = true;
                }
                fclose(f);
            }
        }

        if (resetLayout || !hasIniFile) {
            resetLayout = false;

            showInspector_ = true;
            showHierarchy_ = true;
            showGameView_ = true;
            showPostEffect_ = true;
            showMapEditor_ = true;
            showMapSettings_ = true;
            showReplayEditor_ = true;
            showAnimEditor_ = true;

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
            ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_main);
            ImGui::DockBuilderDockWindow("リプレイエディター", dock_id_main);
            ImGui::DockBuilderDockWindow("アニメーションエディター", dock_id_main);

            // 左側
            ImGui::DockBuilderDockWindow("ヒエラルキー", dock_id_left);
            ImGui::DockBuilderDockWindow("マイメディア (リプレイ履歴)", dock_id_left);

            // 右側
            ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
            ImGui::DockBuilderDockWindow("ポストエフェクト", dock_id_right);

            // 下側
            ImGui::DockBuilderDockWindow("マップ設定", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ステージセレクトエディター", dock_id_bottom);
            ImGui::DockBuilderDockWindow("タイムライン", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ドープシート (タイムライン)", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    // --- Game View ウィンドウ ---
    if (showGameView_) {
        if (ImGui::Begin("ゲームビュー", &showGameView_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
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
        if (ImGui::Begin("リプレイエディター", &showReplayEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowAppearing()) {
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
        if (ImGui::Begin("アニメーションエディター", &showAnimEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowAppearing()) {
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
                    // 1. プレイヤー（存在する場合）
                    if (activeScene->GetPlayer()) {
                        auto* player = activeScene->GetPlayer();
                        bool isSelected = (selectedPrimitive_ == player->GetPrimitiveObject() || (selectedObject_ && selectedObject_ == player->GetModelObject()));
                        if (ImGui::Selectable("[Player] プレイヤー", isSelected)) {
                            selectedGameObject_ = nullptr;
                            selectedParticle_ = nullptr;
                            selectedPrimitive_ = player->GetPrimitiveObject();
                            selectedObject_ = player->GetModelObject();
                            if (animationEditor_) { animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_); animationEditor_->RefreshAnimationJointList(sceneManager); }
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
            bool isMapChipSelected = (mapEditorSelectedTool_ >= 100 || (mapEditorSelectedTool_ >= 1 && mapEditorSelectedTool_ <= 12));
            if (selectedGameObject_ || selectedObject_ || selectedParticle_ || selectedPrimitive_ || isMapChipSelected || selectedReplayBlock_.IsValid()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.35f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
                if (ImGui::Button("グローバル設定を表示", ImVec2(-1, 0))) {
                    selectedGameObject_ = nullptr;
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                    selectedReplayBlock_.Clear();
                    mapEditorSelectedTool_ = 0;
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
                    IScene *activeScene = sceneManager->GetCurrentScene();
                    bool handled = false;
                    if (activeScene) {
                    MapChip2D* mapChip = activeScene->GetMapChip();
                    if (mapChip && (mapEditorSelectedTool_ >= 100 || (mapEditorSelectedTool_ >= 1 && mapEditorSelectedTool_ <= 12))) {
                        // ブロックの設定を表示
                        MapChip2D::CustomBlockDef* targetDef = nullptr;
                        bool isTemplate = false;
                        bool changed = false;

                        if (mapEditorSelectedTool_ >= 100) {
                            auto& palette = mapChip->GetCustomPalette();
                            for (auto& def : palette) {
                                if (def.id == mapEditorSelectedTool_) {
                                    targetDef = &def;
                                    break;
                                }
                            }
                        } else {
                            auto& templates = mapChip->GetTemplatePalette();
                            for (auto& def : templates) {
                                if (def.id == mapEditorSelectedTool_) {
                                    targetDef = &def;
                                    isTemplate = true;
                                    break;
                                }
                            }
                        }

                        if (targetDef) {
                            handled = true;
                            if (isTemplate) {
                                ImGui::Text("Template Settings (ID: %d)", targetDef->id);
                            } else {
                                ImGui::Text("Custom Block Settings (ID: %d)", targetDef->id);
                            }
                            
                            char nameBuf[256];
                            strcpy_s(nameBuf, sizeof(nameBuf), targetDef->name.c_str());
                            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                                targetDef->name = nameBuf;
                                changed = true;
                            }

                            const char* types[] = { "NormalBlock", "DeathBlock", "GoalBlock", "OneWayBlock" };
                            int currentType = -1;
                            for (int i = 0; i < 4; ++i) {
                                if (targetDef->type == types[i]) {
                                    currentType = i;
                                    break;
                                }
                            }
                            if (ImGui::Combo("種類 (Type)", &currentType, types, 4)) {
                                targetDef->type = types[currentType];
                                changed = true;
                                // デフォルトプロパティを設定 (BasicToolsのテンプレートに合わせる)
                                bool foundTemplate = false;
                                for (const auto& t : mapChip->GetTemplatePalette()) {
                                    if (t.type == targetDef->type) {
                                        targetDef->properties = t.properties;
                                        foundTemplate = true;
                                        break;
                                    }
                                }
                                if (!foundTemplate) {
                                    targetDef->properties = nlohmann::json::object(); // リセット
                                }
                            }

                            float col[4] = { targetDef->color.x, targetDef->color.y, targetDef->color.z, targetDef->color.w };
                            if (ImGui::ColorEdit4("色 (Color)", col)) {
                                targetDef->color = { col[0], col[1], col[2], col[3] };
                                changed = true;
                            }

                            float scale[3] = { targetDef->scale.x, targetDef->scale.y, targetDef->scale.z };
                            if (ImGui::DragFloat3("スケール (Scale)", scale, 0.01f)) {
                                targetDef->scale = { scale[0], scale[1], scale[2] };
                                changed = true;
                            }

                            if (ImGui::BeginCombo("モデル (Model)", targetDef->modelName.empty() ? "なし (None)" : targetDef->modelName.c_str())) {
                                bool isNoneSelected = targetDef->modelName.empty();
                                if (ImGui::Selectable("なし (None)", isNoneSelected)) {
                                    targetDef->modelName = "";
                                    changed = true;
                                }
                                if (isNoneSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                                for (const auto& modelPath : availableModels_) {
                                    bool isSelected = (targetDef->modelName == modelPath);
                                    if (ImGui::Selectable(modelPath.c_str(), isSelected)) {
                                        targetDef->modelName = modelPath;
                                        changed = true;
                                    }
                                    if (isSelected) {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();

                            }

                            if (ImGui::BeginCombo("テクスチャ (Texture)", targetDef->textureName.empty() ? "なし (None)" : targetDef->textureName.c_str())) {
                                bool isTexNoneSelected = targetDef->textureName.empty();
                                if (ImGui::Selectable("なし (None)", isTexNoneSelected)) {
                                    targetDef->textureName = "";
                                    changed = true;
                                }
                                if (isTexNoneSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                                for (const auto& texPath : availableTextures_) {
                                    bool isSelected = (targetDef->textureName == texPath);
                                    if (ImGui::Selectable(texPath.c_str(), isSelected)) {
                                        targetDef->textureName = texPath;
                                        changed = true;
                                    }
                                    if (isSelected) {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }

                            ImGui::Separator();
                            ImGui::Text("プロパティ:");
                            auto getJpKey = [](const std::string& k) {
                                if (k == "speed") return std::string("スピード (speed)");
                                if (k == "speedForward") return std::string("往路の速さ (speedForward)");
                                if (k == "speedBackward") return std::string("復路の速さ (speedBackward)");
                                if (k == "waitTime") return std::string("待機時間 (waitTime)");
                                if (k == "acceleration") return std::string("加速度 (acceleration)");
                                if (k == "maxSpeedForward") return std::string("往路の最高速度 (maxSpeedForward)");
                                if (k == "maxSpeedBackward") return std::string("復路の最高速度 (maxSpeedBackward)");
                                if (k == "maxSpeed") return std::string("最高速度 (maxSpeed)");
                                if (k == "direction") return std::string("方向 (direction)");
                                if (k == "range") return std::string("移動距離 (range)");
                                if (k == "jumpVelocityVertical") return std::string("縦ジャンプ力 (jumpVelocityVertical)");
                                if (k == "jumpVelocityHorizontal") return std::string("横ジャンプ力 (jumpVelocityHorizontal)");
                                if (k == "moveSpeed") return std::string("移動速度 (moveSpeed)");
                                return k;
                            };
                            for (auto& [key, value] : targetDef->properties.items()) {
                                std::string jpKey = getJpKey(key);
                                if (value.is_number()) {
                                    float v = value.get<float>();
                                    if (ImGui::DragFloat(jpKey.c_str(), &v, 0.1f)) {
                                        value = v;
                                        changed = true;
                                    }
                                } else if (value.is_string()) {
                                    std::string v = value.get<std::string>();
                                    char buf[256];
                                    strcpy_s(buf, sizeof(buf), v.c_str());
                                    if (ImGui::InputText(jpKey.c_str(), buf, sizeof(buf))) {
                                        value = buf;
                                        changed = true;
                                    }
                                } else if (value.is_boolean()) {
                                    bool v = value.get<bool>();
                                    if (ImGui::Checkbox(jpKey.c_str(), &v)) {
                                        value = v;
                                        changed = true;
                                    }
                                }
                            }

                            static bool autoApply = true;
                            ImGui::Checkbox("自動適用 (Auto Apply)", &autoApply);
                            ImGui::SameLine();
                            if (ImGui::Button("デフォルトに戻す (Reset to Default)")) {
                                bool found = false;
                                auto& templates = mapChip->GetTemplatePalette();
                                for (const auto& t : templates) {
                                    if (t.type == targetDef->type) {
                                        targetDef->color = t.color;
                                        targetDef->scale = t.scale;
                                        targetDef->modelName = t.modelName;
                                        targetDef->textureName = t.textureName;
                                        targetDef->properties = t.properties;
                                        changed = true;
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    targetDef->color = {1.0f, 1.0f, 1.0f, 1.0f};
                                    targetDef->scale = {1.0f, 1.0f, 1.0f};
                                    targetDef->modelName = "";
                                    targetDef->textureName = "";
                                    bool foundTemplate = false;
                                    for (const auto& t : mapChip->GetTemplatePalette()) {
                                        if (t.type == targetDef->type) {
                                            targetDef->properties = t.properties;
                                            foundTemplate = true;
                                            break;
                                        }
                                    }
                                    if (!foundTemplate) {
                                        targetDef->properties = nlohmann::json::object();
                                    }
                                    changed = true;
                                }
                            }

                            if (ImGui::Button("変更を適用 (Apply & Rebuild)") || (autoApply && changed)) {
                                if (isTemplate) {
                                    mapChip->SaveTemplatesToFile("resources/json/shared/templates_config.json");
                                } else {
                                    std::string name = stageFilename_;
                                    if (name.length() < 4 || name.substr(name.length() - 4) != ".txt") name += ".txt";
                                    mapChip->SaveToFile("resources/json/shared/MapData/" + name);
                                }
                                mapChip->RebuildChipObjects();
                            }
                        }
                    }
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

                bool lightingChanged = false;

                ImGui::Text("アクティブな光源");
                lightingChanged |= ImGui::RadioButton("平行光源 (Directional)", &activeLightType_, 0);
                ImGui::SameLine();
                lightingChanged |= ImGui::RadioButton("点光源 (Point)", &activeLightType_, 1);
                ImGui::SameLine();
                lightingChanged |= ImGui::RadioButton("スポットライト (Spot)", &activeLightType_, 2);
                ImGui::Separator();

                DirectionalLight *dLight = modelCommon->GetDirectionalLight();
                PointLight *pLight = modelCommon->GetPointLight();
                SpotLight *sLight = modelCommon->GetSpotLight();

                bool isFlatShading = (dLight->enableFlatShading != 0);
                if (ImGui::Checkbox("フラットシェーディング", &isFlatShading)) {
                    dLight->enableFlatShading = isFlatShading ? 1 : 0;
                }
                ImGui::Separator();

                if (activeLightType_ == 0) {
                    dLight->intensity = dIntensity_;
                    pLight->intensity = 0.0f;
                    sLight->intensity = 0.0f;
                    ImGui::Text("平行光源設定");
                    
                    auto onLightUpdate = [=]() { SaveLightingConfig(modelCommon); };
                    
                    lightingChanged |= TrackColorEdit4("色", &dLight->color, onLightUpdate);
                    if (TrackDragFloat("輝度 (Intensity)", &dIntensity_, 0.01f, 0.0f, 10.0f, "%.3f", [=](){ dLight->intensity = dIntensity_; onLightUpdate(); })) {
                        dLight->intensity = dIntensity_;
                        lightingChanged = true;
                    }
                    if (TrackDragFloat3("方向", &dLight->direction, 0.01f, -1.0f, 1.0f, "%.3f", [=](){ dLight->direction = TransformFunctions::Normalize(dLight->direction); onLightUpdate(); })) {
                        dLight->direction = TransformFunctions::Normalize(dLight->direction);
                        lightingChanged = true;
                    }
                } else if (activeLightType_ == 1) {
                    pLight->intensity = pIntensity_;
                    dLight->intensity = 0.0f;
                    sLight->intensity = 0.0f;
                    ImGui::Text("点光源設定");
                    
                    auto onLightUpdate = [=]() { SaveLightingConfig(modelCommon); };
                    
                    lightingChanged |= TrackColorEdit4("色", &pLight->color, onLightUpdate);
                    if (TrackDragFloat("輝度 (Intensity)", &pIntensity_, 0.01f, 0.0f, 10.0f, "%.3f", [=](){ pLight->intensity = pIntensity_; onLightUpdate(); })) {
                        pLight->intensity = pIntensity_;
                        lightingChanged = true;
                    }
                    lightingChanged |= TrackDragFloat3("位置", &pLight->position, 0.1f, 0.0f, 0.0f, "%.3f", onLightUpdate);
                    lightingChanged |= TrackDragFloat("半径 (Radius)", &pLight->radius, 0.1f, 0.0f, 100.0f, "%.3f", onLightUpdate);
                    lightingChanged |= TrackDragFloat("減衰 (Decay)", &pLight->decay, 0.01f, 0.0f, 10.0f, "%.3f", onLightUpdate);
                } else if (activeLightType_ == 2) {
                    sLight->intensity = sIntensity_;
                    dLight->intensity = 0.0f;
                    pLight->intensity = 0.0f;
                    ImGui::Text("スポットライト設定");
                    
                    auto onLightUpdate = [=]() { SaveLightingConfig(modelCommon); };
                    
                    lightingChanged |= TrackColorEdit4("色", &sLight->color, onLightUpdate);
                    if (TrackDragFloat("輝度 (Intensity)", &sIntensity_, 0.01f, 0.0f, 20.0f, "%.3f", [=](){ sLight->intensity = sIntensity_; onLightUpdate(); })) {
                        sLight->intensity = sIntensity_;
                        lightingChanged = true;
                    }
                    lightingChanged |= TrackDragFloat3("位置", &sLight->position, 0.1f, 0.0f, 0.0f, "%.3f", onLightUpdate);
                    if (TrackDragFloat3("方向", &sLight->direction, 0.01f, -1.0f, 1.0f, "%.3f", [=](){ sLight->direction = TransformFunctions::Normalize(sLight->direction); onLightUpdate(); })) {
                        sLight->direction = TransformFunctions::Normalize(sLight->direction);
                        lightingChanged = true;
                    }
                    lightingChanged |= TrackDragFloat("距離", &sLight->distance, 0.1f, 0.0f, 100.0f, "%.3f", onLightUpdate);
                    lightingChanged |= TrackDragFloat("減衰 (Decay)", &sLight->decay, 0.01f, 0.0f, 10.0f, "%.3f", onLightUpdate);
                    
                    if (TrackSliderFloat("全角 (Total Angle)", &spotAngleDeg_, 0.0f, 90.0f, "%.3f", [=](){ sLight->cosAngle = std::cos(spotAngleDeg_ * static_cast<float>(M_PI) / 180.0f); onLightUpdate(); })) {
                        sLight->cosAngle = std::cos(spotAngleDeg_ * static_cast<float>(M_PI) / 180.0f);
                        lightingChanged = true;
                    }
                    if (TrackSliderFloat("フォールオフ開始角", &spotFalloffDeg_, 0.0f, spotAngleDeg_, "%.3f", [=](){ sLight->cosFalloffStart = std::cos(spotFalloffDeg_ * static_cast<float>(M_PI) / 180.0f); onLightUpdate(); })) {
                        sLight->cosFalloffStart = std::cos(spotFalloffDeg_ * static_cast<float>(M_PI) / 180.0f);
                        lightingChanged = true;
                    }
                }

                if (lightingChanged) {
                    SaveLightingConfig(modelCommon);
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
    if (showMapEditor_) {
        if (ImGui::Begin("マップチップ画面", &showMapEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
                currentMode_ = EditorMode::Normal;
            }
            isMapEditorVisible_ = true;

            ImGui::Text("編集モード:");
            ImGui::SameLine();
            int modeInt = static_cast<int>(mapEditMode_);
            ImGui::RadioButton("通常塗", &modeInt, 0); ImGui::SameLine();
            ImGui::RadioButton("範囲選択", &modeInt, 1); ImGui::SameLine();
            ImGui::RadioButton("コピー", &modeInt, 2); ImGui::SameLine();
            ImGui::RadioButton("貼り付け", &modeInt, 3); ImGui::SameLine();
            ImGui::RadioButton("バケツ塗", &modeInt, 4);
            mapEditMode_ = static_cast<MapEditMode>(modeInt);

            IScene *activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                MapChip2D* mapChip = activeScene->GetMapChip();
                if (mapChip) {
                    // サブ画面描画 (GameViewと同様の処理)
                    ImVec2 contentSize = ImGui::GetContentRegionAvail();
                    if (contentSize.x < 100.0f) contentSize.x = 100.0f;
                    if (contentSize.y < 100.0f) contentSize.y = 100.0f;
                    
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
                    // 中央寄せ
                    ImVec2 currentPos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));
                    
                    ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();
                    
                    // GameViewの描画と同じテクスチャを表示
                    ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);

                    // 1マスのグリッドを描画する
                    Camera* camera = *activeCamera;
                    if (camera) {
                        Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                        
                        auto WorldToScreen = [&](float wx, float wy) -> ImVec2 {
                            Vector3 ndc = TransformFunctions::EulerTransform({wx, wy, 0.0f}, viewProj);
                            float screenX = imageScreenPos.x + (ndc.x + 1.0f) * 0.5f * imageSize.x;
                            float screenY = imageScreenPos.y + (1.0f - ndc.y) * 0.5f * imageSize.y;
                            return ImVec2(screenX, screenY);
                        };

                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        int mapWidth = mapChip->GetWidth();
                        int mapHeight = mapChip->GetHeight();
                        
                        // 画像外にはみ出ないようにクリッピング
                        drawList->PushClipRect(imageScreenPos, ImVec2(imageScreenPos.x + imageSize.x, imageScreenPos.y + imageSize.y), true);

                        // 縦線
                        for (int x = 0; x <= mapWidth; ++x) {
                            ImVec2 p1 = WorldToScreen(static_cast<float>(x), 0.0f);
                            ImVec2 p2 = WorldToScreen(static_cast<float>(x), static_cast<float>(mapHeight));
                            drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, 80), 1.0f);
                        }
                        
                        // 横線
                        for (int y = 0; y <= mapHeight; ++y) {
                            ImVec2 p1 = WorldToScreen(0.0f, static_cast<float>(y));
                            ImVec2 p2 = WorldToScreen(static_cast<float>(mapWidth), static_cast<float>(y));
                            drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, 80), 1.0f);
                        }

                        // ルームの描画
                        const auto& rooms = mapChip->GetRooms();
                        for (size_t i = 0; i < rooms.size(); ++i) {
                            const auto& r = rooms[i];
                            ImVec2 pTL = WorldToScreen(r.x, r.y + r.height);
                            ImVec2 pBR = WorldToScreen(r.x + r.width, r.y);
                            ImU32 color = (isRoomEditMode_ && draggingRoomIndex_ == static_cast<int>(i)) ? IM_COL32(255, 255, 0, 100) : IM_COL32(50, 50, 255, 50);
                            ImU32 borderColor = (isRoomEditMode_ && draggingRoomIndex_ == static_cast<int>(i)) ? IM_COL32(255, 255, 0, 255) : IM_COL32(50, 50, 255, 255);
                            drawList->AddRectFilled(pTL, pBR, color);
                            drawList->AddRect(pTL, pBR, borderColor, 0.0f, 0, 2.0f);
                        }

                        // 範囲選択の描画
                        if (selectStartX_ != -1 && selectStartY_ != -1 && selectEndX_ != -1 && selectEndY_ != -1) {
                            int minX = (std::min)(selectStartX_, selectEndX_);
                            int maxX = (std::max)(selectStartX_, selectEndX_);
                            int minY = (std::min)(selectStartY_, selectEndY_);
                            int maxY = (std::max)(selectStartY_, selectEndY_);
                            ImVec2 pTL = WorldToScreen(static_cast<float>(minX), static_cast<float>(maxY + 1));
                            ImVec2 pBR = WorldToScreen(static_cast<float>(maxX + 1), static_cast<float>(minY));
                            drawList->AddRectFilled(pTL, pBR, IM_COL32(0, 255, 255, 80));
                            drawList->AddRect(pTL, pBR, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                        }
                        
                        // プレビュー描画
                        if (mapEditMode_ == MapEditMode::Normal && !pendingBlocks_.empty()) {
                            for (const auto& pos : pendingBlocks_) {
                                ImVec2 pTL = WorldToScreen(static_cast<float>(pos.first), static_cast<float>(pos.second + 1));
                                ImVec2 pBR = WorldToScreen(static_cast<float>(pos.first + 1), static_cast<float>(pos.second));
                                drawList->AddRectFilled(pTL, pBR, IM_COL32(255, 100, 100, 150));
                                drawList->AddRect(pTL, pBR, IM_COL32(255, 100, 100, 255), 0.0f, 0, 2.0f);
                            }
                        }

                        // スポーン地点とルームリスポーン地点の描画（ゲーム上では非表示のためここでオーバーレイ描画）
                        for (int y = 0; y < mapHeight; ++y) {
                            for (int x = 0; x < mapWidth; ++x) {
                                MapChip2D::ChipType type = mapChip->GetChip(x, y);
                                if (type == MapChip2D::ChipType::kPlayerSpawn || type == MapChip2D::ChipType::kRoomRespawn) {
                                    ImVec2 pTL = WorldToScreen(static_cast<float>(x), static_cast<float>(y + 1));
                                    ImVec2 pBR = WorldToScreen(static_cast<float>(x + 1), static_cast<float>(y));
                                    if (type == MapChip2D::ChipType::kPlayerSpawn) {
                                        drawList->AddRectFilled(pTL, pBR, IM_COL32(51, 153, 255, 180));
                                        drawList->AddRect(pTL, pBR, IM_COL32(51, 153, 255, 255), 0.0f, 0, 2.0f);
                                    } else {
                                        drawList->AddRectFilled(pTL, pBR, IM_COL32(51, 204, 255, 180));
                                        drawList->AddRect(pTL, pBR, IM_COL32(51, 204, 255, 255), 0.0f, 0, 2.0f);
                                    }
                                }
                            }
                        }

                        drawList->PopClipRect();
                    }
                    
                    // クリック判定用の見えないボタン
                    ImGui::SetCursorScreenPos(imageScreenPos);
                    ImGui::InvisibleButton("MapCanvasImage", imageSize);
                    isMapEditorHovered_ = ImGui::IsItemHovered();
                    
                    if (isMapEditorHovered_) {
                        ImVec2 mousePos = ImGui::GetIO().MousePos;
                        float localX = mousePos.x - imageScreenPos.x;
                        float localY = mousePos.y - imageScreenPos.y;
                        
                        float u = localX / imageSize.x;
                        float v = localY / imageSize.y;
                        
                        float ndcX = u * 2.0f - 1.0f;
                        float ndcY = 1.0f - v * 2.0f;
                        
                        Camera* camera = *activeCamera;
                        Vector3 worldPos = {0,0,0};
                        if (camera) {
                            Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                            Matrix4x4 invViewProj = TransformFunctions::Inverse(viewProj);
                            worldPos = TransformFunctions::EulerTransform({ndcX, ndcY, 0.0f}, invViewProj);
                        }

                        // モード切り替えショートカット
                        if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
                            int m = static_cast<int>(mapEditMode_);
                            if (ImGui::GetIO().MouseWheel < 0.0f) m = (m + 1) % 5;
                            else m = (m + 4) % 5;
                            mapEditMode_ = static_cast<MapEditMode>(m);
                        }

                        // Undo / Redo
                        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                            Undo();
                        }
                        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                            Redo();
                        }

                        if (isRoomEditMode_) {
                            float hitDist = 0.5f;
                            auto& rooms = mapChip->GetRooms();

                            // ドラッグ開始判定
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                                BeginRoomHistoryCapture(mapChip);
                                draggingRoomIndex_ = -1;
                                roomDragHandle_ = 0;

                                for (int i = static_cast<int>(rooms.size()) - 1; i >= 0; --i) {
                                    const auto& r = rooms[i];
                                    bool inX = (worldPos.x >= r.x && worldPos.x <= r.x + r.width);
                                    bool inY = (worldPos.y >= r.y && worldPos.y <= r.y + r.height);
                                    
                                    bool onLeft = std::abs(worldPos.x - r.x) < hitDist;
                                    bool onRight = std::abs(worldPos.x - (r.x + r.width)) < hitDist;
                                    bool onBottom = std::abs(worldPos.y - r.y) < hitDist;
                                    bool onTop = std::abs(worldPos.y - (r.y + r.height)) < hitDist;
                                    
                                    if ((inX && inY) || ((onLeft || onRight) && inY) || ((onTop || onBottom) && inX)) {
                                        draggingRoomIndex_ = i;
                                        if (onLeft && onTop) roomDragHandle_ = 2;
                                        else if (onRight && onTop) roomDragHandle_ = 3;
                                        else if (onLeft && onBottom) roomDragHandle_ = 4;
                                        else if (onRight && onBottom) roomDragHandle_ = 5;
                                        else if (onLeft) roomDragHandle_ = 6;
                                        else if (onRight) roomDragHandle_ = 7;
                                        else if (onTop) roomDragHandle_ = 8;
                                        else if (onBottom) roomDragHandle_ = 9;
                                        else {
                                            roomDragHandle_ = 1; // Move
                                            roomDragOffsetX_ = worldPos.x - r.x;
                                            roomDragOffsetY_ = worldPos.y - r.y;
                                        }
                                        break;
                                    }
                                }

                                if (draggingRoomIndex_ != -1 && ImGui::GetIO().KeyCtrl) {
                                    rooms.erase(rooms.begin() + draggingRoomIndex_);
                                    draggingRoomIndex_ = -1;
                                    roomDragHandle_ = 0;
                                } else if (draggingRoomIndex_ == -1 && !ImGui::GetIO().KeyCtrl) {
                                    bool snap = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                                    StageRoom newRoom;
                                    newRoom.x = snap ? std::floor(worldPos.x) : worldPos.x;
                                    newRoom.y = snap ? std::floor(worldPos.y) : worldPos.y;
                                    newRoom.width = 1.0f;
                                    newRoom.height = 1.0f;
                                    rooms.push_back(newRoom);
                                    draggingRoomIndex_ = static_cast<int>(rooms.size()) - 1;
                                    roomDragHandle_ = 5; // BottomRight drag
                                }
                            }

                            if ((ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)) && draggingRoomIndex_ != -1) {
                                auto& r = rooms[draggingRoomIndex_];
                                bool snap = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
                                float snapX_left = snap ? std::floor(worldPos.x) : worldPos.x;
                                float snapY_bottom = snap ? std::floor(worldPos.y) : worldPos.y;
                                float snapX_right = snap ? std::floor(worldPos.x) + 1.0f : worldPos.x;
                                float snapY_top = snap ? std::floor(worldPos.y) + 1.0f : worldPos.y;
                                float minSize = snap ? 1.0f : 0.1f;
                                
                                if (roomDragHandle_ == 1) { // Move
                                    float targetX = snap ? std::floor(worldPos.x) : worldPos.x;
                                    float targetY = snap ? std::floor(worldPos.y) : worldPos.y;
                                    r.x = targetX - roomDragOffsetX_;
                                    r.y = targetY - roomDragOffsetY_;
                                    if (snap) {
                                        r.x = std::round(r.x);
                                        r.y = std::round(r.y);
                                    }
                                } else {
                                    if (roomDragHandle_ == 2 || roomDragHandle_ == 6 || roomDragHandle_ == 4) {
                                        float right = r.x + r.width;
                                        r.x = std::fmin(snapX_left, right - minSize);
                                        r.width = right - r.x;
                                    }
                                    if (roomDragHandle_ == 3 || roomDragHandle_ == 7 || roomDragHandle_ == 5) {
                                        r.width = std::fmax(minSize, snapX_right - r.x);
                                    }
                                    if (roomDragHandle_ == 4 || roomDragHandle_ == 9 || roomDragHandle_ == 5) {
                                        float top = r.y + r.height;
                                        r.y = std::fmin(snapY_bottom, top - minSize);
                                        r.height = top - r.y;
                                    }
                                    if (roomDragHandle_ == 2 || roomDragHandle_ == 8 || roomDragHandle_ == 3) {
                                        r.height = std::fmax(minSize, snapY_top - r.y);
                                    }
                                }
                            }

                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                                EndRoomHistoryCapture(mapChip);
                                draggingRoomIndex_ = -1;
                                roomDragHandle_ = 0;
                            }
                        } else {
                            if (camera) {
                                int mapWidth = mapChip->GetWidth();
                                int mapHeight = mapChip->GetHeight();
                                int gridX = mapChip->WorldToChipX(worldPos.x);
                                int gridY = mapChip->WorldToChipY(worldPos.y);

                                bool inBounds = (gridX >= 0 && gridX < mapWidth && gridY >= 0 && gridY < mapHeight);
                                bool isSelectableTool = (mapEditorSelectedTool_ >= 0);
                                
                                if (mapEditMode_ == MapEditMode::Normal) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isSelectableTool) {
                                        if (inBounds) {
                                            if (std::find(pendingBlocks_.begin(), pendingBlocks_.end(), std::make_pair(gridX, gridY)) == pendingBlocks_.end()) {
                                                pendingBlocks_.push_back({gridX, gridY});
                                            }
                                        }
                                        prevGridX_ = gridX;
                                        prevGridY_ = gridY;
                                    }
                                    else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && isSelectableTool) {
                                        if (prevGridX_ != -1 && prevGridY_ != -1 && (prevGridX_ != gridX || prevGridY_ != gridY)) {
                                            int x0 = prevGridX_;
                                            int y0 = prevGridY_;
                                            int x1 = gridX;
                                            int y1 = gridY;
                                            int dx = std::abs(x1 - x0);
                                            int dy = std::abs(y1 - y0);
                                            int sx = x0 < x1 ? 1 : -1;
                                            int sy = y0 < y1 ? 1 : -1;
                                            int err = (dx > dy ? dx : -dy) / 2;
                                            int e2;

                                            while (true) {
                                                if (x0 >= 0 && x0 < mapWidth && y0 >= 0 && y0 < mapHeight) {
                                                    if (std::find(pendingBlocks_.begin(), pendingBlocks_.end(), std::make_pair(x0, y0)) == pendingBlocks_.end()) {
                                                        pendingBlocks_.push_back({x0, y0});
                                                    }
                                                }
                                                if (x0 == x1 && y0 == y1) break;
                                                e2 = err;
                                                if (e2 > -dx) { err -= dy; x0 += sx; }
                                                if (e2 < dy) { err += dx; y0 += sy; }
                                            }
                                        }
                                        prevGridX_ = gridX;
                                        prevGridY_ = gridY;
                                    }
                                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                        if (!pendingBlocks_.empty()) {
                                            BeginMapHistoryCapture(mapChip);
                                            for (const auto& pos : pendingBlocks_) {
                                                if (mapChip->GetChip(pos.first, pos.second) != static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_)) {
                                                    mapChip->SetChip(pos.first, pos.second, static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_));
                                                }
                                            }
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty(); // Greedy Meshingの再計算
                                            pendingBlocks_.clear();
                                        }
                                        prevGridX_ = -1;
                                        prevGridY_ = -1;
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::Select) {
                                    bool isInsideSelection = false;
                                    if (selectStartX_ != -1 && selectStartY_ != -1 && selectEndX_ != -1 && selectEndY_ != -1) {
                                        int minX = (std::min)(selectStartX_, selectEndX_);
                                        int maxX = (std::max)(selectStartX_, selectEndX_);
                                        int minY = (std::min)(selectStartY_, selectEndY_);
                                        int maxY = (std::max)(selectStartY_, selectEndY_);
                                        if (gridX >= minX && gridX <= maxX && gridY >= minY && gridY <= maxY) {
                                            isInsideSelection = true;
                                        }
                                    }

                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                        if (isInsideSelection) {
                                            isDraggingSelection_ = true;
                                            dragStartGridX_ = gridX;
                                            dragStartGridY_ = gridY;
                                            
                                            // Copy the selection
                                            int minX = (std::min)(selectStartX_, selectEndX_);
                                            int maxX = (std::max)(selectStartX_, selectEndX_);
                                            int minY = (std::min)(selectStartY_, selectEndY_);
                                            int maxY = (std::max)(selectStartY_, selectEndY_);
                                            
                                            clipboardMapData_.clear();
                                            for (int y = minY; y <= maxY; ++y) {
                                                std::vector<int> row;
                                                for (int x = minX; x <= maxX; ++x) {
                                                    row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
                                                }
                                                clipboardMapData_.push_back(row);
                                            }
                                            BeginMapHistoryCapture(mapChip);
                                            // Clear original area
                                            for (int y = minY; y <= maxY; ++y) {
                                                for (int x = minX; x <= maxX; ++x) {
                                                    mapChip->SetChip(x, y, MapChip2D::ChipType::kNone);
                                                }
                                            }
                                        } else {
                                            selectStartX_ = gridX;
                                            selectStartY_ = gridY;
                                            selectEndX_ = gridX;
                                            selectEndY_ = gridY;
                                        }
                                    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                                        if (!isDraggingSelection_) {
                                            selectEndX_ = gridX;
                                            selectEndY_ = gridY;
                                        }
                                    } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                                        if (isDraggingSelection_) {
                                            int deltaX = gridX - dragStartGridX_;
                                            int deltaY = gridY - dragStartGridY_;
                                            
                                            int minX = (std::min)(selectStartX_, selectEndX_);
                                            int minY = (std::min)(selectStartY_, selectEndY_);
                                            
                                            // Paste to new location
                                            for (size_t r = 0; r < clipboardMapData_.size(); ++r) {
                                                for (size_t c = 0; c < clipboardMapData_[r].size(); ++c) {
                                                    int tx = minX + deltaX + static_cast<int>(c);
                                                    int ty = minY + deltaY + static_cast<int>(r);
                                                    if (tx >= 0 && tx < mapWidth && ty >= 0 && ty < mapHeight) {
                                                        if (clipboardMapData_[r][c] != static_cast<int>(MapChip2D::ChipType::kNone)) {
                                                            mapChip->SetChip(tx, ty, static_cast<MapChip2D::ChipType>(clipboardMapData_[r][c]));
                                                        }
                                                    }
                                                }
                                            }
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                            
                                            // Update selection rect
                                            selectStartX_ += deltaX;
                                            selectEndX_ += deltaX;
                                            selectStartY_ += deltaY;
                                            selectEndY_ += deltaY;
                                            
                                            isDraggingSelection_ = false;
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::Copy) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                        if (selectStartX_ != -1 && selectStartY_ != -1 && selectEndX_ != -1 && selectEndY_ != -1) {
                                            int minX = (std::max)(0, (std::min)(selectStartX_, selectEndX_));
                                            int maxX = (std::min)(mapWidth - 1, (std::max)(selectStartX_, selectEndX_));
                                            int minY = (std::max)(0, (std::min)(selectStartY_, selectEndY_));
                                            int maxY = (std::min)(mapHeight - 1, (std::max)(selectStartY_, selectEndY_));
                                            
                                            clipboardMapData_.clear();
                                            for (int y = minY; y <= maxY; ++y) {
                                                std::vector<int> row;
                                                for (int x = minX; x <= maxX; ++x) {
                                                    row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
                                                }
                                                clipboardMapData_.push_back(row);
                                            }
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::Paste) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inBounds) {
                                        if (!clipboardMapData_.empty()) {
                                            BeginMapHistoryCapture(mapChip);
                                            for (int y = 0; y < static_cast<int>(clipboardMapData_.size()); ++y) {
                                                for (int x = 0; x < static_cast<int>(clipboardMapData_[y].size()); ++x) {
                                                    int targetX = gridX + x;
                                                    int targetY = gridY + y;
                                                    if (targetX >= 0 && targetX < mapWidth && targetY >= 0 && targetY < mapHeight) {
                                                        mapChip->SetChip(targetX, targetY, static_cast<MapChip2D::ChipType>(clipboardMapData_[y][x]));
                                                    }
                                                }
                                            }
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                        }
                                    }
                                }
                                else if (mapEditMode_ == MapEditMode::BucketFill) {
                                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inBounds && isSelectableTool) {
                                        MapChip2D::ChipType targetType = mapChip->GetChip(gridX, gridY);
                                        MapChip2D::ChipType replacementType = static_cast<MapChip2D::ChipType>(mapEditorSelectedTool_);
                                        if (targetType != replacementType) {
                                            BeginMapHistoryCapture(mapChip);
                                            mapChip->BucketFill(gridX, gridY, targetType, replacementType);
                                            EndMapHistoryCapture(mapChip);
                                            mapChip->SetDirty();
                                        }
                                    }
                                }
                            }
                        }
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

    // --- 下部ペイン (通常: マップ設定 / リプレイ時: タイムライン / アニメーション時: ドープシート) ---
    if (showMapSettings_ || currentMode_ == EditorMode::Animation) {
        if (currentMode_ == EditorMode::Animation) {
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
                        mapEditorSelectedTool_ = 0;
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
        } else {
            if (ImGui::Begin("マップ設定", &showMapSettings_)) {
            IScene *activeScene = sceneManager->GetCurrentScene();
            if (activeScene) {
                MapChip2D* mapChip = activeScene->GetMapChip();
                if (mapChip) {
                    if (mapEditorInputWidth_ == -1) {
                        mapEditorInputWidth_ = mapChip->GetWidth();
                    }
                    if (mapEditorInputHeight_ == -1) {
                        mapEditorInputHeight_ = mapChip->GetHeight();
                    }

                    // ==========================================
                    // ここから移動してきたマップ設定（ファイル・サイズ）
                    // ==========================================
                    // json ディレクトリ内の .txt ファイルを自動走査
                    std::vector<std::string> stageFiles;
                    try {
                        if (std::filesystem::exists("resources/json/shared/MapData")) {
                            for (const auto& entry : std::filesystem::directory_iterator("resources/json/shared/MapData")) {
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
                        return std::string("resources/json/shared/MapData/") + name;
                    };

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

                        std::string comboPreview = (selectedFileIndex != -1) ? stageFiles[selectedFileIndex] : "既存のマップを選択...";
                        if (ImGui::BeginCombo("マップファイルを選択", comboPreview.c_str())) {
                            for (int i = 0; i < static_cast<int>(stageFiles.size()); ++i) {
                                bool isSelected = (selectedFileIndex == i);
                                if (ImGui::Selectable(stageFiles[i].c_str(), isSelected)) {
                                    strcpy_s(stageFilename_, sizeof(stageFilename_), stageFiles[i].c_str());
                                    selectedFileIndex = i;
                                    
                                    // 選択時に自動でロードする
                                    if (mapChip->LoadFromFile(GetFullFilePath(stageFilename_))) {
                                        mapEditorInputWidth_ = mapChip->GetWidth();
                                        mapEditorInputHeight_ = mapChip->GetHeight();
                                        UpdateAStarPositionsFromMap(mapChip, sceneManager);
                                    }
                                }
                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    // ファイル名入力 (Enterキーでロード)
                    if (ImGui::InputText("ファイル名", stageFilename_, sizeof(stageFilename_), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (mapChip->LoadFromFile(GetFullFilePath(stageFilename_))) {
                            mapEditorInputWidth_ = mapChip->GetWidth();
                            mapEditorInputHeight_ = mapChip->GetHeight();
                            UpdateAStarPositionsFromMap(mapChip, sceneManager);
                        }
                    }

                    ImGui::Spacing();
                    
                    // ルーム編集モード
                    ImGui::Checkbox("ルーム編集モード", &isRoomEditMode_);
                    if (isRoomEditMode_) {
                        ImGui::Text("左ドラッグ: マス目にスナップして作成・移動・リサイズ");
                        ImGui::Text("右ドラッグ: スナップなしで作成・移動・リサイズ");
                        ImGui::Text("Ctrl + クリック: ルームの削除");
                    }

                    ImGui::Separator();
                    
                    ImGui::Text("マップサイズ設定 (1画面＝ 幅:20, 高さ:11)");
                    ImGui::TextDisabled("※ 画面を増やしたい場合はサイズを広げてください");
                    
                    // マップサイズ入力と適用ボタン
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::InputInt("Width", &mapEditorInputWidth_);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::InputInt("Height", &mapEditorInputHeight_);
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Size")) {
                        BeginMapHistoryCapture(mapChip);
                        if (mapEditorInputWidth_ < 1) mapEditorInputWidth_ = 1;
                        if (mapEditorInputHeight_ < 1) mapEditorInputHeight_ = 1;
                        mapChip->Resize(mapEditorInputWidth_, mapEditorInputHeight_);
                        EndMapHistoryCapture(mapChip);
                    }

                    ImGui::Separator();

                    // 操作ボタン
                    if (ImGui::Button("保存")) {
                        mapChip->SaveToFile(GetFullFilePath(stageFilename_));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("クリア")) {
                        mapChip->ClearMap();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("初期化")) {
                        mapChip->ResetMap();
                        mapEditorInputWidth_ = mapChip->GetWidth();
                        mapEditorInputHeight_ = mapChip->GetHeight();
                        UpdateAStarPositionsFromMap(mapChip, sceneManager);
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    // ==========================================

                    // ペイントツール選択
                    static int selectedTool = 1; // 0 = None (Erase), 1 = Block (Paint), 2 = Death (DeathBlock), 3 = Goal, 4 = Coin
                    // ペイントツール選択 (ルーム編集モード中は操作不可にするかグレーアウトする)
                    ImGui::BeginDisabled(isRoomEditMode_);
                    ImGui::Text("Paint Tool:");
                    ImGui::Spacing();

                    struct ToolIcon {
                        int id;
                        std::string name;
                        ImVec4 color;
                        float scale; // 実際のモデルに合わせたサイズ比率
                    };

                    std::vector<ToolIcon> systemTools = {
                        { 6, "Spawn", ImVec4(0.2f, 0.6f, 1.0f, 1.0f), 1.0f },
                        { 10, "RoomSpawn", ImVec4(0.2f, 0.8f, 1.0f, 1.0f), 1.0f },
                        { 0, "Erase", ImVec4(0.2f, 0.2f, 0.2f, 1.0f), 1.0f }
                    };

                    std::vector<ToolIcon> templateTools;
                    for (const auto& def : mapChip->GetTemplatePalette()) {
                        templateTools.push_back({ def.id, def.name, ImVec4(def.color.x, def.color.y, def.color.z, def.color.w), 1.0f });
                    }

                    std::set<std::string> availableTypes;
                    for (const auto& def : mapChip->GetCustomPalette()) {
                        availableTypes.insert(def.type);
                    }

                    std::vector<ToolIcon> customTools;
                    for (const auto& def : mapChip->GetCustomPalette()) {
                        // customToolFilters_ は「非表示」にするタイプを格納するセットとして扱う
                        if (customToolFilters_.find(def.type) != customToolFilters_.end()) {
                            continue;
                        }
                        customTools.push_back({ def.id, def.name, ImVec4(def.color.x, def.color.y, def.color.z, def.color.w), 1.0f });
                    }

                    float itemSize = 64.0f; // アイコン枠のサイズ
                    float totalHeight = itemSize + 24.0f; // アイコン＋テキスト
                    float itemSpacing = ImGui::GetStyle().ItemSpacing.x;

                    static int toolToDelete = -1;
                    static bool openDeletePopup = false;

                    // ツール描画用の共通ラムダ
                    // sectionType: 0=System, 1=Template(Basic), 2=Custom
                    auto DrawTools = [&](const std::vector<ToolIcon>& tools, int sectionType) {
                        float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                        int numTools = static_cast<int>(tools.size());
                        // カスタムセクションの場合は最後に +追加 ボタンを描画するため +1 する
                        int maxIter = (sectionType == 2) ? numTools + 1 : numTools;

                        for (int i = 0; i < maxIter; i++) {
                            ImGui::PushID(sectionType * 1000 + i);
                            
                            ImVec2 p = ImGui::GetCursorScreenPos();
                            float lastButtonX2 = 0.0f;
                            
                            if (i < numTools) {
                                const ToolIcon& tool = tools[i];
                                bool isSelected = (mapEditorSelectedTool_ == tool.id);

                                // 当たり判定 (InvisibleButton)
                                ImGui::SetNextItemAllowOverlap();
                                if (ImGui::InvisibleButton("##Tool", ImVec2(itemSize, totalHeight))) {
                                    mapEditorSelectedTool_ = tool.id;
                                    selectedGameObject_ = nullptr;
                                    selectedObject_ = nullptr;
                                    selectedParticle_ = nullptr;
                                    selectedPrimitive_ = nullptr;
                                }
                                
                                lastButtonX2 = ImGui::GetItemRectMax().x;
                                ImVec2 backupCursorPos = ImGui::GetCursorScreenPos();

                                bool isHovered = ImGui::IsItemHovered();
                                ImDrawList* drawList = ImGui::GetWindowDrawList();

                                // 背景 (ホバー時)
                                if (isHovered) {
                                    drawList->AddRectFilled(p, ImVec2(p.x + itemSize, p.y + totalHeight), IM_COL32(80, 80, 80, 255), 4.0f);
                                }

                                // アイコン (四角形 - スケールを適用)
                                float iconPadding = 4.0f;
                                float actualIconSize = (itemSize - iconPadding * 2) * tool.scale;
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
                                ImVec2 textSize = ImGui::CalcTextSize(tool.name.c_str());
                                float textX = p.x + (itemSize - textSize.x) * 0.5f;
                                drawList->AddText(ImVec2(textX, p.y + itemSize + 2.0f), IM_COL32(255, 255, 255, 255), tool.name.c_str());

                                // 選択中の黄色の枠線
                                if (isSelected && sectionType != 1) { // テンプレートは選択状態の枠線を表示しない
                                    drawList->AddRect(p, ImVec2(p.x + itemSize, p.y + totalHeight), IM_COL32(255, 255, 0, 255), 4.0f, 0, 2.0f);
                                }

                                // Custom Tools セクションの場合はドラッグ＆ドロップ並べ替えに対応
                                if (sectionType == 2) {
                                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                                        ImGui::SetDragDropPayload("DND_CUSTOM_TOOL", &tool.id, sizeof(int));
                                        ImGui::Text("Move %s", tool.name.c_str());
                                        ImGui::EndDragDropSource();
                                    }
                                    if (ImGui::BeginDragDropTarget()) {
                                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_CUSTOM_TOOL")) {
                                            IM_ASSERT(payload->DataSize == sizeof(int));
                                            int payload_id = *(const int*)payload->Data;
                                            int target_id = tool.id;
                                            
                                            auto& palette = mapChip->GetCustomPalette();
                                            int srcIdx = -1, dstIdx = -1;
                                            for (size_t k = 0; k < palette.size(); ++k) {
                                                if (palette[k].id == payload_id) srcIdx = static_cast<int>(k);
                                                if (palette[k].id == target_id) dstIdx = static_cast<int>(k);
                                            }
                                            if (srcIdx != -1 && dstIdx != -1 && srcIdx != dstIdx) {
                                                auto item = palette[srcIdx];
                                                palette.erase(palette.begin() + srcIdx);
                                                if (srcIdx < dstIdx) dstIdx--; // 要素が詰められる分を補正
                                                palette.insert(palette.begin() + dstIdx, item);
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }
                                    
                                    ImGui::SetCursorScreenPos(ImVec2(p.x + itemSize - 16.0f, p.y));
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    if (ImGui::Button(("X##del_" + std::to_string(tool.id)).c_str(), ImVec2(16, 16))) {
                                        toolToDelete = tool.id;
                                        openDeletePopup = true;
                                    }
                                    ImGui::PopStyleVar();
                                    ImGui::PopStyleColor();
                                    
                                    // カーソル位置を元の正しい位置に復帰させる
                                    ImGui::SetCursorScreenPos(backupCursorPos);
                                }
                            } else if (sectionType == 2) {
                                // "＋ 追加" ボタン
                                if (ImGui::Button("＋\n追加", ImVec2(itemSize, totalHeight))) {
                                    auto& palette = mapChip->GetCustomPalette();
                                    MapChip2D::CustomBlockDef newDef;
                                    newDef.id = 100 + static_cast<int>(palette.size());
                                    newDef.name = "Custom " + std::to_string(palette.size() + 1);
                                    newDef.type = "NormalBlock";
                                    palette.push_back(newDef);
                                    mapEditorSelectedTool_ = newDef.id; // 新しいものを選択状態にする
                                    selectedGameObject_ = nullptr;
                                    selectedObject_ = nullptr;
                                    selectedParticle_ = nullptr;
                                    selectedPrimitive_ = nullptr;
                                    mapChip->SaveToFile(GetFullFilePath(stageFilename_));
                                }
                                lastButtonX2 = ImGui::GetItemRectMax().x;
                            }

                            ImGui::PopID();

                            // 折り返し処理 (ウィンドウ幅を超える場合は次の行へ)
                            float nextButtonX2 = lastButtonX2 + itemSpacing + itemSize;
                            if (nextButtonX2 < windowVisibleX && i + 1 < maxIter) {
                                ImGui::SameLine();
                            }
                        }
                        if (maxIter > 0) {
                            ImGui::NewLine(); // 最後の項目の後に改行を入れる
                        }
                    };

                    if (ImGui::CollapsingHeader("System Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
                        DrawTools(systemTools, 0);
                    }
                    if (ImGui::CollapsingHeader("Basic Tools (Settings)", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::TextDisabled("※これらのブロックはマップ設定用のテンプレートです。（直接設置はできません）");
                        DrawTools(templateTools, 1);
                    }
                    if (ImGui::CollapsingHeader("Custom Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::Button("フィルター設定...")) {
                            ImGui::OpenPopup("FilterPopup");
                        }
                        
                        if (ImGui::BeginPopup("FilterPopup")) {
                            ImGui::Text("表示するブロックの種類:");
                            ImGui::Separator();
                            for (const auto& type : availableTypes) {
                                bool isChecked = (customToolFilters_.find(type) == customToolFilters_.end()); // 見つからなければ表示(true)
                                if (ImGui::Checkbox(type.c_str(), &isChecked)) {
                                    if (isChecked) {
                                        customToolFilters_.erase(type); // チェックされた＝表示する＝非表示リストから削除
                                    } else {
                                        customToolFilters_.insert(type); // チェック外れた＝非表示リストに追加
                                    }
                                }
                            }
                            ImGui::Separator();
                            if (ImGui::Button("閉じる", ImVec2(120, 0))) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::Spacing();

                        DrawTools(customTools, 2);

                        if (openDeletePopup) {
                            ImGui::OpenPopup("DeleteConfirmPopup");
                            openDeletePopup = false;
                        }
                        if (ImGui::BeginPopupModal("DeleteConfirmPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                            ImGui::Text("本当に削除しますか？\n(マップ上で使用されている場合はエラーになる可能性があります)");
                            ImGui::Separator();
                            if (ImGui::Button("はい", ImVec2(120, 0))) {
                                auto& palette = mapChip->GetCustomPalette();
                                auto it = std::remove_if(palette.begin(), palette.end(), [&](const MapChip2D::CustomBlockDef& d) { return d.id == toolToDelete; });
                                palette.erase(it, palette.end());
                                mapChip->SaveToFile(GetFullFilePath(stageFilename_));
                                toolToDelete = -1;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SetItemDefaultFocus();
                            ImGui::SameLine();
                            if (ImGui::Button("いいえ", ImVec2(120, 0))) {
                                toolToDelete = -1;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }

                    ImGui::Spacing();
                    ImGui::EndDisabled();

                    // 2Dグリッド描画のUIは削除されました
                } else {
                    ImGui::Text("現在のアクティブシーンは2Dマップ編集をサポートしていません。");
                }
            } else {
                ImGui::Text("アクティブなシーンがありません。");
            }
        }
        ImGui::End();
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
}

void EditorManager::Draw() {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectXCommon::GetInstance()->GetCommandList());
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
        ofs << "{" << std::endl;
        ofs << "  \"currentScene\": \"" << SceneFactory::GetSceneTypeName(currentSceneType_) << "\"," << std::endl;
        ofs << "  \"isOutlineEnabled\": " << (dxCommon->IsOutlineEnabled() ? "true" : "false") << "," << std::endl;
        ofs << "  \"outlineThickness\": " << dxCommon->GetOutlineThickness() << std::endl;
        ofs << "}" << std::endl;
        ofs.close();
    }
}

void EditorManager::LoadSceneConfig() {
    std::ifstream ifs("resources/json/local/editor_config.json");
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

void EditorManager::SaveLightingConfig(ModelCommon* modelCommon) {
    std::filesystem::create_directories("resources/json/shared");
    std::ofstream ofs("resources/json/shared/lighting_config.json");
    if (ofs.is_open()) {
        nlohmann::json j;
        j["activeLightType"] = activeLightType_;
        j["enableFog"] = enableFog_;
        j["enableFlatShading"] = enableFlatShading_;
        j["dIntensity"] = dIntensity_;
        j["pIntensity"] = pIntensity_;
        j["sIntensity"] = sIntensity_;
        j["spotAngleDeg"] = spotAngleDeg_;
        j["spotFalloffDeg"] = spotFalloffDeg_;

        if (modelCommon) {
            auto d = modelCommon->GetDirectionalLight();
            j["dLight"]["color"] = {d->color.x, d->color.y, d->color.z, d->color.w};
            j["dLight"]["direction"] = {d->direction.x, d->direction.y, d->direction.z};
            
            auto p = modelCommon->GetPointLight();
            j["pLight"]["color"] = {p->color.x, p->color.y, p->color.z, p->color.w};
            j["pLight"]["position"] = {p->position.x, p->position.y, p->position.z};
            j["pLight"]["radius"] = p->radius;
            j["pLight"]["decay"] = p->decay;

            auto s = modelCommon->GetSpotLight();
            j["sLight"]["color"] = {s->color.x, s->color.y, s->color.z, s->color.w};
            j["sLight"]["position"] = {s->position.x, s->position.y, s->position.z};
            j["sLight"]["direction"] = {s->direction.x, s->direction.y, s->direction.z};
            j["sLight"]["distance"] = s->distance;
            j["sLight"]["decay"] = s->decay;
        }

        ofs << j.dump(4);
        ofs.close();
    }
}

void EditorManager::LoadLightingConfig(ModelCommon* modelCommon) {
    std::ifstream ifs("resources/json/shared/lighting_config.json");
    if (!ifs.is_open()) return;

    try {
        nlohmann::json j;
        ifs >> j;
        if (j.contains("activeLightType")) activeLightType_ = j["activeLightType"];
        if (j.contains("enableFog")) enableFog_ = j["enableFog"];
        if (j.contains("enableFlatShading")) enableFlatShading_ = j["enableFlatShading"];
        if (j.contains("dIntensity")) dIntensity_ = j["dIntensity"];
        if (j.contains("pIntensity")) pIntensity_ = j["pIntensity"];
        if (j.contains("sIntensity")) sIntensity_ = j["sIntensity"];
        if (j.contains("spotAngleDeg")) spotAngleDeg_ = j["spotAngleDeg"];
        if (j.contains("spotFalloffDeg")) spotFalloffDeg_ = j["spotFalloffDeg"];

        if (modelCommon) {
            auto d = modelCommon->GetDirectionalLight();
            if (j.contains("dLight")) {
                if (j["dLight"].contains("color")) {
                    d->color = {j["dLight"]["color"][0], j["dLight"]["color"][1], j["dLight"]["color"][2], j["dLight"]["color"][3]};
                }
                if (j["dLight"].contains("direction")) {
                    d->direction = {j["dLight"]["direction"][0], j["dLight"]["direction"][1], j["dLight"]["direction"][2]};
                }
            }
            d->enableFlatShading = enableFlatShading_ ? 1 : 0;
            
            auto p = modelCommon->GetPointLight();
            if (j.contains("pLight")) {
                if (j["pLight"].contains("color")) p->color = {j["pLight"]["color"][0], j["pLight"]["color"][1], j["pLight"]["color"][2], j["pLight"]["color"][3]};
                if (j["pLight"].contains("position")) p->position = {j["pLight"]["position"][0], j["pLight"]["position"][1], j["pLight"]["position"][2]};
                if (j["pLight"].contains("radius")) p->radius = j["pLight"]["radius"];
                if (j["pLight"].contains("decay")) p->decay = j["pLight"]["decay"];
            }

            auto s = modelCommon->GetSpotLight();
            if (j.contains("sLight")) {
                if (j["sLight"].contains("color")) s->color = {j["sLight"]["color"][0], j["sLight"]["color"][1], j["sLight"]["color"][2], j["sLight"]["color"][3]};
                if (j["sLight"].contains("position")) s->position = {j["sLight"]["position"][0], j["sLight"]["position"][1], j["sLight"]["position"][2]};
                if (j["sLight"].contains("direction")) s->direction = {j["sLight"]["direction"][0], j["sLight"]["direction"][1], j["sLight"]["direction"][2]};
                if (j["sLight"].contains("distance")) s->distance = j["sLight"]["distance"];
                if (j["sLight"].contains("decay")) s->decay = j["sLight"]["decay"];
            }
            
            // intensity の反映
            if (activeLightType_ == 0) {
                d->intensity = dIntensity_;
                p->intensity = 0.0f;
                s->intensity = 0.0f;
            } else if (activeLightType_ == 1) {
                d->intensity = 0.0f;
                p->intensity = pIntensity_;
                s->intensity = 0.0f;
            } else if (activeLightType_ == 2) {
                d->intensity = 0.0f;
                p->intensity = 0.0f;
                s->intensity = sIntensity_;
                s->cosAngle = std::cos(spotAngleDeg_ * static_cast<float>(M_PI) / 180.0f);
                s->cosFalloffStart = std::cos(spotFalloffDeg_ * static_cast<float>(M_PI) / 180.0f);
            }
        }
    } catch (...) {}
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

class MapEditCommand : public EditorManager::IEditorCommand {
    MapChip2D* mapChip_;
    EditorManager::MapState oldState_;
    EditorManager::MapState newState_;
public:
    MapEditCommand(MapChip2D* chip, const EditorManager::MapState& oldS, const EditorManager::MapState& newS)
        : mapChip_(chip), oldState_(oldS), newState_(newS) {}
    void Undo() override {
        mapChip_->Resize(oldState_.width, oldState_.height);
        for (int y = 0; y < oldState_.height; ++y) {
            for (int x = 0; x < oldState_.width; ++x) {
                mapChip_->SetChip(x, y, static_cast<MapChip2D::ChipType>(oldState_.data[y][x]));
            }
        }
    }
    void Redo() override {
        mapChip_->Resize(newState_.width, newState_.height);
        for (int y = 0; y < newState_.height; ++y) {
            for (int x = 0; x < newState_.width; ++x) {
                mapChip_->SetChip(x, y, static_cast<MapChip2D::ChipType>(newState_.data[y][x]));
            }
        }
    }
};

class RoomEditCommand : public EditorManager::IEditorCommand {
    MapChip2D* mapChip_;
    EditorManager::RoomState oldState_;
    EditorManager::RoomState newState_;
public:
    RoomEditCommand(MapChip2D* chip, const EditorManager::RoomState& oldS, const EditorManager::RoomState& newS)
        : mapChip_(chip), oldState_(oldS), newState_(newS) {}
    void Undo() override {
        mapChip_->GetRooms() = oldState_.rooms;
    }
    void Redo() override {
        mapChip_->GetRooms() = newState_.rooms;
    }
};

static void CaptureMapState(MapChip2D* mapChip, EditorManager::MapState& state) {
    state.width = mapChip->GetWidth();
    state.height = mapChip->GetHeight();
    state.data.clear();
    for (int y = 0; y < state.height; ++y) {
        std::vector<int> row;
        for (int x = 0; x < state.width; ++x) {
            row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
        }
        state.data.push_back(row);
    }
}

void EditorManager::BeginMapHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    CaptureMapState(mapChip, oldMapState_);
}

void EditorManager::EndMapHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    MapState newState;
    CaptureMapState(mapChip, newState);
    // 変化があればコマンドを積む
    if (oldMapState_.width != newState.width || oldMapState_.height != newState.height || oldMapState_.data != newState.data) {
        PushCommand(std::make_shared<MapEditCommand>(mapChip, oldMapState_, newState));
    }
}

void EditorManager::BeginRoomHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    oldRoomState_.rooms = mapChip->GetRooms();
}

void EditorManager::EndRoomHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    RoomState newState;
    newState.rooms = mapChip->GetRooms();
    bool changed = false;
    if (oldRoomState_.rooms.size() != newState.rooms.size()) {
        changed = true;
    } else {
        for (size_t i = 0; i < newState.rooms.size(); ++i) {
            if (oldRoomState_.rooms[i].x != newState.rooms[i].x ||
                oldRoomState_.rooms[i].y != newState.rooms[i].y ||
                oldRoomState_.rooms[i].width != newState.rooms[i].width ||
                oldRoomState_.rooms[i].height != newState.rooms[i].height) {
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        PushCommand(std::make_shared<RoomEditCommand>(mapChip, oldRoomState_, newState));
    }
}

void EditorManager::UpdateAStarPositionsFromMap(MapChip2D* mapChip, SceneManager* sceneManager) {
    if (!mapChip) return;

    float halfChip = mapChip->GetChipSize() * 0.5f;
    bool foundSpawn = false;

    // 1. 最優先: シーンの GetPlayer() から Player2D の位置を取得
    IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;

    if (activeScene) {
        Player2D* player = activeScene->GetPlayer();
        if (player) {
            Vector3 pos = player->GetStartPosition();
            if (pos.x != 0.0f || pos.y != 0.0f) {
                aStarStartPos_[0] = pos.x;
                aStarStartPos_[1] = pos.y;
                foundSpawn = true;
            } else {
                pos = player->GetPosition();
                if (pos.x != 0.0f || pos.y != 0.0f) {
                    aStarStartPos_[0] = pos.x;
                    aStarStartPos_[1] = pos.y;
                    foundSpawn = true;
                }
            }
        }
    }

    // 2. もし Player2D オブジェクトが見つからない場合、MapChip2D 上の kPlayerSpawn (スポーンブロック) を検索
    if (!foundSpawn && mapChip->HasPlayerSpawn()) {
        Vector3 spawnWorldPos = mapChip->GetPlayerSpawnWorldPosition();
        aStarStartPos_[0] = spawnWorldPos.x;
        aStarStartPos_[1] = spawnWorldPos.y;
        foundSpawn = true;
    }

    // 3. それでも見つからない場合、ReplayManager の playerInitPos を参照（0,0でない場合）
    if (!foundSpawn) {
        const auto& currentReplay = ReplayManager::GetInstance()->GetCurrentReplay();
        if (currentReplay.playerInitPos.x != 0.0f || currentReplay.playerInitPos.y != 0.0f) {
            aStarStartPos_[0] = currentReplay.playerInitPos.x;
            aStarStartPos_[1] = currentReplay.playerInitPos.y;
            foundSpawn = true;
        }
    }

    // 4. デフォルト位置へのフォールバック (2.0, 5.0)
    if (!foundSpawn) {
        aStarStartPos_[0] = 2.0f;
        aStarStartPos_[1] = 5.0f;
    }

    // 5. ゴール座標 (kGoal) の検索
    bool foundGoal = false;
    for (int y = 0; y < mapChip->GetHeight(); ++y) {
        for (int x = 0; x < mapChip->GetWidth(); ++x) {
            if (mapChip->GetChipType(x, y) == MapChip2D::ChipType::kGoal) {
                aStarGoalPos_[0] = mapChip->ChipToWorldX(x) + halfChip;
                aStarGoalPos_[1] = mapChip->ChipToWorldY(y) + halfChip;
                foundGoal = true;
                break;
            }
        }
        if (foundGoal) break;
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
    ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_main);
    ImGui::DockBuilderDockWindow("リプレイエディター", dock_id_main);
    ImGui::DockBuilderDockWindow("アニメーションエディター", dock_id_main);

    // 左側
    ImGui::DockBuilderDockWindow("ヒエラルキー", dock_id_left);
    ImGui::DockBuilderDockWindow("マイメディア (リプレイ履歴)", dock_id_left);

    // 右側
    ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
    ImGui::DockBuilderDockWindow("ポストエフェクト", dock_id_right);

    // 下側
    ImGui::DockBuilderDockWindow("マップ設定", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ステージセレクトエディター", dock_id_bottom);
    ImGui::DockBuilderDockWindow("タイムライン", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ドープシート (タイムライン)", dock_id_bottom);
    ImGui::DockBuilderDockWindow("ログ (Log Window)", dock_id_bottom);

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