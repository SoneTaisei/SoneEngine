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
                if (isAnimationScenePushed_) {
                    sceneManager->PopScene();
                    isAnimationScenePushed_ = false;
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
                if (isAnimationScenePushed_) {
                    sceneManager->PopScene();
                    isAnimationScenePushed_ = false;
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
    isAnimationEditorHovered_ = false;
    if (showAnimEditor_) {
        if (ImGui::Begin("アニメーションエディター", &showAnimEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowAppearing()) {
                currentMode_ = EditorMode::Animation;
                if (!isAnimationScenePushed_) {
                    sceneManager->PushScene(std::make_unique<AnimationPreviewScene>());
                    isAnimationScenePushed_ = true;
                    RefreshAnimationJointList(sceneManager);
                }
            }
            isAnimationEditorHovered_ = ImGui::IsWindowHovered();
            DrawAnimationEditorMainView(sceneManager, activeCamera, renderTextureSrvHandle);
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
                            RefreshAnimationJointList(sceneManager);
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
                                RefreshAnimationJointList(sceneManager);
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
                                RefreshAnimationJointList(sceneManager);
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
                    if (!currentJointList_.empty()) {
                        ImGui::Spacing();
                        ImGui::Separator();
                        if (ImGui::TreeNodeEx("[Bones] ボーン / 関節", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (const auto& jointName : currentJointList_) {
                                bool isJointSelected = (animEditorSelectedJointName_ == jointName);
                                if (ImGui::Selectable(("  " + jointName).c_str(), isJointSelected)) {
                                    animEditorSelectedJointName_ = jointName;
                                    animEditorSelectedKeyIndex_ = -1;
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
                DrawAnimationInspectorUI(sceneManager);
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

                            const char* types[] = { "NormalBlock", "DeathBlock", "GoalBlock", "CoinBlock", "OneWayBlock", "LiftBlock", "RailBlock", "JumpBlock", "PatrolEnemyBlock" };
                            int currentType = -1;
                            for (int i = 0; i < 9; ++i) {
                                if (targetDef->type == types[i]) {
                                    currentType = i;
                                    break;
                                }
                            }
                            if (ImGui::Combo("種類 (Type)", &currentType, types, 9)) {
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
            DrawAnimationDopeSheetUI(sceneManager);
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

                    std::vector<ToolIcon> templateTools = {
                        { 1, "Block", ImVec4(0.3f, 0.7f, 0.3f, 1.0f), 1.0f },
                        { 2, "Death", ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 1.0f },
                        { 3, "Goal",  ImVec4(0.8f, 0.2f, 0.8f, 1.0f), 1.0f },
                        { 4, "Coin",  ImVec4(1.0f, 0.8f, 0.0f, 1.0f), 0.5f },
                        { 5, "OneWay",ImVec4(0.4f, 0.8f, 0.8f, 1.0f), 1.0f },
                        { 7, "Lift",  ImVec4(0.9f, 0.6f, 0.1f, 1.0f), 1.0f },
                        { 8, "Rail",  ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 1.0f },
                        { 9, "Jump",  ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 1.0f },
                        { 12, "Enemy", ImVec4(0.9f, 0.2f, 0.2f, 1.0f), 1.0f }
                    };

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
                                    newDef.type = "JumpBlock";
                                    newDef.properties["jumpVelocityVertical"] = 15.0f;
                                    newDef.properties["jumpVelocityHorizontal"] = 15.0f;
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

    UpdateAnimationPosePreview(sceneManager);

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

AnimatorComponent* EditorManager::GetTargetAnimator(SceneManager* sceneManager) {
    if (selectedGameObject_) {
        auto* a = selectedGameObject_->GetComponent<AnimatorComponent>();
        if (a) return a;
    }
    if (selectedObject_) {
        auto* a = selectedObject_->GetAnimator();
        if (a) return a;
    }
    if (sceneManager && sceneManager->GetCurrentScene()) {
        auto* scene = sceneManager->GetCurrentScene();
        for (auto& go : scene->GetGameObjects()) {
            if (go) {
                auto* a = go->GetComponent<AnimatorComponent>();
                if (a && a->HasSkeleton()) return a;
            }
        }
        for (auto* obj : scene->GetObjects()) {
            if (obj && obj->GetAnimator() && obj->GetAnimator()->HasSkeleton()) {
                return obj->GetAnimator();
            }
        }
        auto* player = scene->GetPlayer();
        if (player && player->GetAnimator()) {
            return player->GetAnimator();
        }
    }
    return nullptr;
}

void EditorManager::RefreshAnimationJointList(SceneManager* sceneManager) {
    animJointTreeNodes_.clear();
    animJointRootIndices_.clear();
    currentJointList_.clear();
    
    // 1. シーン内のスケルトンを持つAnimatorを検索
    AnimatorComponent* animator = GetTargetAnimator(sceneManager);
    
    if (animator && animator->HasSkeleton()) {
        const auto& joints = animator->GetSkeleton().joints;
        int32_t numJoints = static_cast<int32_t>(joints.size());
        animJointTreeNodes_.resize(numJoints);

        for (int32_t i = 0; i < numJoints; ++i) {
            const auto& j = joints[i];
            currentJointList_.push_back(j.name);

            animJointTreeNodes_[i].name = j.name;
            animJointTreeNodes_[i].jointIndex = j.index;
            animJointTreeNodes_[i].parentIndex = j.parent.has_value() ? *j.parent : -1;
            animJointTreeNodes_[i].children = j.children;
            animJointTreeNodes_[i].depth = 0;
        }

        // 親を持たないジョイント（ルート候補）を収集
        for (int32_t i = 0; i < numJoints; ++i) {
            if (animJointTreeNodes_[i].parentIndex < 0) {
                animJointRootIndices_.push_back(i);
            }
        }
        if (animJointRootIndices_.empty() && animator->GetSkeleton().root >= 0 && animator->GetSkeleton().root < numJoints) {
            animJointRootIndices_.push_back(animator->GetSkeleton().root);
        } else if (animJointRootIndices_.empty() && numJoints > 0) {
            animJointRootIndices_.push_back(0);
        }

        // 各ノードの深さ (depth) を再帰的に計算
        std::function<void(int32_t, int)> calcDepth = [&](int32_t nodeIdx, int d) {
            if (nodeIdx < 0 || nodeIdx >= numJoints) return;
            animJointTreeNodes_[nodeIdx].depth = d;
            for (int32_t childIdx : animJointTreeNodes_[nodeIdx].children) {
                calcDepth(childIdx, d + 1);
            }
        };
        for (int32_t rootIdx : animJointRootIndices_) {
            calcDepth(rootIdx, 0);
        }

        // 開閉フラグの初期化（未設定のノードについて設定: ルートのみ展開し、子は閉じる）
        for (int32_t i = 0; i < numJoints; ++i) {
            const std::string& name = animJointTreeNodes_[i].name;
            if (animJointExpanded_.find(name) == animJointExpanded_.end()) {
                bool isRoot = (animJointTreeNodes_[i].parentIndex < 0);
                animJointExpanded_[name] = isRoot;
            }
        }
    }
    
    // フォールバック（デフォルトジョイント）
    if (currentJointList_.empty()) {
        static const char* defaultJoints[] = {
            "Hips_01",
            "LeftArm_09",
            "RightArm_014",
            "LeftForeArm_010",
            "RightForeArm_015",
            "LeftUpLeg_019",
            "RightUpLeg_024",
            "LeftLeg_020",
            "RightLeg_025",
            "LeftFoot_021",
            "RightFoot_026"
        };
        for (int i = 0; i < 11; ++i) {
            std::string name = defaultJoints[i];
            currentJointList_.push_back(name);

            AnimJointTreeNode node;
            node.name = name;
            node.jointIndex = i;
            node.parentIndex = (i == 0) ? -1 : 0;
            node.depth = (i == 0) ? 0 : 1;
            if (i == 0) {
                for (int c = 1; c < 11; ++c) node.children.push_back(c);
                animJointRootIndices_.push_back(0);
            }
            animJointTreeNodes_.push_back(node);
            if (animJointExpanded_.find(name) == animJointExpanded_.end()) {
                animJointExpanded_[name] = (i == 0);
            }
        }
    }
    
    // 選択中ジョイント名がリストにない場合はリストの先頭にする
    bool found = false;
    for (const auto& name : currentJointList_) {
        if (name == animEditorSelectedJointName_) {
            found = true;
            break;
        }
    }
    if (!found && !currentJointList_.empty()) {
        animEditorSelectedJointName_ = currentJointList_[0];
    }
}

void EditorManager::ScanAnimationFiles() {
    availableAnimationFiles_.clear();
    const std::string animDir = "resources/json/shared/Player";
    std::filesystem::create_directories(animDir);

    // 既知の標準プリセットファイルが存在しなければ作成
    std::string wallClimbPath = animDir + "/wall_climb_animation.json";
    std::string airDashPath = animDir + "/air_dash_animation.json";

    if (!std::filesystem::exists(wallClimbPath)) {
        SaveAnimationToJsonFile(CreateDefaultWallClimbAnimation(), wallClimbPath);
    }
    if (!std::filesystem::exists(airDashPath)) {
        SaveAnimationToJsonFile(CreateDefaultAirDashAnimation(), airDashPath);
    }

    // ディレクトリ内のすべての.jsonファイルを列挙
    for (const auto& entry : std::filesystem::directory_iterator(animDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string filename = entry.path().filename().string();
            if (filename == "player_parameters.json") continue;

            std::string fullPath = entry.path().string();
            std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
            availableAnimationFiles_.push_back(fullPath);
        }
    }

    // ソート
    std::sort(availableAnimationFiles_.begin(), availableAnimationFiles_.end());

    // 現在のファイルパスがリストにない場合は先頭に設定
    if (std::find(availableAnimationFiles_.begin(), availableAnimationFiles_.end(), currentAnimFilePath_) == availableAnimationFiles_.end()) {
        if (!availableAnimationFiles_.empty()) {
            currentAnimFilePath_ = availableAnimationFiles_[0];
        } else {
            currentAnimFilePath_ = wallClimbPath;
            availableAnimationFiles_.push_back(wallClimbPath);
        }
    }
}

void EditorManager::UpdateAnimationPosePreview(SceneManager* sceneManager) {
    if (!sceneManager || !sceneManager->GetCurrentScene()) return;
    
    if (!animEditorInitialized_) {
        ScanAnimationFiles();
        if (!LoadAnimationFromJsonFile(editingAnimation_, currentAnimFilePath_)) {
            if (currentAnimFilePath_.find("wall_climb") != std::string::npos) {
                editingAnimation_ = CreateDefaultWallClimbAnimation();
            } else if (currentAnimFilePath_.find("air_dash") != std::string::npos) {
                editingAnimation_ = CreateDefaultAirDashAnimation();
            }
        }
        animEditorInitialized_ = true;
    }

    AnimatorComponent* animator = GetTargetAnimator(sceneManager);
    if (!animator) return;
    
    // 再生中の時間更新
    if (animEditorPlaying_) {
        animTempOverrides_.clear();
        float dt = TimeManager::GetInstance().GetDeltaTime();
        animEditorTime_ += dt;
        if (editingAnimation_.duration > 0.0f) {
            if (animEditorTime_ >= editingAnimation_.duration) {
                if (animEditorLoop_) {
                    animEditorTime_ = std::fmod(animEditorTime_, editingAnimation_.duration);
                } else {
                    animEditorTime_ = editingAnimation_.duration;
                    animEditorPlaying_ = false;
                }
            }
        }
    }
    
    // アニメーションをモデルに適用（アニメーションモード時または編集中）
    if (currentMode_ == EditorMode::Animation || isPlaying_ || showAnimEditor_) {
        animator->ClearJointOverrides();
        for (const auto& [nodeName, nodeAnim] : editingAnimation_.nodeAnimations) {
            if (!nodeAnim.rotate.empty()) {
                Quaternion rot = CalculateValue(nodeAnim.rotate, animEditorTime_);
                animator->SetJointRotationOverride(nodeName, rot, 1.0f);
            }
            if (!nodeAnim.translate.empty()) {
                Vector3 trans = CalculateValue(nodeAnim.translate, animEditorTime_);
                animator->SetJointTranslationOverride(nodeName, trans, 1.0f);
            }
            if (!nodeAnim.scale.empty()) {
                Vector3 sc = CalculateValue(nodeAnim.scale, animEditorTime_);
                animator->SetJointScaleOverride(nodeName, sc, 1.0f);
            }
        }
        // 未挿入時の一時プレビュー値を上書き反映（キー未登録でも画面上で動く）
        for (const auto& [nodeName, ov] : animTempOverrides_) {
            if (ov.rotate) animator->SetJointRotationOverride(nodeName, *ov.rotate, 1.0f);
            if (ov.translate) animator->SetJointTranslationOverride(nodeName, *ov.translate, 1.0f);
            if (ov.scale) animator->SetJointScaleOverride(nodeName, *ov.scale, 1.0f);
        }
        animator->UpdateSkeletonAndSkinCluster();
    }
}

void EditorManager::PushAnimUndoState(const std::string& desc) {
    AnimEditorSnapshot snap;
    snap.animation = editingAnimation_;
    snap.time = animEditorTime_;
    snap.selectedJointName = animEditorSelectedJointName_;
    snap.selectedKeyIndex = animEditorSelectedKeyIndex_;
    snap.description = desc;

    animUndoStack_.push_back(snap);
    if (animUndoStack_.size() > 64) {
        animUndoStack_.erase(animUndoStack_.begin());
    }
    animRedoStack_.clear();
}

void EditorManager::PerformAnimUndo(SceneManager* sceneManager) {
    if (animUndoStack_.empty()) return;

    // 現在の状態をRedoスタックにプッシュ
    AnimEditorSnapshot curSnap;
    curSnap.animation = editingAnimation_;
    curSnap.time = animEditorTime_;
    curSnap.selectedJointName = animEditorSelectedJointName_;
    curSnap.selectedKeyIndex = animEditorSelectedKeyIndex_;
    curSnap.description = "Current";
    animRedoStack_.push_back(curSnap);

    // Undoスタックから最新のスナップショットを取り出して適用
    AnimEditorSnapshot prevSnap = animUndoStack_.back();
    animUndoStack_.pop_back();

    editingAnimation_ = prevSnap.animation;
    animEditorTime_ = prevSnap.time;
    animEditorSelectedJointName_ = prevSnap.selectedJointName;
    animEditorSelectedKeyIndex_ = prevSnap.selectedKeyIndex;

    animTempOverrides_.clear();
    UpdateAnimationPosePreview(sceneManager);
}

void EditorManager::PerformAnimRedo(SceneManager* sceneManager) {
    if (animRedoStack_.empty()) return;

    // 現在の状態をUndoスタックにプッシュ
    AnimEditorSnapshot curSnap;
    curSnap.animation = editingAnimation_;
    curSnap.time = animEditorTime_;
    curSnap.selectedJointName = animEditorSelectedJointName_;
    curSnap.selectedKeyIndex = animEditorSelectedKeyIndex_;
    curSnap.description = "Current";
    animUndoStack_.push_back(curSnap);

    // Redoスタックから最新のスナップショットを取り出して適用
    AnimEditorSnapshot nextSnap = animRedoStack_.back();
    animRedoStack_.pop_back();

    editingAnimation_ = nextSnap.animation;
    animEditorTime_ = nextSnap.time;
    animEditorSelectedJointName_ = nextSnap.selectedJointName;
    animEditorSelectedKeyIndex_ = nextSnap.selectedKeyIndex;

    animTempOverrides_.clear();
    UpdateAnimationPosePreview(sceneManager);
}

void EditorManager::ClearAnimUndoRedo() {
    animUndoStack_.clear();
    animRedoStack_.clear();
    hasAnimDragPreSnapshot_ = false;
}

void EditorManager::DrawAnimationViewportGrid(const Matrix4x4& viewProjectionMatrix, ImVec2 vpPos, ImVec2 vpSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    const float gridExtent = 12.0f;
    const float gridStep = 1.0f;
    const float nearW = 0.05f; // Nearクリップ閾値
    const float gridY = 0.005f; // 床オブジェクトとの干渉を避ける高さ

    // 同次クリップ座標の計算
    auto transformToClip = [&](const Vector3& p, Vector4& outClip) {
        outClip.x = p.x * viewProjectionMatrix.m[0][0] + p.y * viewProjectionMatrix.m[1][0] + p.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
        outClip.y = p.x * viewProjectionMatrix.m[0][1] + p.y * viewProjectionMatrix.m[1][1] + p.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
        outClip.z = p.x * viewProjectionMatrix.m[0][2] + p.y * viewProjectionMatrix.m[1][2] + p.z * viewProjectionMatrix.m[2][2] + viewProjectionMatrix.m[3][2];
        outClip.w = p.x * viewProjectionMatrix.m[0][3] + p.y * viewProjectionMatrix.m[1][3] + p.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
    };

    // クリップ座標からスクリーン座標への変換
    auto clipToScreen = [&](const Vector4& clip, ImVec2& outP) {
        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;
        outP.x = vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x;
        outP.y = vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y;
    };

    // 3D線分のクリッピング＆描画
    auto drawSegment3D = [&](Vector3 p1, Vector3 p2, ImU32 col, float thickness) {
        Vector4 c1, c2;
        transformToClip(p1, c1);
        transformToClip(p2, c2);

        // 両方ともカメラ背後の場合はスキップ
        if (c1.w < nearW && c2.w < nearW) return;

        // 片方がカメラ背後にある場合、Near平面 (w = nearW) でクリップ
        if (c1.w < nearW) {
            float t = (nearW - c1.w) / (c2.w - c1.w);
            p1 = { p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t, p1.z + (p2.z - p1.z) * t };
            transformToClip(p1, c1);
        } else if (c2.w < nearW) {
            float t = (nearW - c2.w) / (c1.w - c2.w);
            p2 = { p2.x + (p1.x - p2.x) * t, p2.y + (p1.y - p2.y) * t, p2.z + (p1.z - p2.z) * t };
            transformToClip(p2, c2);
        }

        if (c1.w < nearW || c2.w < nearW) return;

        ImVec2 s1, s2;
        clipToScreen(c1, s1);
        clipToScreen(c2, s2);
        drawList->AddLine(s1, s2, col, thickness);
    };

    // 1. 通常グリッド線 (XZ平面) - 1.0mセグメント分割で安定描画
    ImU32 gridCol = IM_COL32(75, 75, 80, 140);
    for (float x = -gridExtent; x <= gridExtent; x += gridStep) {
        if (std::abs(x) < 0.001f) continue; // Z軸(x=0)は後で強調描画
        for (float z = -gridExtent; z < gridExtent; z += gridStep) {
            drawSegment3D(Vector3{ x, gridY, z }, Vector3{ x, gridY, z + gridStep }, gridCol, 1.0f);
        }
    }
    for (float z = -gridExtent; z <= gridExtent; z += gridStep) {
        if (std::abs(z) < 0.001f) continue; // X軸(z=0)は後で強調描画
        for (float x = -gridExtent; x < gridExtent; x += gridStep) {
            drawSegment3D(Vector3{ x, gridY, z }, Vector3{ x + gridStep, gridY, z }, gridCol, 1.0f);
        }
    }

    // 2. 0のライン強調 (X軸: 赤, Z軸: 青/緑) - 1.0mセグメント分割で安定描画
    ImU32 xAxisCol = IM_COL32(230, 60, 75, 230);
    for (float x = -gridExtent; x < gridExtent; x += gridStep) {
        drawSegment3D(Vector3{ x, gridY, 0.0f }, Vector3{ x + gridStep, gridY, 0.0f }, xAxisCol, 2.0f);
    }

    ImU32 zAxisCol = IM_COL32(60, 140, 230, 230);
    for (float z = -gridExtent; z < gridExtent; z += gridStep) {
        drawSegment3D(Vector3{ 0.0f, gridY, z }, Vector3{ 0.0f, gridY, z + gridStep }, zAxisCol, 2.0f);
    }

    drawList->PopClipRect();
}

void EditorManager::DrawCameraOrientationGizmo(Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!activeCamera) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(vpPos.x + vpSize.x - 55.0f, vpPos.y + 55.0f);
    const float radius = 36.0f;
    const float badgeRadius = 9.5f;

    // 背景の円形プレート（半透明）
    drawList->AddCircleFilled(center, radius + 12.0f, IM_COL32(30, 30, 35, 130), 32);

    Matrix4x4 viewMat = activeCamera->GetViewMatrix();

    // 6本の軸定義 (ワールド方向, 正/負, 色, ラベル, スナップ時の目標回転, 目標位置オフセット方向)
    struct AxisInfo {
        Vector3 worldDir;
        bool isPositive;
        ImU32 color;
        ImU32 ringColor;
        char label;
        Vector3 snapRotate;
        Vector3 camOffsetDir;
        Vector3 camDir;
        ImVec2 screenPos;
        bool isHovered;
    };

    const float PI_VAL = 3.1415926535f;
    const float HALF_PI = 1.5707963268f;

    AxisInfo axes[6] = {
        // +X: 右側面 (Right) - カメラは+X側に位置し、-X方向を向く -> Y回転: -90°
        { {  1.0f,  0.0f,  0.0f }, true,  IM_COL32(235, 65,  75,  255), IM_COL32(235, 65,  75,  180), 'X', { 0.0f, -HALF_PI, 0.0f }, {  1.0f, 0.0f, 0.0f }, {}, {}, false },
        // -X: 左側面 (Left) - カメラは-X側に位置し、+X方向を向く -> Y回転: +90°
        { { -1.0f,  0.0f,  0.0f }, false, IM_COL32(235, 65,  75,  140), IM_COL32(235, 65,  75,  200), ' ', { 0.0f,  HALF_PI, 0.0f }, { -1.0f, 0.0f, 0.0f }, {}, {}, false },
        // +Y: 上面 (Top) - カメラは+Y側に位置し、真下を向く -> X回転: +90°
        { {  0.0f,  1.0f,  0.0f }, true,  IM_COL32(130, 200, 45,  255), IM_COL32(130, 200, 45,  180), 'Y', { HALF_PI - 0.001f, 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0001f }, {}, {}, false },
        // -Y: 底面 (Bottom) - カメラは-Y側に位置し、真上を向く -> X回転: -90°
        { {  0.0f, -1.0f,  0.0f }, false, IM_COL32(130, 200, 45,  140), IM_COL32(130, 200, 45,  200), ' ', { -HALF_PI + 0.001f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0001f }, {}, {}, false },
        // +Z: 正面 (Front) - カメラは-Z側に位置し、+Z方向(手前)を向く -> Y回転: 0°
        { {  0.0f,  0.0f,  1.0f }, true,  IM_COL32(50,  135, 245, 255), IM_COL32(50,  135, 245, 180), 'Z', { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, {}, {}, false },
        // -Z: 背面 (Back) - カメラは+Z側に位置し、-Z方向(奥)を向く -> Y回転: 180°
        { {  0.0f,  0.0f, -1.0f }, false, IM_COL32(50,  135, 245, 140), IM_COL32(50,  135, 245, 200), ' ', { 0.0f, PI_VAL, 0.0f }, { 0.0f, 0.0f,  1.0f }, {}, {}, false }
    };

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    // カメラ座標系への変換 (ViewMatrixの回転成分を適用)
    for (int i = 0; i < 6; ++i) {
        axes[i].camDir.x = axes[i].worldDir.x * viewMat.m[0][0] + axes[i].worldDir.y * viewMat.m[1][0] + axes[i].worldDir.z * viewMat.m[2][0];
        axes[i].camDir.y = axes[i].worldDir.x * viewMat.m[0][1] + axes[i].worldDir.y * viewMat.m[1][1] + axes[i].worldDir.z * viewMat.m[2][1];
        axes[i].camDir.z = axes[i].worldDir.x * viewMat.m[0][2] + axes[i].worldDir.y * viewMat.m[1][2] + axes[i].worldDir.z * viewMat.m[2][2];

        axes[i].screenPos = ImVec2(
            center.x + axes[i].camDir.x * radius,
            center.y - axes[i].camDir.y * radius
        );
    }

    // 深度(camDir.z)の昇順(奥 -> 手前)にソート
    std::vector<int> sortedIndices = { 0, 1, 2, 3, 4, 5 };
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
        return axes[a].camDir.z < axes[b].camDir.z;
    });

    // ホバー＆クリック判定（手前側にある軸から優先して判定）
    int clickedAxisIdx = -1;
    for (int i = 5; i >= 0; --i) {
        int idx = sortedIndices[i];
        float dx = mousePos.x - axes[idx].screenPos.x;
        float dy = mousePos.y - axes[idx].screenPos.y;
        float distSq = dx * dx + dy * dy;
        float hitRadius = badgeRadius + 2.5f;
        if (distSq <= hitRadius * hitRadius) {
            axes[idx].isHovered = true;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                clickedAxisIdx = idx;
            }
            break;
        }
    }

    // 軸スナップの開始 (クリック時 - 線形補間アニメーション)
    if (clickedAxisIdx >= 0) {
        // 現在すでにクリックした軸の正面を向いている場合、反対側の軸（+X<->-X, +Y<->-Y, +Z<->-Z）にトグル反転
        if (axes[clickedAxisIdx].camDir.z > 0.70f) {
            int oppIdx = (clickedAxisIdx % 2 == 0) ? (clickedAxisIdx + 1) : (clickedAxisIdx - 1);
            clickedAxisIdx = oppIdx;
        }

        const auto& snapAxis = axes[clickedAxisIdx];
        
        // 注視点 (モデル中心: 0, 1.0, 0)
        Vector3 target = { 0.0f, 1.0f, 0.0f };
        Vector3 curCamPos = activeCamera->GetTranslation();
        Vector3 diff = { curCamPos.x - target.x, curCamPos.y - target.y, curCamPos.z - target.z };
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (dist < 2.0f || dist > 30.0f) dist = 6.0f; // 適切な距離にフォールバック

        cameraSnapStartRot_ = activeCamera->GetRotation();
        cameraSnapStartPos_ = curCamPos;

        // 角度の最短経路補間（-PI〜+PIの差分調整）
        auto normalizeAngleDiff = [](float start, float target) {
            float diff = target - start;
            while (diff > 3.14159265f) { start += 6.2831853f; diff = target - start; }
            while (diff < -3.14159265f) { start -= 6.2831853f; diff = target - start; }
            return start;
        };
        cameraSnapStartRot_.x = normalizeAngleDiff(cameraSnapStartRot_.x, snapAxis.snapRotate.x);
        cameraSnapStartRot_.y = normalizeAngleDiff(cameraSnapStartRot_.y, snapAxis.snapRotate.y);
        cameraSnapStartRot_.z = normalizeAngleDiff(cameraSnapStartRot_.z, snapAxis.snapRotate.z);

        cameraSnapEndRot_ = snapAxis.snapRotate;
        cameraSnapEndPos_ = {
            target.x + snapAxis.camOffsetDir.x * dist,
            target.y + snapAxis.camOffsetDir.y * dist,
            target.z + snapAxis.camOffsetDir.z * dist
        };
        cameraSnapLerpTimer_ = 0.0f;
        cameraSnapLerpDuration_ = 0.25f;
        isCameraSnapLerping_ = true;
    }

    // 描画: 奥から手前へ
    for (int idx : sortedIndices) {
        const auto& axis = axes[idx];
        float curRadius = axis.isHovered ? (badgeRadius + 1.5f) : badgeRadius;

        if (!axis.isPositive) {
            // 負方向軸: 半透明のリング（円周のみ）
            if (axis.isHovered) {
                drawList->AddCircleFilled(axis.screenPos, curRadius * 0.75f, axis.color, 16);
                drawList->AddCircle(axis.screenPos, curRadius * 0.75f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            } else {
                drawList->AddCircle(axis.screenPos, curRadius * 0.75f, axis.ringColor, 16, 1.5f);
            }
        } else {
            // 正方向軸: 中心から軸への線分
            drawList->AddLine(center, axis.screenPos, axis.color, 2.5f);

            // 塗りつぶしバッジ円
            drawList->AddCircleFilled(axis.screenPos, curRadius, axis.color, 16);
            if (axis.isHovered) {
                drawList->AddCircle(axis.screenPos, curRadius, IM_COL32(255, 255, 255, 255), 16, 2.0f);
            } else {
                drawList->AddCircle(axis.screenPos, curRadius, IM_COL32(255, 255, 255, 120), 16, 1.0f);
            }

            // 文字 ('X', 'Y', 'Z')
            char txt[2] = { axis.label, '\0' };
            ImVec2 txtSz = ImGui::CalcTextSize(txt);
            drawList->AddText(ImVec2(axis.screenPos.x - txtSz.x * 0.5f, axis.screenPos.y - txtSz.y * 0.5f), IM_COL32(255, 255, 255, 255), txt);
        }
    }
}

void EditorManager::DrawSkeletonJointsOverlay(SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!sceneManager || !activeCamera) return;

    AnimatorComponent* animator = GetTargetAnimator(sceneManager);
    Matrix4x4 worldMatrix = TransformFunctions::MakeIdentity4x4();

    if (selectedGameObject_) {
        auto* tr = selectedGameObject_->GetComponent<TransformComponent>();
        if (tr) worldMatrix = tr->GetWorldMatrix();
    } else if (selectedObject_) {
        worldMatrix = TransformFunctions::MakeAffineMatrix(
            selectedObject_->GetScale(),
            selectedObject_->GetRotation(),
            selectedObject_->GetTranslation()
        );
    } else if (sceneManager && sceneManager->GetCurrentScene()) {
        auto* scene = sceneManager->GetCurrentScene();
        for (auto& go : scene->GetGameObjects()) {
            if (go && go->GetComponent<AnimatorComponent>() == animator) {
                auto* tr = go->GetComponent<TransformComponent>();
                if (tr) worldMatrix = tr->GetWorldMatrix();
                break;
            }
        }
    }

    if (!animator || !animator->HasSkeleton()) return;

    const Skeleton& skeleton = animator->GetSkeleton();
    if (skeleton.joints.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    Matrix4x4 vpMat = TransformFunctions::Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

    // 各ジョイントのスクリーン座標を事前計算
    struct JointScreenInfo {
        Vector3 worldPos;
        ImVec2 screenPos;
        bool isVisible;
        float depth;
    };
    std::vector<JointScreenInfo> screenJoints(skeleton.joints.size());

    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        const auto& joint = skeleton.joints[i];
        // 親階層の回転・平行移動を含む完全なワールド変換行列
        Matrix4x4 jointWorld = TransformFunctions::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
        Vector3 worldPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

        screenJoints[i].worldPos = worldPos;

        Vector4 clip;
        clip.x = worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0];
        clip.y = worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1];
        clip.z = worldPos.x * vpMat.m[0][2] + worldPos.y * vpMat.m[1][2] + worldPos.z * vpMat.m[2][2] + vpMat.m[3][2];
        clip.w = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];

        if (clip.w > 0.05f) {
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            screenJoints[i].screenPos = ImVec2(
                vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x,
                vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y
            );
            screenJoints[i].isVisible = true;
            screenJoints[i].depth = clip.w;
        } else {
            screenJoints[i].isVisible = false;
        }
    }

    // 1. 親子間のボーン接続線を描画
    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        const auto& joint = skeleton.joints[i];
        if (joint.parent.has_value()) {
            int32_t pIdx = joint.parent.value();
            if (pIdx >= 0 && pIdx < static_cast<int32_t>(screenJoints.size())) {
                if (screenJoints[i].isVisible && screenJoints[pIdx].isVisible) {
                    bool isConnectedToSelected = (joint.name == animEditorSelectedJointName_ || skeleton.joints[pIdx].name == animEditorSelectedJointName_);
                    ImU32 boneCol = isConnectedToSelected ? IM_COL32(255, 220, 80, 230) : IM_COL32(140, 210, 255, 150);
                    float thickness = isConnectedToSelected ? 3.0f : 1.8f;
                    drawList->AddLine(screenJoints[pIdx].screenPos, screenJoints[i].screenPos, boneCol, thickness);
                }
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    int hoveredJointIdx = -1;
    float closestDistSq = 20.0f * 20.0f; // クリック判定半径

    // 2. 非選択ジョイントの丸を描画 & ホバー判定
    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        if (!screenJoints[i].isVisible) continue;
        const auto& joint = skeleton.joints[i];
        bool isSelected = (joint.name == animEditorSelectedJointName_);

        ImVec2 p = screenJoints[i].screenPos;
        float dx = mousePos.x - p.x;
        float dy = mousePos.y - p.y;
        float dSq = dx * dx + dy * dy;

        if (dSq < closestDistSq) {
            closestDistSq = dSq;
            hoveredJointIdx = static_cast<int>(i);
        }

        if (!isSelected) {
            // パキッとした水色サークル＋白い核＋濃い縁取り
            drawList->AddCircleFilled(p, 5.0f, IM_COL32(60, 170, 255, 230), 16);
            drawList->AddCircleFilled(p, 2.0f, IM_COL32(255, 255, 255, 255), 10);
            drawList->AddCircle(p, 5.0f, IM_COL32(20, 50, 100, 240), 16, 1.2f);
        }
    }

    // ホバー時のハイライトとクリック選択（ロック中でない場合のみ他ボーンを選択可能）
    if (hoveredJointIdx >= 0 && !isDraggingAnimGizmo_ && !isAnimLocked_) {
        const auto& hJoint = skeleton.joints[hoveredJointIdx];
        if (hJoint.name != animEditorSelectedJointName_) {
            ImVec2 hp = screenJoints[hoveredJointIdx].screenPos;
            drawList->AddCircle(hp, 9.0f, IM_COL32(255, 255, 255, 230), 16, 2.0f);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && animGizmoActiveAxis_ < 0) {
            animEditorSelectedJointName_ = hJoint.name;
            animEditorSelectedKeyIndex_ = -1;
        }
    }

    // 3. 選択中のジョイントを最前面に強調描画
    auto it = skeleton.jointMap.find(animEditorSelectedJointName_);
    if (it == skeleton.jointMap.end() && !skeleton.joints.empty()) {
        // 見つからない場合は先頭のボーンにフォールバック
        animEditorSelectedJointName_ = skeleton.joints[0].name;
        it = skeleton.jointMap.find(animEditorSelectedJointName_);
    }

    if (it != skeleton.jointMap.end() && it->second < screenJoints.size()) {
        int selIdx = it->second;
        if (screenJoints[selIdx].isVisible) {
            ImVec2 sp = screenJoints[selIdx].screenPos;

            // パルスアニメーション効果
            static float pulseTimer = 0.0f;
            pulseTimer += (io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f) * 5.0f;
            float pulseOffset = std::sin(pulseTimer) * 2.0f;

            // 外側の強調リング
            drawList->AddCircle(sp, 12.0f + pulseOffset, IM_COL32(255, 255, 255, 240), 24, 2.5f);
            drawList->AddCircle(sp, 15.5f + pulseOffset, IM_COL32(255, 200, 30, 160), 24, 1.2f);

            // 中心の黄色い丸
            drawList->AddCircleFilled(sp, 8.0f, IM_COL32(255, 200, 30, 255), 20);
            drawList->AddCircleFilled(sp, 3.5f, IM_COL32(255, 255, 255, 255), 12);
            drawList->AddCircle(sp, 8.0f, IM_COL32(180, 110, 0, 255), 20, 1.5f);

            // ボーン名のテキストラベル（右上に黒半透明背景付き）
            std::string label = animEditorSelectedJointName_ + (isAnimLocked_ ? " [Locked]" : "");
            ImVec2 txtSz = ImGui::CalcTextSize(label.c_str());
            ImVec2 boxMin = ImVec2(sp.x + 12.0f, sp.y - txtSz.y * 0.5f - 4.0f);
            ImVec2 boxMax = ImVec2(boxMin.x + txtSz.x + 10.0f, boxMin.y + txtSz.y + 8.0f);

            drawList->AddRectFilled(boxMin, boxMax, IM_COL32(15, 15, 20, 230), 4.0f);
            drawList->AddRect(boxMin, boxMax, isAnimLocked_ ? IM_COL32(255, 90, 90, 220) : IM_COL32(255, 205, 40, 220), 4.0f, 0, 1.5f);
            drawList->AddText(ImVec2(boxMin.x + 5.0f, boxMin.y + 4.0f), isAnimLocked_ ? IM_COL32(255, 140, 140, 255) : IM_COL32(255, 240, 120, 255), label.c_str());
        }
    }

    drawList->PopClipRect();
}

inline Quaternion MatrixToQuaternion(const Matrix4x4& m) {
    float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
    Quaternion q;
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m.m[1][2] - m.m[2][1]) / s;
        q.y = (m.m[2][0] - m.m[0][2]) / s;
        q.z = (m.m[0][1] - m.m[1][0]) / s;
    } else if ((m.m[0][0] > m.m[1][1]) && (m.m[0][0] > m.m[2][2])) {
        float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f;
        q.w = (m.m[1][2] - m.m[2][1]) / s;
        q.x = 0.25f * s;
        q.y = (m.m[0][1] + m.m[1][0]) / s;
        q.z = (m.m[0][2] + m.m[2][0]) / s;
    } else if (m.m[1][1] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f;
        q.w = (m.m[2][0] - m.m[0][2]) / s;
        q.x = (m.m[0][1] + m.m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[1][2] + m.m[2][1]) / s;
    } else {
        float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f;
        q.w = (m.m[0][1] - m.m[1][0]) / s;
        q.x = (m.m[0][2] + m.m[2][0]) / s;
        q.y = (m.m[1][2] + m.m[2][1]) / s;
        q.z = 0.25f * s;
    }
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-5f) {
        q.x /= len; q.y /= len; q.z /= len; q.w /= len;
    }
    return q;
}

inline void DecomposeAffineMatrix(const Matrix4x4& m, Vector3& outT, Quaternion& outR, Vector3& outS) {
    outT = Vector3{ m.m[3][0], m.m[3][1], m.m[3][2] };

    Vector3 row0 = { m.m[0][0], m.m[0][1], m.m[0][2] };
    Vector3 row1 = { m.m[1][0], m.m[1][1], m.m[1][2] };
    Vector3 row2 = { m.m[2][0], m.m[2][1], m.m[2][2] };

    outS.x = std::sqrt(row0.x * row0.x + row0.y * row0.y + row0.z * row0.z);
    outS.y = std::sqrt(row1.x * row1.x + row1.y * row1.y + row1.z * row1.z);
    outS.z = std::sqrt(row2.x * row2.x + row2.y * row2.y + row2.z * row2.z);

    float sx = (outS.x > 1e-6f) ? (1.0f / outS.x) : 1.0f;
    float sy = (outS.y > 1e-6f) ? (1.0f / outS.y) : 1.0f;
    float sz = (outS.z > 1e-6f) ? (1.0f / outS.z) : 1.0f;

    Matrix4x4 rotMat = TransformFunctions::MakeIdentity4x4();
    rotMat.m[0][0] = row0.x * sx; rotMat.m[0][1] = row0.y * sx; rotMat.m[0][2] = row0.z * sx;
    rotMat.m[1][0] = row1.x * sy; rotMat.m[1][1] = row1.y * sy; rotMat.m[1][2] = row1.z * sy;
    rotMat.m[2][0] = row2.x * sz; rotMat.m[2][1] = row2.y * sz; rotMat.m[2][2] = row2.z * sz;

    Vector3 cross01 = TransformFunctions::Cross(
        Vector3{ rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] },
        Vector3{ rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] }
    );
    float det = cross01.x * rotMat.m[2][0] + cross01.y * rotMat.m[2][1] + cross01.z * rotMat.m[2][2];
    if (det < 0.0f) {
        rotMat.m[2][0] = -rotMat.m[2][0];
        rotMat.m[2][1] = -rotMat.m[2][1];
        rotMat.m[2][2] = -rotMat.m[2][2];
    }

    outR = MatrixToQuaternion(rotMat);
}

inline bool ComputeBlenderSymmetrySRT(
    const Skeleton& skeleton,
    const std::string& srcJointName,
    const std::string& oppJointName,
    const Vector3& srcScale,
    const Quaternion& srcRot,
    const Vector3& srcTrans,
    bool axisX,
    bool axisY,
    bool axisZ,
    Vector3& outOppScale,
    Quaternion& outOppRot,
    Vector3& outOppTrans)
{
    auto itA = skeleton.jointMap.find(srcJointName);
    auto itB = skeleton.jointMap.find(oppJointName);
    if (itA == skeleton.jointMap.end() || itB == skeleton.jointMap.end()) return false;

    const Joint& jointA = skeleton.joints[itA->second];
    const Joint& jointB = skeleton.joints[itB->second];

    // 1. レストポーズのモデル空間行列（バインドポーズ）
    Matrix4x4 mRestA = jointA.skeletonSpaceMatrix;
    Matrix4x4 mRestB = jointB.skeletonSpaceMatrix;

    // 2. 現在のボーンAのモデル空間行列 M_currA を計算 (行ベクトル形式: local * parent)
    Matrix4x4 mLocalA = TransformFunctions::MakeAffineMatrix(srcScale, srcRot, srcTrans);
    Matrix4x4 mParentA = TransformFunctions::MakeIdentity4x4();
    if (jointA.parent.has_value() && jointA.parent.value() >= 0 && jointA.parent.value() < static_cast<int32_t>(skeleton.joints.size())) {
        mParentA = skeleton.joints[jointA.parent.value()].skeletonSpaceMatrix;
    }
    Matrix4x4 mCurrentA = mLocalA * mParentA;

    // 3. ボーンAの変位行列 DeltaA = inv(mRestA) * mCurrentA
    Matrix4x4 invRestA = TransformFunctions::Inverse(mRestA);
    Matrix4x4 deltaA = invRestA * mCurrentA;

    // 4. 反射行列 Sx
    Vector3 reflScale = { 1.0f, 1.0f, 1.0f };
    if (axisX) reflScale.x = -1.0f; // X軸対称 (YZ平面反射)
    if (axisY) reflScale.y = -1.0f; // Y軸対称 (XZ平面反射)
    if (axisZ) reflScale.z = -1.0f; // Z軸対称 (XY平面反射)
    Matrix4x4 mSx = TransformFunctions::MakeScaleMatrix(reflScale);

    // 5. 鏡映変位 DeltaB = Sx * DeltaA * Sx
    Matrix4x4 deltaB = mSx * deltaA * mSx;

    // 6. ボーンBの目標モデル空間行列 M_currB = mRestB * deltaB
    Matrix4x4 mCurrentB = mRestB * deltaB;

    // 7. ボーンBの親空間ローカル行列へ逆変換
    Matrix4x4 mParentB = TransformFunctions::MakeIdentity4x4();
    if (jointB.parent.has_value() && jointB.parent.value() >= 0 && jointB.parent.value() < static_cast<int32_t>(skeleton.joints.size())) {
        mParentB = skeleton.joints[jointB.parent.value()].skeletonSpaceMatrix;
    }
    Matrix4x4 invParentB = TransformFunctions::Inverse(mParentB);
    Matrix4x4 mLocalB = mCurrentB * invParentB;

    // 8. ローカル行列から SRT を分解
    DecomposeAffineMatrix(mLocalB, outOppTrans, outOppRot, outOppScale);
    return true;
}

void EditorManager::DrawBoneTransformGizmo(SceneManager* sceneManager, Camera* activeCamera, ImVec2 vpPos, ImVec2 vpSize) {
    if (!sceneManager || !activeCamera) return;

    AnimatorComponent* animator = GetTargetAnimator(sceneManager);
    if (!animator || !animator->HasSkeleton()) return;

    const Skeleton& skeleton = animator->GetSkeleton();
    auto it = skeleton.jointMap.find(animEditorSelectedJointName_);
    if (it == skeleton.jointMap.end()) return;

    int32_t jointIdx = it->second;
    if (jointIdx < 0 || jointIdx >= static_cast<int32_t>(skeleton.joints.size())) return;

    const Joint& joint = skeleton.joints[jointIdx];

    Matrix4x4 worldMatrix = TransformFunctions::MakeIdentity4x4();
    if (selectedGameObject_) {
        auto* tr = selectedGameObject_->GetComponent<TransformComponent>();
        if (tr) worldMatrix = tr->GetWorldMatrix();
    } else if (selectedObject_) {
        worldMatrix = TransformFunctions::MakeAffineMatrix(
            selectedObject_->GetScale(),
            selectedObject_->GetRotation(),
            selectedObject_->GetTranslation()
        );
    } else if (sceneManager && sceneManager->GetCurrentScene()) {
        for (auto& go : sceneManager->GetCurrentScene()->GetGameObjects()) {
            if (go && go->GetComponent<AnimatorComponent>() == animator) {
                auto* tr = go->GetComponent<TransformComponent>();
                if (tr) worldMatrix = tr->GetWorldMatrix();
                break;
            }
        }
    }

    Matrix4x4 jointWorld = TransformFunctions::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
    Vector3 origin = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

    Matrix4x4 vpMat = TransformFunctions::Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());

    Vector4 clipOrigin;
    clipOrigin.x = origin.x * vpMat.m[0][0] + origin.y * vpMat.m[1][0] + origin.z * vpMat.m[2][0] + vpMat.m[3][0];
    clipOrigin.y = origin.x * vpMat.m[0][1] + origin.y * vpMat.m[1][1] + origin.z * vpMat.m[2][1] + vpMat.m[3][1];
    clipOrigin.z = origin.x * vpMat.m[0][2] + origin.y * vpMat.m[1][2] + origin.z * vpMat.m[2][2] + vpMat.m[3][2];
    clipOrigin.w = origin.x * vpMat.m[0][3] + origin.y * vpMat.m[1][3] + origin.z * vpMat.m[2][3] + vpMat.m[3][3];

    if (clipOrigin.w <= 0.05f) return;

    float ndcX = clipOrigin.x / clipOrigin.w;
    float ndcY = clipOrigin.y / clipOrigin.w;
    ImVec2 screenOrigin = ImVec2(
        vpPos.x + (ndcX + 1.0f) * 0.5f * vpSize.x,
        vpPos.y + (1.0f - ndcY) * 0.5f * vpSize.y
    );

    float gizmoRadius = clipOrigin.w * 0.18f;

    Vector3 axes[3] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };

    if (animGizmoSpace_ == 0 && animGizmoMode_ != 1) { // Local space (Rotation Mode 1 は常にワールド固定軸で表示)
        Vector3 lx = { jointWorld.m[0][0], jointWorld.m[0][1], jointWorld.m[0][2] };
        Vector3 ly = { jointWorld.m[1][0], jointWorld.m[1][1], jointWorld.m[1][2] };
        Vector3 lz = { jointWorld.m[2][0], jointWorld.m[2][1], jointWorld.m[2][2] };
        float lenX = std::sqrt(lx.x * lx.x + lx.y * lx.y + lx.z * lx.z); if (lenX > 1e-5f) axes[0] = lx * (1.0f / lenX);
        float lenY = std::sqrt(ly.x * ly.x + ly.y * ly.y + ly.z * ly.z); if (lenY > 1e-5f) axes[1] = ly * (1.0f / lenY);
        float lenZ = std::sqrt(lz.x * lz.x + lz.y * lz.y + lz.z * lz.z); if (lenZ > 1e-5f) axes[2] = lz * (1.0f / lenZ);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), true);

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    auto project = [&](const Vector3& p, ImVec2& outP, float& outW) -> bool {
        Vector4 c;
        c.x = p.x * vpMat.m[0][0] + p.y * vpMat.m[1][0] + p.z * vpMat.m[2][0] + vpMat.m[3][0];
        c.y = p.x * vpMat.m[0][1] + p.y * vpMat.m[1][1] + p.z * vpMat.m[2][1] + vpMat.m[3][1];
        c.z = p.x * vpMat.m[0][2] + p.y * vpMat.m[1][2] + p.z * vpMat.m[2][2] + vpMat.m[3][2];
        c.w = p.x * vpMat.m[0][3] + p.y * vpMat.m[1][3] + p.z * vpMat.m[2][3] + vpMat.m[3][3];
        outW = c.w;
        if (c.w <= 0.05f) return false;
        outP.x = vpPos.x + (c.x / c.w + 1.0f) * 0.5f * vpSize.x;
        outP.y = vpPos.y + (1.0f - c.y / c.w) * 0.5f * vpSize.y;
        return true;
    };

    auto distToSegment = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
        float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
        if (l2 < 1e-4f) return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
        float t = std::clamp(((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2, 0.0f, 1.0f);
        ImVec2 proj = ImVec2(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y));
        return std::sqrt((p.x - proj.x) * (p.x - proj.x) + (p.y - proj.y) * (p.y - proj.y));
    };

    const ImU32 axisColors[3] = {
        IM_COL32(235, 60, 60, 240),
        IM_COL32(60, 220, 60, 240),
        IM_COL32(60, 140, 255, 240)
    };
    const ImU32 axisHoverColors[3] = {
        IM_COL32(255, 140, 140, 255),
        IM_COL32(140, 255, 140, 255),
        IM_COL32(140, 200, 255, 255)
    };

    NodeAnimation& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];

    bool hasAnySymmetryAxis = animSymmetryAxisX_ || animSymmetryAxisY_ || animSymmetryAxisZ_;
    std::string gizmoOppJoint = (animSymmetryMode_ && hasAnySymmetryAxis) ? FindOppositeJointName(animEditorSelectedJointName_, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, &skeleton) : "";
    auto syncGizmoOppositeSRT = [&](const Vector3* newT, const Quaternion* newR, const Vector3* newS) {
        if (!animSymmetryMode_ || !hasAnySymmetryAxis || gizmoOppJoint.empty() || gizmoOppJoint == animEditorSelectedJointName_) return;

        Vector3 curS = joint.defaultTransform.scale;
        if (!nodeAnim.scale.empty()) curS = CalculateValue(nodeAnim.scale, animEditorTime_);
        auto itS = animTempOverrides_.find(animEditorSelectedJointName_);
        if (itS != animTempOverrides_.end() && itS->second.scale) curS = *itS->second.scale;

        Quaternion curR = joint.defaultTransform.rotate;
        if (!nodeAnim.rotate.empty()) curR = CalculateValue(nodeAnim.rotate, animEditorTime_);
        auto itR = animTempOverrides_.find(animEditorSelectedJointName_);
        if (itR != animTempOverrides_.end() && itR->second.rotate) curR = *itR->second.rotate;

        Vector3 curT = joint.defaultTransform.translate;
        if (!nodeAnim.translate.empty()) curT = CalculateValue(nodeAnim.translate, animEditorTime_);
        auto itT = animTempOverrides_.find(animEditorSelectedJointName_);
        if (itT != animTempOverrides_.end() && itT->second.translate) curT = *itT->second.translate;

        if (newT) curT = *newT;
        if (newR) curR = *newR;
        if (newS) curS = *newS;

        Vector3 oppS, oppT;
        Quaternion oppQ;
        if (!ComputeBlenderSymmetrySRT(skeleton, animEditorSelectedJointName_, gizmoOppJoint, curS, curR, curT, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, oppS, oppQ, oppT)) {
            return;
        }

        NodeAnimation& oppNode = editingAnimation_.nodeAnimations[gizmoOppJoint];

        if (newT) {
            bool found = false;
            for (size_t idx = 0; idx < oppNode.translate.size(); ++idx) {
                if (std::abs(oppNode.translate[idx].time - animEditorTime_) < 0.005f) {
                    oppNode.translate[idx].value = oppT;
                    found = true;
                    break;
                }
            }
            if (!found) animTempOverrides_[gizmoOppJoint].translate = oppT;
        }
        if (newR) {
            bool found = false;
            for (size_t idx = 0; idx < oppNode.rotate.size(); ++idx) {
                if (std::abs(oppNode.rotate[idx].time - animEditorTime_) < 0.005f) {
                    oppNode.rotate[idx].value = oppQ;
                    found = true;
                    break;
                }
            }
            if (!found) animTempOverrides_[gizmoOppJoint].rotate = oppQ;
        }
        if (newS) {
            bool found = false;
            for (size_t idx = 0; idx < oppNode.scale.size(); ++idx) {
                if (std::abs(oppNode.scale[idx].time - animEditorTime_) < 0.005f) {
                    oppNode.scale[idx].value = oppS;
                    found = true;
                    break;
                }
            }
            if (!found) animTempOverrides_[gizmoOppJoint].scale = oppS;
        }
    };

    if (!isDraggingAnimGizmo_) {
        animGizmoActiveAxis_ = -1;
    }

    // --------------------------------------------------------
    // Mode 0: Translation (移動)
    // --------------------------------------------------------
    if (animGizmoMode_ == 0) {
        ImVec2 screenTips[3];
        bool tipsValid[3] = { false, false, false };
        int hoveredAxis = -1;
        float minD = 12.0f;

        for (int i = 0; i < 3; ++i) {
            Vector3 tipPos = origin + axes[i] * gizmoRadius;
            float tipW;
            if (project(tipPos, screenTips[i], tipW)) {
                tipsValid[i] = true;
                float d = distToSegment(mousePos, screenOrigin, screenTips[i]);
                if (d < minD) {
                    minD = d;
                    hoveredAxis = i;
                }
            }
        }

        if (!isDraggingAnimGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingAnimGizmo_ = true;
            animGizmoActiveAxis_ = hoveredAxis;
            animGizmoDragStartMouse_ = mousePos;
            animDragPreSnapshot_.animation = editingAnimation_;
            animDragPreSnapshot_.time = animEditorTime_;
            animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
            animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
            animDragPreSnapshot_.description = "ギズモ移動";
            hasAnimDragPreSnapshot_ = true;
        }

        // Draw axis lines and arrow tips (移動用矢印)
        for (int i = 0; i < 3; ++i) {
            if (!tipsValid[i]) continue;
            bool isAct = (animGizmoActiveAxis_ == i) || (!isDraggingAnimGizmo_ && hoveredAxis == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 3.5f : 2.0f;

            ImVec2 dir2D = ImVec2(screenTips[i].x - screenOrigin.x, screenTips[i].y - screenOrigin.y);
            float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
            if (len2D > 1.0f) {
                dir2D.x /= len2D;
                dir2D.y /= len2D;
            } else {
                dir2D = ImVec2(1.0f, 0.0f);
            }
            ImVec2 perp2D = ImVec2(-dir2D.y, dir2D.x);

            float arrowLen = isAct ? 15.0f : 12.0f;
            float arrowWidth = isAct ? 6.5f : 5.0f;

            ImVec2 apex = screenTips[i];
            ImVec2 baseCenter = ImVec2(apex.x - dir2D.x * arrowLen, apex.y - dir2D.y * arrowLen);
            ImVec2 baseL = ImVec2(baseCenter.x + perp2D.x * arrowWidth, baseCenter.y + perp2D.y * arrowWidth);
            ImVec2 baseR = ImVec2(baseCenter.x - perp2D.x * arrowWidth, baseCenter.y - perp2D.y * arrowWidth);

            // 軸ラインの描画 (矢印の付け根まで)
            drawList->AddLine(screenOrigin, baseCenter, col, thick);

            // 矢印ヘッド（三角形）の描画
            drawList->AddTriangleFilled(apex, baseL, baseR, col);
            drawList->AddTriangle(apex, baseL, baseR, IM_COL32(20, 20, 20, 240), 1.2f);
        }

        // Handle Dragging Translation
        if (isDraggingAnimGizmo_ && animGizmoActiveAxis_ >= 0 && animGizmoActiveAxis_ < 3) {
            int a = animGizmoActiveAxis_;
            if (tipsValid[a] && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
                ImVec2 dir2D = ImVec2(screenTips[a].x - screenOrigin.x, screenTips[a].y - screenOrigin.y);
                float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
                if (len2D > 1.0f) {
                    dir2D.x /= len2D;
                    dir2D.y /= len2D;
                    float proj = io.MouseDelta.x * dir2D.x + io.MouseDelta.y * dir2D.y;
                    float factor = gizmoRadius / (std::max)(len2D, 60.0f);
                    float deltaAmount = proj * factor * 1.2f;

                    Vector3 curT = joint.defaultTransform.translate;
                    if (!nodeAnim.translate.empty()) {
                        curT = CalculateValue(nodeAnim.translate, animEditorTime_);
                    }
                    auto itTempT = animTempOverrides_.find(animEditorSelectedJointName_);
                    if (itTempT != animTempOverrides_.end() && itTempT->second.translate) {
                        curT = *itTempT->second.translate;
                    }

                    Vector3 newT = curT;
                    if (a == 0) newT.x += deltaAmount;
                    else if (a == 1) newT.y += deltaAmount;
                    else if (a == 2) newT.z += deltaAmount;

                    bool found = false;
                    for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
                        if (std::abs(nodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                            nodeAnim.translate[idx].value = newT;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        animTempOverrides_[animEditorSelectedJointName_].translate = newT;
                    }
                    syncGizmoOppositeSRT(&newT, nullptr, nullptr);
                    UpdateAnimationPosePreview(sceneManager);
                }
            }
        }
    }
    // --------------------------------------------------------
    // Mode 1: Rotation (回転)
    // --------------------------------------------------------
    else if (animGizmoMode_ == 1) {
        const int numSegments = 32;
        int hoveredRing = -1;
        float minD = 8.0f;

        struct RingData {
            std::vector<ImVec2> pts;
            bool valid = false;
        };
        RingData rings[3];

        for (int i = 0; i < 3; ++i) {
            Vector3 uAxis = axes[(i + 1) % 3];
            Vector3 vAxis = axes[(i + 2) % 3];
            float ringR = gizmoRadius * 0.85f;

            rings[i].pts.reserve(numSegments + 1);
            for (int s = 0; s <= numSegments; ++s) {
                float theta = (static_cast<float>(s) / numSegments) * 6.2831853f;
                Vector3 p = origin + (uAxis * std::cos(theta) + vAxis * std::sin(theta)) * ringR;
                ImVec2 sp;
                float spW;
                if (project(p, sp, spW)) {
                    rings[i].pts.push_back(sp);
                }
            }
            if (rings[i].pts.size() >= numSegments) {
                rings[i].valid = true;
                for (size_t s = 0; s + 1 < rings[i].pts.size(); ++s) {
                    float d = distToSegment(mousePos, rings[i].pts[s], rings[i].pts[s + 1]);
                    if (d < minD) {
                        minD = d;
                        hoveredRing = i;
                    }
                }
            }
        }

        if (!isDraggingAnimGizmo_ && hoveredRing >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingAnimGizmo_ = true;
            animGizmoActiveAxis_ = hoveredRing;
            animGizmoDragStartMouse_ = mousePos;
            animDragPreSnapshot_.animation = editingAnimation_;
            animDragPreSnapshot_.time = animEditorTime_;
            animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
            animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
            animDragPreSnapshot_.description = "ギズモ回転";
            hasAnimDragPreSnapshot_ = true;
        }

        // Draw rings
        for (int i = 0; i < 3; ++i) {
            if (!rings[i].valid) continue;
            bool isAct = (animGizmoActiveAxis_ == i) || (!isDraggingAnimGizmo_ && hoveredRing == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 3.5f : 2.0f;

            for (size_t s = 0; s + 1 < rings[i].pts.size(); ++s) {
                drawList->AddLine(rings[i].pts[s], rings[i].pts[s + 1], col, thick);
            }
        }

        // Handle Dragging Rotation
        if (isDraggingAnimGizmo_ && animGizmoActiveAxis_ >= 0 && animGizmoActiveAxis_ < 3) {
            int a = animGizmoActiveAxis_;
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                float prevAng = std::atan2((mousePos.y - io.MouseDelta.y) - screenOrigin.y, (mousePos.x - io.MouseDelta.x) - screenOrigin.x);
                float curAng = std::atan2(mousePos.y - screenOrigin.y, mousePos.x - screenOrigin.x);
                float deltaAngle = curAng - prevAng;
                while (deltaAngle > 3.14159f) deltaAngle -= 6.28318f;
                while (deltaAngle < -3.14159f) deltaAngle += 6.28318f;

                Vector3 toCam = activeCamera->GetTranslation() - origin;
                float lenCam = std::sqrt(toCam.x * toCam.x + toCam.y * toCam.y + toCam.z * toCam.z);
                if (lenCam > 1e-5f) toCam = toCam / lenCam;
                float facing = axes[a].x * toCam.x + axes[a].y * toCam.y + axes[a].z * toCam.z;
                if (facing < 0.0f) deltaAngle = -deltaAngle;

                Quaternion curQ = joint.defaultTransform.rotate;
                if (!nodeAnim.rotate.empty()) {
                    curQ = CalculateValue(nodeAnim.rotate, animEditorTime_);
                }
                auto itTempR = animTempOverrides_.find(animEditorSelectedJointName_);
                if (itTempR != animTempOverrides_.end() && itTempR->second.rotate) {
                    curQ = *itTempR->second.rotate;
                }

                // ワールド固定軸 axes[a] を親の空間に逆変換してローカル回転軸を得る
                Vector3 worldRotAxis = axes[a];
                Matrix4x4 mParent = TransformFunctions::MakeIdentity4x4();
                if (joint.parent.has_value() && joint.parent.value() >= 0 && joint.parent.value() < static_cast<int32_t>(skeleton.joints.size())) {
                    mParent = skeleton.joints[joint.parent.value()].skeletonSpaceMatrix;
                }
                Matrix4x4 invParent = TransformFunctions::Inverse(mParent);
                Vector3 localRotAxis = invParent * worldRotAxis;
                float lenAx = std::sqrt(localRotAxis.x * localRotAxis.x + localRotAxis.y * localRotAxis.y + localRotAxis.z * localRotAxis.z);
                if (lenAx > 1e-5f) localRotAxis = localRotAxis * (1.0f / lenAx);

                Quaternion qRotDelta = MakeRotHelper(localRotAxis, deltaAngle * 1.5f);
                Quaternion newQ = qRotDelta * curQ;
                float qLen = std::sqrt(newQ.x * newQ.x + newQ.y * newQ.y + newQ.z * newQ.z + newQ.w * newQ.w);
                if (qLen > 1e-5f) {
                    newQ.x /= qLen;
                    newQ.y /= qLen;
                    newQ.z /= qLen;
                    newQ.w /= qLen;
                }

                bool found = false;
                for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
                    if (std::abs(nodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                        nodeAnim.rotate[idx].value = newQ;
                        animEditorSelectedKeyIndex_ = static_cast<int>(idx);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    animTempOverrides_[animEditorSelectedJointName_].rotate = newQ;
                }
                syncGizmoOppositeSRT(nullptr, &newQ, nullptr);
                UpdateAnimationPosePreview(sceneManager);
            }
        }
    }
    // --------------------------------------------------------
    // Mode 2: Scale (拡大縮小)
    // --------------------------------------------------------
    else if (animGizmoMode_ == 2) {
        ImVec2 screenTips[3];
        bool tipsValid[3] = { false, false, false };
        int hoveredAxis = -1;
        float minD = 12.0f;

        // Check center box (uniform scale)
        float distCenter = std::sqrt((mousePos.x - screenOrigin.x) * (mousePos.x - screenOrigin.x) + (mousePos.y - screenOrigin.y) * (mousePos.y - screenOrigin.y));
        if (distCenter <= 8.0f) {
            hoveredAxis = 3;
        } else {
            for (int i = 0; i < 3; ++i) {
                Vector3 tipPos = origin + axes[i] * gizmoRadius;
                float tipW;
                if (project(tipPos, screenTips[i], tipW)) {
                    tipsValid[i] = true;
                    float d = distToSegment(mousePos, screenOrigin, screenTips[i]);
                    if (d < minD) {
                        minD = d;
                        hoveredAxis = i;
                    }
                }
            }
        }

        if (!isDraggingAnimGizmo_ && hoveredAxis >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isDraggingAnimGizmo_ = true;
            animGizmoActiveAxis_ = hoveredAxis;
            animGizmoDragStartMouse_ = mousePos;
            animDragPreSnapshot_.animation = editingAnimation_;
            animDragPreSnapshot_.time = animEditorTime_;
            animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
            animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
            animDragPreSnapshot_.description = "ギズモ拡縮";
            hasAnimDragPreSnapshot_ = true;
        }

        // Draw axis lines and boxes
        for (int i = 0; i < 3; ++i) {
            if (!tipsValid[i]) continue;
            bool isAct = (animGizmoActiveAxis_ == i) || (!isDraggingAnimGizmo_ && hoveredAxis == i);
            ImU32 col = isAct ? axisHoverColors[i] : axisColors[i];
            float thick = isAct ? 3.5f : 2.0f;

            drawList->AddLine(screenOrigin, screenTips[i], col, thick);

            // Tip box
            float bSz = isAct ? 7.0f : 5.0f;
            drawList->AddRectFilled(ImVec2(screenTips[i].x - bSz, screenTips[i].y - bSz), ImVec2(screenTips[i].x + bSz, screenTips[i].y + bSz), col);
            drawList->AddRect(ImVec2(screenTips[i].x - bSz, screenTips[i].y - bSz), ImVec2(screenTips[i].x + bSz, screenTips[i].y + bSz), IM_COL32(20, 20, 20, 240));
        }

        // Draw center box
        bool centerAct = (animGizmoActiveAxis_ == 3) || (!isDraggingAnimGizmo_ && hoveredAxis == 3);
        float cSz = centerAct ? 8.0f : 6.0f;
        ImU32 cCol = centerAct ? IM_COL32(255, 255, 140, 255) : IM_COL32(240, 220, 80, 240);
        drawList->AddRectFilled(ImVec2(screenOrigin.x - cSz, screenOrigin.y - cSz), ImVec2(screenOrigin.x + cSz, screenOrigin.y + cSz), cCol);
        drawList->AddRect(ImVec2(screenOrigin.x - cSz, screenOrigin.y - cSz), ImVec2(screenOrigin.x + cSz, screenOrigin.y + cSz), IM_COL32(20, 20, 20, 255));

        // Handle Dragging Scale
        if (isDraggingAnimGizmo_ && animGizmoActiveAxis_ >= 0) {
            Vector3 curS = joint.defaultTransform.scale;
            if (!nodeAnim.scale.empty()) {
                curS = CalculateValue(nodeAnim.scale, animEditorTime_);
            }
            auto itTempS = animTempOverrides_.find(animEditorSelectedJointName_);
            if (itTempS != animTempOverrides_.end() && itTempS->second.scale) {
                curS = *itTempS->second.scale;
            }

            if (animGizmoActiveAxis_ == 3) {
                // Uniform scale
                float proj = io.MouseDelta.x - io.MouseDelta.y;
                float factor = 1.0f + proj * 0.02f;
                curS = curS * factor;
            } else {
                int a = animGizmoActiveAxis_;
                if (tipsValid[a]) {
                    ImVec2 dir2D = ImVec2(screenTips[a].x - screenOrigin.x, screenTips[a].y - screenOrigin.y);
                    float len2D = std::sqrt(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
                    if (len2D > 1.0f) {
                        dir2D.x /= len2D;
                        dir2D.y /= len2D;
                        float proj = io.MouseDelta.x * dir2D.x + io.MouseDelta.y * dir2D.y;
                        float factor = 1.0f + proj * 0.02f;
                        if (a == 0) curS.x *= factor;
                        else if (a == 1) curS.y *= factor;
                        else if (a == 2) curS.z *= factor;
                    }
                }
            }

            curS.x = (std::max)(0.001f, curS.x);
            curS.y = (std::max)(0.001f, curS.y);
            curS.z = (std::max)(0.001f, curS.z);

            bool found = false;
            for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
                if (std::abs(nodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                    nodeAnim.scale[idx].value = curS;
                    found = true;
                    break;
                }
            }
            if (!found) {
                animTempOverrides_[animEditorSelectedJointName_].scale = curS;
            }
            syncGizmoOppositeSRT(nullptr, nullptr, &curS);
            UpdateAnimationPosePreview(sceneManager);
        }
    }

    if (isAnimLocked_) {
        drawList->AddText(ImVec2(screenOrigin.x + 8, screenOrigin.y - 18), IM_COL32(255, 120, 120, 255), "[LOCKED (L)]");
    }

    if (!io.MouseDown[0]) {
        if (isDraggingAnimGizmo_ && hasAnimDragPreSnapshot_) {
            animUndoStack_.push_back(animDragPreSnapshot_);
            if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
            animRedoStack_.clear();
            hasAnimDragPreSnapshot_ = false;
        }
        isDraggingAnimGizmo_ = false;
        animGizmoActiveAxis_ = -1;
    }

    drawList->PopClipRect();
}

void EditorManager::DrawAnimationEditorMainView(SceneManager* sceneManager, Camera** activeCamera, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle) {
    // 3Dレンダリング結果のプレビュー表示
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

    // カメラ軸スナップの線形補間アニメーション更新
    Camera* cam = activeCamera ? *activeCamera : nullptr;
    if (cam) {
        if (isCameraSnapLerping_) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[1] || io.MouseDown[2] || io.MouseWheel != 0.0f) {
                // ユーザーによる手動操作があれば補間中断
                isCameraSnapLerping_ = false;
            } else {
                float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;
                cameraSnapLerpTimer_ += dt;
                float t = std::clamp(cameraSnapLerpTimer_ / cameraSnapLerpDuration_, 0.0f, 1.0f);
                float easeT = t * t * (3.0f - 2.0f * t); // スムーズステップ

                Vector3 curRot = {
                    cameraSnapStartRot_.x + (cameraSnapEndRot_.x - cameraSnapStartRot_.x) * easeT,
                    cameraSnapStartRot_.y + (cameraSnapEndRot_.y - cameraSnapStartRot_.y) * easeT,
                    cameraSnapStartRot_.z + (cameraSnapEndRot_.z - cameraSnapStartRot_.z) * easeT
                };
                Vector3 curPos = {
                    cameraSnapStartPos_.x + (cameraSnapEndPos_.x - cameraSnapStartPos_.x) * easeT,
                    cameraSnapStartPos_.y + (cameraSnapEndPos_.y - cameraSnapStartPos_.y) * easeT,
                    cameraSnapStartPos_.z + (cameraSnapEndPos_.z - cameraSnapStartPos_.z) * easeT
                };

                cam->SetRotation(curRot);
                cam->SetTranslation(curPos);
                cam->UpdateMatrix();

                if (t >= 1.0f) {
                    isCameraSnapLerping_ = false;
                }
            }
        }

        // ボーン（スケルトン）位置の可視化＆選択オーバーレイ描画
        DrawSkeletonJointsOverlay(sceneManager, cam, gameViewPos_, gameViewSize_);

        // ボーン SRT ギズモ（マニピュレーター）の描画 & 操作
        DrawBoneTransformGizmo(sceneManager, cam, gameViewPos_, gameViewSize_);

        // カメラ向きギズモ（スナップ対応）の描画
        DrawCameraOrientationGizmo(cam, gameViewPos_, gameViewSize_);
    }

    // ビューポート左上にHUD / ギズモツールバー表示
    ImGui::SetCursorPos(ImVec2(currentPos.x + 10.0f, currentPos.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.70f));
    
    if (isAnimHudMinimized_) {
        // 縮小化表示 (「拡大化」ボタンのみを表示)
        ImGui::BeginChild("##AnimViewportHUD", ImVec2(96, 40), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        if (ImGui::Button("拡大化", ImVec2(80, 24))) {
            isAnimHudMinimized_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("HUDを展開 (Hキーで切替)");
        ImGui::PopStyleVar();
        ImGui::EndChild();
    } else {
        // 通常（展開）表示
        ImGui::BeginChild("##AnimViewportHUD", ImVec2(480, 115), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        // 左上に縮小化ボタンを配置（拡大化ボタンと同じ位置）
        if (ImGui::Button("縮小化", ImVec2(80, 24))) {
            isAnimHudMinimized_ = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("HUDを縮小化 (Hキーで切替)");

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[3D Viewport] アニメーションビューポート");

        int curF = static_cast<int>(std::round(animEditorTime_ * animEditorFps_));
        int totF = static_cast<int>(std::round(editingAnimation_.duration * animEditorFps_));
        ImGui::Text("フレーム: %d / %d  (%.3fs)", curF, totF, animEditorTime_);
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "選択ボーン: %s%s", animEditorSelectedJointName_.c_str(), isAnimLocked_ ? " [固定中]" : "");

        // ギズモツールバー (SRT / Local-World / Lock)
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        {
            bool isTrans = (animGizmoMode_ == 0);
            if (isTrans) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("[T] 移動", ImVec2(62, 20))) animGizmoMode_ = 0;
            if (isTrans) ImGui::PopStyleColor();

            ImGui::SameLine();
            bool isRot = (animGizmoMode_ == 1);
            if (isRot) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.75f, 0.3f, 1.0f));
            if (ImGui::Button("[R] 回転", ImVec2(62, 20))) animGizmoMode_ = 1;
            if (isRot) ImGui::PopStyleColor();

            ImGui::SameLine();
            bool isScale = (animGizmoMode_ == 2);
            if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
            if (ImGui::Button("[S] 拡縮", ImVec2(62, 20))) animGizmoMode_ = 2;
            if (isScale) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::Button(animGizmoSpace_ == 0 ? "Local" : "World", ImVec2(52, 20))) {
                animGizmoSpace_ = (animGizmoSpace_ == 0) ? 1 : 0;
            }

            ImGui::SameLine();
            if (isAnimLocked_) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
                if (ImGui::Button("[L] ロック中", ImVec2(88, 20))) isAnimLocked_ = false;
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button("[L] ロック", ImVec2(75, 20))) isAnimLocked_ = true;
            }
        }
        ImGui::PopStyleVar(); // ItemSpacing
        ImGui::PopStyleVar(); // FrameRounding

        ImGui::EndChild();
    }
    ImGui::PopStyleColor();

    // ショートカットキー判定 (Ctrl+Z: 元に戻す, Ctrl+Y: やり直す, T/R/S でギズモ切替, L でロック切替, H でHUD縮小/展開切替)
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                if (io.KeyShift) {
                    PerformAnimRedo(sceneManager);
                } else {
                    PerformAnimUndo(sceneManager);
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                PerformAnimRedo(sceneManager);
            }
        } else {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false)) animGizmoMode_ = 0; // Translate (移動)
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) animGizmoMode_ = 1; // Rotate (回転)
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) animGizmoMode_ = 2; // Scale (拡縮)
            if (ImGui::IsKeyPressed(ImGuiKey_L, false)) isAnimLocked_ = !isAnimLocked_; // Lock (ロック)
            if (ImGui::IsKeyPressed(ImGuiKey_H, false)) isAnimHudMinimized_ = !isAnimHudMinimized_; // HUD Minimize toggle
        }
    }
}

void EditorManager::DrawAnimationDopeSheetUI(SceneManager* sceneManager) {
    if (!animEditorInitialized_) {
        ScanAnimationFiles();
        if (!LoadAnimationFromJsonFile(editingAnimation_, currentAnimFilePath_)) {
            if (currentAnimFilePath_.find("wall_climb") != std::string::npos) {
                editingAnimation_ = CreateDefaultWallClimbAnimation();
            } else if (currentAnimFilePath_.find("air_dash") != std::string::npos) {
                editingAnimation_ = CreateDefaultAirDashAnimation();
            }
        }
        animEditorInitialized_ = true;
    }

    if (availableAnimationFiles_.empty()) {
        ScanAnimationFiles();
    }

    if (ImGui::Begin("ドープシート (タイムライン)", &showAnimEditor_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowAppearing()) {
            currentMode_ = EditorMode::Animation;
            if (!isAnimationScenePushed_) {
                sceneManager->PushScene(std::make_unique<AnimationPreviewScene>());
                isAnimationScenePushed_ = true;
                RefreshAnimationJointList(sceneManager);
            }
        }

        // ========================================================
        // 1. ヘッダーバー (Playback Controls & Actions)
        // ========================================================
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        // アクション / アニメーション切替 (ファイル一覧から選択 & 自動ロード)
        std::string currentStem = std::filesystem::path(currentAnimFilePath_).stem().string();
        std::string currentDisplay = currentStem;
        if (currentStem == "wall_climb_animation") currentDisplay = "壁つかまり (wall_climb)";
        else if (currentStem == "air_dash_animation") currentDisplay = "空中ダッシュ (air_dash)";

        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("##AnimSelectCombo", currentDisplay.c_str())) {
            for (const auto& filePath : availableAnimationFiles_) {
                std::string stem = std::filesystem::path(filePath).stem().string();
                std::string displayName = stem;
                if (stem == "wall_climb_animation") displayName = "壁つかまり (wall_climb)";
                else if (stem == "air_dash_animation") displayName = "空中ダッシュ (air_dash)";

                bool isSel = (currentAnimFilePath_ == filePath);
                if (ImGui::Selectable(displayName.c_str(), isSel)) {
                    if (currentAnimFilePath_ != filePath) {
                        currentAnimFilePath_ = filePath;
                        if (!LoadAnimationFromJsonFile(editingAnimation_, currentAnimFilePath_)) {
                            if (currentAnimFilePath_.find("wall_climb") != std::string::npos) {
                                editingAnimation_ = CreateDefaultWallClimbAnimation();
                            } else if (currentAnimFilePath_.find("air_dash") != std::string::npos) {
                                editingAnimation_ = CreateDefaultAirDashAnimation();
                            }
                        }
                        animEditorTime_ = 0.0f;
                        animEditorSelectedKeyIndex_ = -1;
                        ClearAnimUndoRedo();
                        UpdateAnimationPosePreview(sceneManager);
                        LogManager::GetInstance()->AddLog(LogLevel::Info, "アニメーション読込: " + currentAnimFilePath_);
                    }
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("編集対象のアニメーションを選択（切り替え時に自動読込）");

        ImGui::SameLine();
        // 上書き保存ボタン
        if (ImGui::Button("[Save] 上書き保存")) {
            SaveAnimationToJsonFile(editingAnimation_, currentAnimFilePath_);
            LogManager::GetInstance()->AddLog(LogLevel::Info, "アニメーション上書き保存: " + currentAnimFilePath_);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("現在のアニメーションファイル (%s) に上書き保存", currentAnimFilePath_.c_str());

        ImGui::SameLine();
        // 名前をつけて保存ボタン
        if (ImGui::Button("[+] 名前をつけて保存")) {
            snprintf(newAnimSaveNameBuf_, sizeof(newAnimSaveNameBuf_), "%s_copy", currentStem.c_str());
            openSaveAnimModal_ = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("新しい名前をつけてJSONファイルとして新規保存");

        // 名前をつけて保存モーダルダイアログ
        if (openSaveAnimModal_) {
            ImGui::OpenPopup("名前をつけて保存##AnimSaveModal");
            openSaveAnimModal_ = false;
        }

        if (ImGui::BeginPopupModal("名前をつけて保存##AnimSaveModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("新しいアニメーションのファイル名を入力してください:");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("##NewAnimFileName", newAnimSaveNameBuf_, sizeof(newAnimSaveNameBuf_));
            ImGui::TextDisabled("保存先: resources/json/shared/Player/%s.json", newAnimSaveNameBuf_);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("保存", ImVec2(120, 0))) {
                std::string inputName = newAnimSaveNameBuf_;
                if (!inputName.empty()) {
                    if (inputName.size() < 5 || inputName.substr(inputName.size() - 5) != ".json") {
                        inputName += ".json";
                    }
                    std::string fullPath = "resources/json/shared/Player/" + inputName;
                    SaveAnimationToJsonFile(editingAnimation_, fullPath);
                    ScanAnimationFiles();
                    currentAnimFilePath_ = fullPath;
                    LogManager::GetInstance()->AddLog(LogLevel::Info, "新規アニメーション保存: " + fullPath);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        // 削除ボタン
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("[-] 削除")) {
            openDeleteAnimModal_ = true;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("現在選択されているアニメーションファイル (%s) を削除", currentAnimFilePath_.c_str());

        // アニメーション削除確認モーダルダイアログ
        if (openDeleteAnimModal_) {
            ImGui::OpenPopup("アニメーションの削除確認##AnimDeleteModal");
            openDeleteAnimModal_ = false;
        }

        if (ImGui::BeginPopupModal("アニメーションの削除確認##AnimDeleteModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("本当にこのアニメーションを削除しますか？");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "ファイル: %s", currentAnimFilePath_.c_str());
            ImGui::TextDisabled("※ 削除したファイルは元に戻せません。");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
            if (ImGui::Button("削除", ImVec2(120, 0))) {
                std::string deletedFilePath = currentAnimFilePath_;
                if (std::filesystem::exists(deletedFilePath)) {
                    std::error_code ec;
                    std::filesystem::remove(deletedFilePath, ec);
                }
                LogManager::GetInstance()->AddLog(LogLevel::Info, "アニメーション削除: " + deletedFilePath);

                ScanAnimationFiles();

                // 新しく利用可能なファイルから読み込み
                if (!availableAnimationFiles_.empty()) {
                    currentAnimFilePath_ = availableAnimationFiles_[0];
                    if (!LoadAnimationFromJsonFile(editingAnimation_, currentAnimFilePath_)) {
                        if (currentAnimFilePath_.find("wall_climb") != std::string::npos) {
                            editingAnimation_ = CreateDefaultWallClimbAnimation();
                        } else if (currentAnimFilePath_.find("air_dash") != std::string::npos) {
                            editingAnimation_ = CreateDefaultAirDashAnimation();
                        }
                    }
                } else {
                    editingAnimation_ = Animation{};
                }

                animEditorTime_ = 0.0f;
                animEditorSelectedKeyIndex_ = -1;
                ClearAnimUndoRedo();
                UpdateAnimationPosePreview(sceneManager);

                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 再生コントロールボタン群
        // 先頭へ (|<<)
        if (ImGui::Button("|<<", ImVec2(32, 0))) {
            animEditorTime_ = 0.0f;
            animEditorPlaying_ = false;
            animTempOverrides_.clear();
            UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("先頭フレームへ");

        ImGui::SameLine();
        // 1フレーム戻る (<)
        if (ImGui::Button("<", ImVec2(28, 0))) {
            animEditorTime_ = (std::max)(0.0f, animEditorTime_ - 1.0f / animEditorFps_);
            animTempOverrides_.clear();
            UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム戻る");

        // 再生 / 一時停止 ([>] 再生 / [||] 停止)
        ImGui::SameLine();
        if (animEditorPlaying_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("[||] 停止", ImVec2(68, 0))) {
                animEditorPlaying_ = false;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("[>] 再生", ImVec2(68, 0))) {
                animEditorPlaying_ = true;
                animTempOverrides_.clear();
            }
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生 / 一時停止 (Space)");

        ImGui::SameLine();
        // 1フレーム進む (>)
        if (ImGui::Button(">", ImVec2(28, 0))) {
            animEditorTime_ = (std::min)(editingAnimation_.duration, animEditorTime_ + 1.0f / animEditorFps_);
            animTempOverrides_.clear();
            UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1フレーム進む");

        ImGui::SameLine();
        // 末尾へ (>>|)
        if (ImGui::Button(">>|", ImVec2(32, 0))) {
            animEditorTime_ = editingAnimation_.duration;
            animEditorPlaying_ = false;
            animTempOverrides_.clear();
            UpdateAnimationPosePreview(sceneManager);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("末尾フレームへ");

        ImGui::SameLine();
        // ループ再生トグル
        if (animEditorLoop_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("Loop: ON")) {
                animEditorLoop_ = false;
            }
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Loop: OFF")) {
                animEditorLoop_ = true;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("ループ再生の切り替え");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // フレーム・時間情報
        int curFrame = static_cast<int>(std::round(animEditorTime_ * animEditorFps_));
        int totalFrames = static_cast<int>(std::round(editingAnimation_.duration * animEditorFps_));
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%d / %d F (%.3fs / %.3fs)", curFrame, totalFrames, animEditorTime_, editingAnimation_.duration);

        ImGui::PopStyleVar(2);

        ImGui::Separator();

        // ショートカットキー判定
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            auto io = ImGui::GetIO();
            if (io.KeyCtrl && !io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io.KeyShift) PerformAnimRedo(sceneManager);
                    else PerformAnimUndo(sceneManager);
                } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    PerformAnimRedo(sceneManager);
                }
            } else if (!io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                    animEditorPlaying_ = !animEditorPlaying_;
                    if (animEditorPlaying_) animTempOverrides_.clear();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_T, false)) animGizmoMode_ = 0; // Translate (移動)
                if (ImGui::IsKeyPressed(ImGuiKey_R, false)) animGizmoMode_ = 1; // Rotate (回転)
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) animGizmoMode_ = 2; // Scale (拡縮)
                if (ImGui::IsKeyPressed(ImGuiKey_L, false)) isAnimLocked_ = !isAnimLocked_; // Lock (ロック)
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) isAnimHudMinimized_ = !isAnimHudMinimized_; // HUD Minimize toggle

                if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
                    PushAnimUndoState("キー挿入 (I)");
                    NodeAnimation& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];
                    AnimatorComponent* anim = GetTargetAnimator(sceneManager);
                    const Skeleton* skel = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

                    Quaternion curQ = { 0.0f, 0.0f, 0.0f, 1.0f };
                    Vector3 curT = { 0.0f, 0.0f, 0.0f };
                    Vector3 curS = { 1.0f, 1.0f, 1.0f };

                    if (skel) {
                        auto itJ = skel->jointMap.find(animEditorSelectedJointName_);
                        if (itJ != skel->jointMap.end()) {
                            curQ = skel->joints[itJ->second].transform.rotate;
                            curT = skel->joints[itJ->second].transform.translate;
                            curS = skel->joints[itJ->second].transform.scale;
                        }
                    }
                    if (!nodeAnim.rotate.empty()) curQ = CalculateValue(nodeAnim.rotate, animEditorTime_);
                    if (!nodeAnim.translate.empty()) curT = CalculateValue(nodeAnim.translate, animEditorTime_);
                    if (!nodeAnim.scale.empty()) curS = CalculateValue(nodeAnim.scale, animEditorTime_);

                    auto itTemp = animTempOverrides_.find(animEditorSelectedJointName_);
                    if (itTemp != animTempOverrides_.end()) {
                        if (itTemp->second.translate) curT = *itTemp->second.translate;
                        if (itTemp->second.rotate) curQ = *itTemp->second.rotate;
                        if (itTemp->second.scale) curS = *itTemp->second.scale;
                    }

                    // Rotation
                    bool foundR = false;
                    for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
                        if (std::abs(nodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                            nodeAnim.rotate[idx].value = curQ;
                            animEditorSelectedKeyIndex_ = static_cast<int>(idx);
                            foundR = true;
                            break;
                        }
                    }
                    if (!foundR) {
                        KeyframeQuaternion newKf{ animEditorTime_, curQ };
                        auto itK = nodeAnim.rotate.begin();
                        while (itK != nodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
                        auto ins = nodeAnim.rotate.insert(itK, newKf);
                        animEditorSelectedKeyIndex_ = static_cast<int>(std::distance(nodeAnim.rotate.begin(), ins));
                    }

                    // Translation
                    bool foundT = false;
                    for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
                        if (std::abs(nodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                            nodeAnim.translate[idx].value = curT;
                            foundT = true;
                            break;
                        }
                    }
                    if (!foundT) {
                        KeyframeVector3 newKf{ animEditorTime_, curT };
                        auto itK = nodeAnim.translate.begin();
                        while (itK != nodeAnim.translate.end() && itK->time < newKf.time) ++itK;
                        nodeAnim.translate.insert(itK, newKf);
                    }

                    // Scale
                    bool foundS = false;
                    for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
                        if (std::abs(nodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                            nodeAnim.scale[idx].value = curS;
                            foundS = true;
                            break;
                        }
                    }
                    if (!foundS) {
                        KeyframeVector3 newKf{ animEditorTime_, curS };
                        auto itK = nodeAnim.scale.begin();
                        while (itK != nodeAnim.scale.end() && itK->time < newKf.time) ++itK;
                        nodeAnim.scale.insert(itK, newKf);
                    }

                    // 対称ボーンへのキー挿入連携
                    std::string oppJointName = FindOppositeJointName(animEditorSelectedJointName_, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, skel);
                    bool hasAnySymmetryAxis = animSymmetryAxisX_ || animSymmetryAxisY_ || animSymmetryAxisZ_;
                    if (animSymmetryMode_ && hasAnySymmetryAxis && !oppJointName.empty() && oppJointName != animEditorSelectedJointName_ && skel) {
                        Vector3 oppS, oppT;
                        Quaternion oppQ;
                        if (ComputeBlenderSymmetrySRT(*skel, animEditorSelectedJointName_, oppJointName, curS, curQ, curT, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, oppS, oppQ, oppT)) {
                            NodeAnimation& oppNodeAnim = editingAnimation_.nodeAnimations[oppJointName];
                            // Translate
                            bool foundOppT = false;
                            for (size_t idx = 0; idx < oppNodeAnim.translate.size(); ++idx) {
                                if (std::abs(oppNodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                                    oppNodeAnim.translate[idx].value = oppT;
                                    foundOppT = true;
                                    break;
                                }
                            }
                            if (!foundOppT) {
                                KeyframeVector3 newKf{ animEditorTime_, oppT };
                                auto itK = oppNodeAnim.translate.begin();
                                while (itK != oppNodeAnim.translate.end() && itK->time < newKf.time) ++itK;
                                oppNodeAnim.translate.insert(itK, newKf);
                            }
                            // Rotate
                            bool foundOppR = false;
                            for (size_t idx = 0; idx < oppNodeAnim.rotate.size(); ++idx) {
                                if (std::abs(oppNodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                                    oppNodeAnim.rotate[idx].value = oppQ;
                                    foundOppR = true;
                                    break;
                                }
                            }
                            if (!foundOppR) {
                                KeyframeQuaternion newKf{ animEditorTime_, oppQ };
                                auto itK = oppNodeAnim.rotate.begin();
                                while (itK != oppNodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
                                oppNodeAnim.rotate.insert(itK, newKf);
                            }
                            // Scale
                            bool foundOppS = false;
                            for (size_t idx = 0; idx < oppNodeAnim.scale.size(); ++idx) {
                                if (std::abs(oppNodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                                    oppNodeAnim.scale[idx].value = oppS;
                                    foundOppS = true;
                                    break;
                                }
                            }
                            if (!foundOppS) {
                                KeyframeVector3 newKf{ animEditorTime_, oppS };
                                auto itK = oppNodeAnim.scale.begin();
                                while (itK != oppNodeAnim.scale.end() && itK->time < newKf.time) ++itK;
                                oppNodeAnim.scale.insert(itK, newKf);
                            }
                            animTempOverrides_.erase(oppJointName);
                        }
                    }

                    animTempOverrides_.erase(animEditorSelectedJointName_);
                    UpdateAnimationPosePreview(sceneManager);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                    if (io.KeyShift || animEditorSelectedKeyIndex_ < 0) {
                        // Shift+Del または サマリー/全体選択時は全ボーンのキーを一括削除
                        PushAnimUndoState("全ボーンキー削除 (Del)");
                        float delTime = animEditorTime_;
                        for (auto& [nName, nAnim] : editingAnimation_.nodeAnimations) {
                            nAnim.rotate.erase(
                                std::remove_if(nAnim.rotate.begin(), nAnim.rotate.end(),
                                    [delTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                                nAnim.rotate.end()
                            );
                            nAnim.translate.erase(
                                std::remove_if(nAnim.translate.begin(), nAnim.translate.end(),
                                    [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                                nAnim.translate.end()
                            );
                            nAnim.scale.erase(
                                std::remove_if(nAnim.scale.begin(), nAnim.scale.end(),
                                    [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                                nAnim.scale.end()
                            );
                        }
                        animEditorSelectedKeyIndex_ = -1;
                        UpdateAnimationPosePreview(sceneManager);
                    } else {
                        // 選択中ボーンの個別キー削除
                        PushAnimUndoState("キー削除 (Del)");
                        NodeAnimation& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];
                        float delTime = animEditorTime_;
                        if (animEditorSelectedKeyIndex_ >= 0 && animEditorSelectedKeyIndex_ < static_cast<int>(nodeAnim.rotate.size())) {
                            delTime = nodeAnim.rotate[animEditorSelectedKeyIndex_].time;
                        }

                        nodeAnim.rotate.erase(
                            std::remove_if(nodeAnim.rotate.begin(), nodeAnim.rotate.end(),
                                [delTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                            nodeAnim.rotate.end()
                        );

                        nodeAnim.translate.erase(
                            std::remove_if(nodeAnim.translate.begin(), nodeAnim.translate.end(),
                                [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                            nodeAnim.translate.end()
                        );

                        nodeAnim.scale.erase(
                            std::remove_if(nodeAnim.scale.begin(), nodeAnim.scale.end(),
                                [delTime](const KeyframeVector3& kf) { return std::abs(kf.time - delTime) < 0.005f; }),
                            nodeAnim.scale.end()
                        );

                        animEditorSelectedKeyIndex_ = -1;
                        UpdateAnimationPosePreview(sceneManager);
                    }
                }
            }
        }

        // ========================================================
        // 2. ドープシート タイムライン本体 (Canvas & Tracks)
        // ========================================================
        if (currentJointList_.empty() || animJointTreeNodes_.empty()) {
            RefreshAnimationJointList(sceneManager);
        }

        // 可視トラックの収集 (開いている親の子孫のみ再帰的に追加)
        struct VisibleAnimTrack {
            std::string name;
            int32_t jointIndex = -1;
            int depth = 0;
            bool hasChildren = false;
            bool isOpen = false;
        };
        std::vector<VisibleAnimTrack> visibleTracks;

        std::function<void(int32_t, int)> collectVisible = [&](int32_t nodeIdx, int depth) {
            if (nodeIdx < 0 || nodeIdx >= static_cast<int32_t>(animJointTreeNodes_.size())) return;
            const auto& node = animJointTreeNodes_[nodeIdx];
            bool hasChildren = !node.children.empty();
            bool isOpen = false;
            if (hasChildren) {
                auto it = animJointExpanded_.find(node.name);
                isOpen = (it != animJointExpanded_.end() && it->second);
            }

            visibleTracks.push_back({ node.name, node.jointIndex, depth, hasChildren, isOpen });

            if (hasChildren && isOpen) {
                for (int32_t childIdx : node.children) {
                    collectVisible(childIdx, depth + 1);
                }
            }
        };

        for (int32_t rootIdx : animJointRootIndices_) {
            collectVisible(rootIdx, 0);
        }

        if (visibleTracks.empty()) {
            for (const auto& name : currentJointList_) {
                visibleTracks.push_back({ name, -1, 0, false, false });
            }
        }

        const float trackListWidth = 220.0f;
        const float rulerHeight = 26.0f;
        const float trackHeight = 22.0f;
        const float summaryHeight = 24.0f;
        int numVisibleTracks = static_cast<int>(visibleTracks.size());
        float totalHeight = rulerHeight + summaryHeight + numVisibleTracks * trackHeight + 50.0f;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
        float canvasWidth = (std::max)(canvasAvail.x, 300.0f);

        // ホイールによるズームと横スクロール
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            if (io.KeyCtrl && io.MouseWheel != 0.0f) {
                float zoomFactor = (io.MouseWheel > 0.0f) ? 1.15f : 0.87f;
                animTimelineZoom_ = std::clamp(animTimelineZoom_ * zoomFactor, 40.0f, 800.0f);
            } else if (io.MouseWheel != 0.0f && !io.KeyCtrl) {
                animTimelineScrollX_ = (std::max)(0.0f, animTimelineScrollX_ - io.MouseWheel * 40.0f);
            }
        }

        ImGui::BeginChild("##DopeSheetScrollArea", ImVec2(canvasWidth, canvasAvail.y), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + canvasWidth, p0.y + totalHeight);
        float contentBottomY = p0.y + totalHeight;

        // タイムライン描画領域
        float timelineStartX = p0.x + trackListWidth;
        float timelineEndX = p0.x + canvasWidth;
        float summaryY = p0.y + rulerHeight;

        // ----------------------------------------------------
        // 1. 背景描画
        // ----------------------------------------------------
        // 全体背景
        drawList->AddRectFilled(p0, p1, IM_COL32(26, 26, 30, 255));

        // 各トラック行の背景（ストライプ＆選択ハイライト）
        float curTrackY = summaryY + summaryHeight;
        for (int i = 0; i < numVisibleTracks; ++i) {
            const auto& item = visibleTracks[i];
            bool isSelected = (animEditorSelectedJointName_ == item.name);
            ImU32 rowBg = isSelected ? IM_COL32(38, 62, 92, 255) : (i % 2 == 0 ? IM_COL32(32, 32, 36, 255) : IM_COL32(26, 26, 30, 255));
            drawList->AddRectFilled(ImVec2(timelineStartX, curTrackY), ImVec2(p1.x, curTrackY + trackHeight), rowBg);
            drawList->AddLine(ImVec2(timelineStartX, curTrackY + trackHeight), ImVec2(p1.x, curTrackY + trackHeight), IM_COL32(45, 45, 52, 255), 1.0f);
            curTrackY += trackHeight;
        }

        // サマリー行のタイムライン背景
        drawList->AddRectFilled(ImVec2(timelineStartX, summaryY), ImVec2(p1.x, summaryY + summaryHeight), IM_COL32(46, 42, 36, 255));
        drawList->AddLine(ImVec2(timelineStartX, summaryY + summaryHeight), ImVec2(p1.x, summaryY + summaryHeight), IM_COL32(70, 64, 55, 255), 1.0f);

        // ----------------------------------------------------
        // 2. タイムライン縦グリッド線（背景の上に描画して完全に貫通させる）
        // ----------------------------------------------------
        float maxDuration = (std::max)(editingAnimation_.duration, 1.0f) + 1.0f;
        int maxFrames = static_cast<int>(std::ceil(maxDuration * animEditorFps_));
        int fpsInt = static_cast<int>(std::round(animEditorFps_));
        if (fpsInt <= 0) fpsInt = 60;
        int stepF = (animTimelineZoom_ > 250.0f) ? 5 : (animTimelineZoom_ > 100.0f ? 10 : 30);

        // (a) マイナーグリッド線（1フレームごと、ズーム時）
        if (animTimelineZoom_ > 140.0f) {
            for (int f = 0; f <= maxFrames; ++f) {
                if (f % 5 == 0) continue;
                float t = f / animEditorFps_;
                float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
                if (x >= timelineStartX && x <= timelineEndX) {
                    drawList->AddLine(ImVec2(x, p0.y + rulerHeight), ImVec2(x, contentBottomY), IM_COL32(40, 42, 48, 220), 1.0f);
                }
            }
        }

        // (b) 中グリッド線（5F / 10F / 30Fごと）
        for (int f = 0; f <= maxFrames; f += stepF) {
            if (f % fpsInt == 0) continue;
            float t = f / animEditorFps_;
            float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
            if (x >= timelineStartX && x <= timelineEndX) {
                drawList->AddLine(ImVec2(x, p0.y + rulerHeight), ImVec2(x, contentBottomY), IM_COL32(58, 62, 72, 230), 1.0f);
            }
        }

        // (c) メジャーグリッド線（1秒ごと / FPSの倍数）
        for (int f = 0; f <= maxFrames; f += fpsInt) {
            float t = f / animEditorFps_;
            float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
            if (x >= timelineStartX && x <= timelineEndX) {
                drawList->AddLine(ImVec2(x, p0.y + rulerHeight), ImVec2(x, contentBottomY), IM_COL32(90, 95, 110, 255), 1.5f);
            }
        }

        // (d) アニメーション終了（Duration）境界線
        float endX = timelineStartX + editingAnimation_.duration * animTimelineZoom_ - animTimelineScrollX_;
        if (endX >= timelineStartX && endX <= timelineEndX) {
            drawList->AddLine(ImVec2(endX, p0.y + rulerHeight), ImVec2(endX, contentBottomY), IM_COL32(235, 150, 40, 255), 2.0f);
        }

        // ----------------------------------------------------
        // 3. ルーラー（上部目盛りバー）
        // ----------------------------------------------------
        drawList->AddRectFilled(ImVec2(timelineStartX, p0.y), ImVec2(p1.x, p0.y + rulerHeight), IM_COL32(42, 44, 50, 255));
        drawList->AddLine(ImVec2(timelineStartX, p0.y + rulerHeight), ImVec2(p1.x, p0.y + rulerHeight), IM_COL32(70, 74, 84, 255), 1.0f);

        for (int f = 0; f <= maxFrames; f += stepF) {
            float t = f / animEditorFps_;
            float x = timelineStartX + t * animTimelineZoom_ - animTimelineScrollX_;
            if (x < timelineStartX || x > timelineEndX) continue;

            bool isSec = (f % fpsInt == 0);
            float tickH = isSec ? 12.0f : 6.0f;
            ImU32 tickCol = isSec ? IM_COL32(220, 225, 235, 255) : IM_COL32(160, 165, 175, 255);
            drawList->AddLine(ImVec2(x, p0.y + rulerHeight - tickH), ImVec2(x, p0.y + rulerHeight), tickCol, isSec ? 1.5f : 1.0f);

            char fBuf[32];
            snprintf(fBuf, sizeof(fBuf), "%d", f);
            drawList->AddText(ImVec2(x + 3, p0.y + 4), isSec ? IM_COL32(230, 235, 245, 255) : IM_COL32(170, 175, 185, 255), fBuf);
        }

        // ルーラーおよびタイムライン全領域でのスクラブ（時間シーク）操作
        ImVec2 mousePos = io.MousePos;
        bool isHoverTimeline = (mousePos.x >= timelineStartX && mousePos.x <= timelineEndX && mousePos.y >= p0.y && mousePos.y <= contentBottomY);

        if (isHoverTimeline && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl) {
            isAnimRulerScrubbing_ = true;
            animTempOverrides_.clear();
        }
        if (isAnimRulerScrubbing_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float newTime = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                animEditorTime_ = std::clamp(newTime, 0.0f, editingAnimation_.duration);
                UpdateAnimationPosePreview(sceneManager);
            } else {
                isAnimRulerScrubbing_ = false;
            }
        }

        // ----------------------------------------------------
        // 4. サマリーキー（概要）の描画 & 操作
        // ----------------------------------------------------
        std::set<float> summaryKeyTimes;
        for (const auto& [nName, nAnim] : editingAnimation_.nodeAnimations) {
            for (const auto& k : nAnim.rotate) summaryKeyTimes.insert(k.time);
            for (const auto& k : nAnim.translate) summaryKeyTimes.insert(k.time);
            for (const auto& k : nAnim.scale) summaryKeyTimes.insert(k.time);
        }

        float deleteSummaryTime = -1.0f;
        for (float sTime : summaryKeyTimes) {
            float sX = timelineStartX + sTime * animTimelineZoom_ - animTimelineScrollX_;
            if (sX >= timelineStartX && sX <= timelineEndX) {
                float sCenterY = summaryY + summaryHeight * 0.5f;
                ImVec2 dP[4] = {
                    ImVec2(sX, sCenterY - 5.0f),
                    ImVec2(sX + 5.0f, sCenterY),
                    ImVec2(sX, sCenterY + 5.0f),
                    ImVec2(sX - 5.0f, sCenterY)
                };
                bool isNearCurTime = std::abs(sTime - animEditorTime_) < 0.01f;
                ImU32 dCol = isNearCurTime ? IM_COL32(255, 220, 60, 255) : IM_COL32(230, 160, 40, 255);
                drawList->AddConvexPolyFilled(dP, 4, dCol);
                drawList->AddPolyline(dP, 4, IM_COL32(20, 20, 20, 255), ImDrawFlags_Closed, 1.0f);

                // 左クリックでサマリーキー選択 & 時間シーク
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && std::abs(mousePos.x - sX) <= 6.0f && std::abs(mousePos.y - sCenterY) <= 6.0f) {
                    animEditorTime_ = sTime;
                    animEditorSelectedKeyIndex_ = -1;
                    animTempOverrides_.clear();
                    UpdateAnimationPosePreview(sceneManager);
                }

                // Ctrlキーを押しながらドラッグした場合のみキー移動を許可（通常ドラッグでの誤移動を完全防止）
                if (io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f) && std::abs(io.MouseClickedPos[0].x - sX) <= 6.0f && std::abs(io.MouseClickedPos[0].y - sCenterY) <= 6.0f) {
                    if (!isSummaryKeyDrag_) {
                        isSummaryKeyDrag_ = true;
                        dragSummaryOriginalTime_ = sTime;
                        animDragPreSnapshot_.animation = editingAnimation_;
                        animDragPreSnapshot_.time = animEditorTime_;
                        animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
                        animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
                        animDragPreSnapshot_.description = "サマリーキー移動";
                        hasAnimDragPreSnapshot_ = true;
                    }
                }

                // 右クリックでサマリーキー（全ボーンの該当フレームキー）を一括削除
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && std::abs(mousePos.x - sX) <= 6.0f && std::abs(mousePos.y - sCenterY) <= 6.0f) {
                    deleteSummaryTime = sTime;
                }
            }
        }

        if (deleteSummaryTime >= 0.0f) {
            PushAnimUndoState("全ボーンキー削除");
            for (auto& [nName, nAnim] : editingAnimation_.nodeAnimations) {
                nAnim.rotate.erase(
                    std::remove_if(nAnim.rotate.begin(), nAnim.rotate.end(),
                        [deleteSummaryTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - deleteSummaryTime) < 0.005f; }),
                    nAnim.rotate.end()
                );
                nAnim.translate.erase(
                    std::remove_if(nAnim.translate.begin(), nAnim.translate.end(),
                        [deleteSummaryTime](const KeyframeVector3& kf) { return std::abs(kf.time - deleteSummaryTime) < 0.005f; }),
                    nAnim.translate.end()
                );
                nAnim.scale.erase(
                    std::remove_if(nAnim.scale.begin(), nAnim.scale.end(),
                        [deleteSummaryTime](const KeyframeVector3& kf) { return std::abs(kf.time - deleteSummaryTime) < 0.005f; }),
                    nAnim.scale.end()
                );
            }
            animEditorSelectedKeyIndex_ = -1;
            UpdateAnimationPosePreview(sceneManager);
        }

        if (isSummaryKeyDrag_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float newT = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                newT = std::clamp(newT, 0.0f, editingAnimation_.duration);
                float dt = newT - dragSummaryOriginalTime_;
                if (std::abs(dt) > 0.001f) {
                    for (auto& [nName, nAnim] : editingAnimation_.nodeAnimations) {
                        for (auto& k : nAnim.rotate) {
                            if (std::abs(k.time - dragSummaryOriginalTime_) < 0.005f) {
                                k.time = newT;
                            }
                        }
                        for (auto& k : nAnim.translate) {
                            if (std::abs(k.time - dragSummaryOriginalTime_) < 0.005f) {
                                k.time = newT;
                            }
                        }
                        for (auto& k : nAnim.scale) {
                            if (std::abs(k.time - dragSummaryOriginalTime_) < 0.005f) {
                                k.time = newT;
                            }
                        }
                    }
                    dragSummaryOriginalTime_ = newT;
                    animEditorTime_ = newT;
                    UpdateAnimationPosePreview(sceneManager);
                }
            } else {
                if (hasAnimDragPreSnapshot_) {
                    animUndoStack_.push_back(animDragPreSnapshot_);
                    if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
                    animRedoStack_.clear();
                    hasAnimDragPreSnapshot_ = false;
                }
                isSummaryKeyDrag_ = false;
            }
        }

        // ----------------------------------------------------
        // 5. 各可視トラックのキーフレーム（◆）描画
        // ----------------------------------------------------
        curTrackY = summaryY + summaryHeight;
        for (int i = 0; i < numVisibleTracks; ++i) {
            const auto& item = visibleTracks[i];
            const std::string& jointName = item.name;
            bool isSelected = (animEditorSelectedJointName_ == jointName);

            if (editingAnimation_.nodeAnimations.find(jointName) != editingAnimation_.nodeAnimations.end()) {
                auto& nodeAnim = editingAnimation_.nodeAnimations[jointName];
                for (size_t k = 0; k < nodeAnim.rotate.size(); ++k) {
                    float kTime = nodeAnim.rotate[k].time;
                    float kX = timelineStartX + kTime * animTimelineZoom_ - animTimelineScrollX_;
                    if (kX >= timelineStartX - 10.0f && kX <= timelineEndX + 10.0f) {
                        float kCenterY = curTrackY + trackHeight * 0.5f;
                        bool isKfSelected = (isSelected && animEditorSelectedKeyIndex_ == static_cast<int>(k));
                        
                        ImVec2 kdP[4] = {
                            ImVec2(kX, kCenterY - 4.5f),
                            ImVec2(kX + 4.5f, kCenterY),
                            ImVec2(kX, kCenterY + 4.5f),
                            ImVec2(kX - 4.5f, kCenterY)
                        };
                        ImU32 kCol = isKfSelected ? IM_COL32(255, 215, 50, 255) : IM_COL32(225, 225, 230, 255);
                        drawList->AddConvexPolyFilled(kdP, 4, kCol);
                        drawList->AddPolyline(kdP, 4, IM_COL32(10, 10, 10, 255), ImDrawFlags_Closed, 1.0f);

                        // 左クリックでキーフレーム選択 & 時間シーク
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            if (std::abs(mousePos.x - kX) <= 6.0f && std::abs(mousePos.y - kCenterY) <= 6.0f) {
                                animEditorSelectedJointName_ = jointName;
                                animEditorSelectedKeyIndex_ = static_cast<int>(k);
                                animEditorTime_ = kTime;
                                animTempOverrides_.clear();
                                UpdateAnimationPosePreview(sceneManager);
                            }
                        }

                        // Ctrlキーを押しながらドラッグした場合のみキー移動を許可（通常ドラッグでの誤移動を完全防止）
                        if (io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f) && isSelected) {
                            if (std::abs(io.MouseClickedPos[0].x - kX) <= 6.0f && std::abs(io.MouseClickedPos[0].y - kCenterY) <= 6.0f) {
                                if (!isDraggingAnimKeyframe_) {
                                    isDraggingAnimKeyframe_ = true;
                                    animEditorSelectedJointName_ = jointName;
                                    animEditorSelectedKeyIndex_ = static_cast<int>(k);
                                    dragAnimKeyOriginalTime_ = kTime;
                                    animDragPreSnapshot_.animation = editingAnimation_;
                                    animDragPreSnapshot_.time = animEditorTime_;
                                    animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
                                    animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
                                    animDragPreSnapshot_.description = "キーフレーム移動";
                                    hasAnimDragPreSnapshot_ = true;
                                }
                            }
                        }
                    }
                }
            }

            curTrackY += trackHeight;
        }

        if (isDraggingAnimKeyframe_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float newT = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                newT = std::clamp(newT, 0.0f, editingAnimation_.duration);
                auto& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];
                if (animEditorSelectedKeyIndex_ >= 0 && animEditorSelectedKeyIndex_ < static_cast<int>(nodeAnim.rotate.size())) {
                    float oldT = nodeAnim.rotate[animEditorSelectedKeyIndex_].time;
                    nodeAnim.rotate[animEditorSelectedKeyIndex_].time = newT;
                    for (auto& kf : nodeAnim.translate) {
                        if (std::abs(kf.time - oldT) < 0.005f) kf.time = newT;
                    }
                    for (auto& kf : nodeAnim.scale) {
                        if (std::abs(kf.time - oldT) < 0.005f) kf.time = newT;
                    }
                    animEditorTime_ = newT;
                    UpdateAnimationPosePreview(sceneManager);
                }
            } else {
                auto& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];
                std::sort(nodeAnim.rotate.begin(), nodeAnim.rotate.end(), [](const KeyframeQuaternion& a, const KeyframeQuaternion& b) {
                    return a.time < b.time;
                });
                if (hasAnimDragPreSnapshot_) {
                    animUndoStack_.push_back(animDragPreSnapshot_);
                    if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
                    animRedoStack_.clear();
                    hasAnimDragPreSnapshot_ = false;
                }
                isDraggingAnimKeyframe_ = false;
            }
        }

        // ----------------------------------------------------
        // 6. 左カラム（トラックリスト / 階層ツリー & 折りたたみ）の描画
        // ----------------------------------------------------
        // 左カラム全体背景
        drawList->AddRectFilled(p0, ImVec2(p0.x + trackListWidth, contentBottomY), IM_COL32(32, 33, 37, 255));
        // ルーラー部左カラム背景
        drawList->AddRectFilled(p0, ImVec2(p0.x + trackListWidth, p0.y + rulerHeight), IM_COL32(40, 42, 48, 255));
        drawList->AddLine(ImVec2(p0.x, p0.y + rulerHeight), ImVec2(p0.x + trackListWidth, p0.y + rulerHeight), IM_COL32(65, 68, 76, 255), 1.0f);
        // サマリー行左カラム背景
        drawList->AddRectFilled(ImVec2(p0.x, summaryY), ImVec2(p0.x + trackListWidth, summaryY + summaryHeight), IM_COL32(46, 42, 36, 255));
        drawList->AddLine(ImVec2(p0.x, summaryY + summaryHeight), ImVec2(p0.x + trackListWidth, summaryY + summaryHeight), IM_COL32(70, 64, 55, 255), 1.0f);
        
        // 縦境界線
        drawList->AddLine(ImVec2(p0.x + trackListWidth, p0.y), ImVec2(p0.x + trackListWidth, contentBottomY), IM_COL32(65, 68, 76, 255), 1.5f);

        // ヘッダー行テキスト
        drawList->AddText(ImVec2(p0.x + 8, p0.y + 5), IM_COL32(200, 205, 215, 255), "チャネル / 関節");

        // [+] 全展開 / [-] 全閉じる ボタン
        float btnY = p0.y + 3.0f;
        float btnExpandX = p0.x + trackListWidth - 52.0f;
        float btnCollapseX = p0.x + trackListWidth - 26.0f;

        // 全展開ボタン [+]
        ImVec2 expMin(btnExpandX, btnY);
        ImVec2 expMax(btnExpandX + 22.0f, btnY + 19.0f);
        bool hoverExp = (mousePos.x >= expMin.x && mousePos.x <= expMax.x && mousePos.y >= expMin.y && mousePos.y <= expMax.y);
        drawList->AddRectFilled(expMin, expMax, hoverExp ? IM_COL32(70, 80, 100, 255) : IM_COL32(48, 52, 60, 255), 3.0f);
        drawList->AddRect(expMin, expMax, IM_COL32(85, 92, 105, 255), 3.0f);
        drawList->AddText(ImVec2(expMin.x + 4, expMin.y + 2), IM_COL32(220, 225, 235, 255), "[+]");
        if (hoverExp) {
            ImGui::SetTooltip("すべての階層を展開");
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                for (const auto& node : animJointTreeNodes_) {
                    if (!node.children.empty()) {
                        animJointExpanded_[node.name] = true;
                    }
                }
            }
        }

        // 全閉じるボタン [-]
        ImVec2 colMin(btnCollapseX, btnY);
        ImVec2 colMax(btnCollapseX + 22.0f, btnY + 19.0f);
        bool hoverCol = (mousePos.x >= colMin.x && mousePos.x <= colMax.x && mousePos.y >= colMin.y && mousePos.y <= colMax.y);
        drawList->AddRectFilled(colMin, colMax, hoverCol ? IM_COL32(70, 80, 100, 255) : IM_COL32(48, 52, 60, 255), 3.0f);
        drawList->AddRect(colMin, colMax, IM_COL32(85, 92, 105, 255), 3.0f);
        drawList->AddText(ImVec2(colMin.x + 5, colMin.y + 2), IM_COL32(220, 225, 235, 255), "[-]");
        if (hoverCol) {
            ImGui::SetTooltip("すべての階層を閉じる");
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                for (const auto& node : animJointTreeNodes_) {
                    animJointExpanded_[node.name] = false;
                }
            }
        }

        // サマリー行ラベル
        drawList->AddText(ImVec2(p0.x + 8, summaryY + 4), IM_COL32(245, 185, 85, 255), "[Summary] 概要");

        // 各可視トラック行（左カラム）のツリー描画
        curTrackY = summaryY + summaryHeight;
        for (int i = 0; i < numVisibleTracks; ++i) {
            const auto& item = visibleTracks[i];
            const std::string& jointName = item.name;
            bool isSelected = (animEditorSelectedJointName_ == jointName);

            // 行背景
            ImU32 rowBg = isSelected ? IM_COL32(38, 62, 92, 255) : (i % 2 == 0 ? IM_COL32(34, 35, 39, 255) : IM_COL32(28, 29, 33, 255));
            drawList->AddRectFilled(ImVec2(p0.x, curTrackY), ImVec2(p0.x + trackListWidth, curTrackY + trackHeight), rowBg);
            drawList->AddLine(ImVec2(p0.x, curTrackY + trackHeight), ImVec2(p0.x + trackListWidth, curTrackY + trackHeight), IM_COL32(48, 50, 56, 255), 1.0f);

            float indentX = p0.x + 8.0f + item.depth * 14.0f;
            float rowMidY = curTrackY + trackHeight * 0.5f;

            // 階層接続線（ツリー線）
            if (item.depth > 0) {
                float lineX = indentX - 7.0f;
                drawList->AddLine(ImVec2(lineX, curTrackY), ImVec2(lineX, rowMidY), IM_COL32(80, 85, 95, 180), 1.0f);
                drawList->AddLine(ImVec2(lineX, rowMidY), ImVec2(indentX - 1.0f, rowMidY), IM_COL32(80, 85, 95, 180), 1.0f);
            }

            // トグルアイコン（▶ / ▼）または葉マーカー
            float iconW = 12.0f;
            if (item.hasChildren) {
                ImVec2 toggleMin(indentX, curTrackY + 2.0f);
                ImVec2 toggleMax(indentX + iconW + 4.0f, curTrackY + trackHeight - 2.0f);
                bool isHoverToggle = (mousePos.x >= toggleMin.x && mousePos.x <= toggleMax.x && mousePos.y >= toggleMin.y && mousePos.y <= toggleMax.y);

                if (item.isOpen) {
                    // 下向き三角 ▼
                    ImVec2 tri[3] = {
                        ImVec2(indentX + 2.0f, rowMidY - 3.0f),
                        ImVec2(indentX + 10.0f, rowMidY - 3.0f),
                        ImVec2(indentX + 6.0f, rowMidY + 3.0f)
                    };
                    drawList->AddTriangleFilled(tri[0], tri[1], tri[2], isHoverToggle ? IM_COL32(255, 230, 100, 255) : IM_COL32(200, 205, 220, 255));
                } else {
                    // 右向き三角 ▶
                    ImVec2 tri[3] = {
                        ImVec2(indentX + 3.0f, rowMidY - 5.0f),
                        ImVec2(indentX + 9.0f, rowMidY),
                        ImVec2(indentX + 3.0f, rowMidY + 5.0f)
                    };
                    drawList->AddTriangleFilled(tri[0], tri[1], tri[2], isHoverToggle ? IM_COL32(255, 230, 100, 255) : IM_COL32(170, 175, 190, 255));
                }

                if (isHoverToggle && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    animJointExpanded_[jointName] = !item.isOpen;
                }
            } else {
                // 葉ノード（ドット •）
                drawList->AddCircleFilled(ImVec2(indentX + 5.0f, rowMidY), 2.0f, IM_COL32(110, 115, 130, 255));
            }

            // ジョイント名テキスト
            float textStartX = indentX + iconW + 4.0f;
            ImU32 textCol = isSelected ? IM_COL32(110, 210, 255, 255) : (item.hasChildren ? IM_COL32(235, 240, 250, 255) : IM_COL32(185, 190, 200, 255));
            std::string dispTrackName = jointName + (isSelected && isAnimLocked_ ? " [Locked]" : "");
            drawList->AddText(ImVec2(textStartX, curTrackY + 3.0f), textCol, dispTrackName.c_str());

            // 行クリックによるボーン選択（トグルアイコン以外の領域をクリック時）
            if (!isAnimLocked_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (mousePos.x >= p0.x && mousePos.x <= p0.x + trackListWidth && mousePos.y >= curTrackY && mousePos.y < curTrackY + trackHeight) {
                    bool clickedToggle = item.hasChildren && (mousePos.x >= indentX && mousePos.x <= indentX + iconW + 4.0f);
                    if (!clickedToggle) {
                        animEditorSelectedJointName_ = jointName;
                        animEditorSelectedKeyIndex_ = -1;
                        animTempOverrides_.clear();
                        UpdateAnimationPosePreview(sceneManager);
                    }
                }
            }

            curTrackY += trackHeight;
        }

        // ----------------------------------------------------
        // 7. 垂直再生ヘッド（Playhead）
        // ----------------------------------------------------
        float playheadX = timelineStartX + animEditorTime_ * animTimelineZoom_ - animTimelineScrollX_;
        if (playheadX >= timelineStartX && playheadX <= timelineEndX) {
            drawList->AddLine(ImVec2(playheadX, p0.y), ImVec2(playheadX, contentBottomY), IM_COL32(50, 160, 255, 255), 2.0f);

            ImVec2 badgeP[4] = {
                ImVec2(playheadX - 10.0f, p0.y),
                ImVec2(playheadX + 10.0f, p0.y),
                ImVec2(playheadX + 6.0f, p0.y + rulerHeight - 2),
                ImVec2(playheadX - 6.0f, p0.y + rulerHeight - 2)
            };
            drawList->AddConvexPolyFilled(badgeP, 4, IM_COL32(40, 140, 255, 255));
            drawList->AddPolyline(badgeP, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.0f);

            char phBuf[16];
            snprintf(phBuf, sizeof(phBuf), "%d", curFrame);
            ImVec2 textSize = ImGui::CalcTextSize(phBuf);
            drawList->AddText(ImVec2(playheadX - textSize.x * 0.5f, p0.y + 3), IM_COL32(255, 255, 255, 255), phBuf);
        }

        // タイムライン領域の空きスペースクリックでシーク
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (mousePos.x >= timelineStartX && mousePos.x <= timelineEndX && mousePos.y > summaryY + summaryHeight && !isDraggingAnimKeyframe_ && !isSummaryKeyDrag_) {
                float clickTime = (mousePos.x - timelineStartX + animTimelineScrollX_) / animTimelineZoom_;
                animEditorTime_ = std::clamp(clickTime, 0.0f, editingAnimation_.duration);
                animTempOverrides_.clear();
                UpdateAnimationPosePreview(sceneManager);
            }
        }

        ImGui::Dummy(ImVec2(timelineStartX + maxDuration * animTimelineZoom_, totalHeight));
        ImGui::EndChild();
    }
    ImGui::End();
}

std::string EditorManager::FindOppositeJointName(const std::string& jointName, bool axisX, bool axisY, bool axisZ, const Skeleton* skeleton) {
    if (jointName.empty()) return "";

    // 0. ユーザー手動指定マッピングがあれば最優先で適用
    auto itCustom = customSymmetryMap_.find(jointName);
    if (itCustom != customSymmetryMap_.end() && !itCustom->second.empty()) {
        return itCustom->second;
    }

    if (!axisX && !axisY && !axisZ) return "";

    auto startsWith = [](const std::string& str, const std::string& prefix) -> bool {
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    };

    // 1. 親子関係の階層構造（Skeleton）を用いた探索
    if (skeleton && !skeleton->joints.empty()) {
        auto itJ = skeleton->jointMap.find(jointName);
        if (itJ != skeleton->jointMap.end()) {
            int32_t curIdx = itJ->second;

            // ルートまでの祖先パス（親インデックスのリスト）を構築: [curIdx, parent, grandParent, ..., root]
            std::vector<int32_t> path;
            int32_t temp = curIdx;
            while (temp >= 0 && temp < static_cast<int32_t>(skeleton->joints.size())) {
                path.push_back(temp);
                const auto& j = skeleton->joints[temp];
                if (!j.parent.has_value() || j.parent.value() < 0 || j.parent.value() >= static_cast<int32_t>(skeleton->joints.size())) {
                    break;
                }
                temp = j.parent.value();
            }

            // パスを親方向へ遡り、2つ以上の子を持つ分岐祖先（Branching Ancestor）を探す
            for (size_t p = 1; p < path.size(); ++p) {
                int32_t ancestorIdx = path[p];
                int32_t childOnPath = path[p - 1]; // 祖先の子で、現在のジョイントを含む枝
                const auto& ancestorJoint = skeleton->joints[ancestorIdx];

                if (ancestorJoint.children.size() >= 2) {
                    // 他の子（別の枝）の中から、対称となる枝 branchB を探す
                    int32_t bestOppChild = -1;
                    float bestScore = -1e9f;

                    const auto& jointA = skeleton->joints[childOnPath];
                    Vector3 posA = { jointA.skeletonSpaceMatrix.m[3][0], jointA.skeletonSpaceMatrix.m[3][1], jointA.skeletonSpaceMatrix.m[3][2] };

                    for (int32_t cIdx : ancestorJoint.children) {
                        if (cIdx == childOnPath) continue;
                        if (cIdx < 0 || cIdx >= static_cast<int32_t>(skeleton->joints.size())) continue;

                        const auto& jointB = skeleton->joints[cIdx];
                        Vector3 posB = { jointB.skeletonSpaceMatrix.m[3][0], jointB.skeletonSpaceMatrix.m[3][1], jointB.skeletonSpaceMatrix.m[3][2] };

                        float score = 0.0f;
                        // 座標の対称性スコア
                        float diff = 0.0f;
                        if (axisX) diff += std::abs(posB.x + posA.x);
                        else       diff += std::abs(posB.x - posA.x);

                        if (axisY) diff += std::abs(posB.y + posA.y);
                        else       diff += std::abs(posB.y - posA.y);

                        if (axisZ) diff += std::abs(posB.z + posA.z);
                        else       diff += std::abs(posB.z - posA.z);

                        score -= diff * 10.0f;

                        // 名前のLeft/Right等 対称性ボーナス
                        if (axisX) {
                            if ((jointA.name.find("Left") != std::string::npos && jointB.name.find("Right") != std::string::npos) ||
                                (jointA.name.find("Right") != std::string::npos && jointB.name.find("Left") != std::string::npos) ||
                                (jointA.name.find("_L") != std::string::npos && jointB.name.find("_R") != std::string::npos) ||
                                (jointA.name.find("_R") != std::string::npos && jointB.name.find("_L") != std::string::npos) ||
                                (jointA.name.find(".L") != std::string::npos && jointB.name.find(".R") != std::string::npos) ||
                                (jointA.name.find(".R") != std::string::npos && jointB.name.find(".L") != std::string::npos) ||
                                (jointA.name.find("left") != std::string::npos && jointB.name.find("right") != std::string::npos) ||
                                (jointA.name.find("right") != std::string::npos && jointB.name.find("left") != std::string::npos)) {
                                score += 100.0f;
                            }
                        }
                        if (axisY) {
                            if ((jointA.name.find("Up") != std::string::npos && jointB.name.find("Down") != std::string::npos) ||
                                (jointA.name.find("Down") != std::string::npos && jointB.name.find("Up") != std::string::npos) ||
                                (jointA.name.find("Top") != std::string::npos && jointB.name.find("Bottom") != std::string::npos) ||
                                (jointA.name.find("Bottom") != std::string::npos && jointB.name.find("Top") != std::string::npos)) {
                                score += 100.0f;
                            }
                        }
                        if (axisZ) {
                            if ((jointA.name.find("Front") != std::string::npos && jointB.name.find("Back") != std::string::npos) ||
                                (jointA.name.find("Back") != std::string::npos && jointB.name.find("Front") != std::string::npos) ||
                                (jointA.name.find("Forward") != std::string::npos && jointB.name.find("Backward") != std::string::npos) ||
                                (jointA.name.find("Backward") != std::string::npos && jointB.name.find("Forward") != std::string::npos)) {
                                score += 100.0f;
                            }
                        }

                        if (score > bestScore) {
                            bestScore = score;
                            bestOppChild = cIdx;
                        }
                    }

                    if (bestOppChild >= 0) {
                        // childOnPath から curIdx までの下降ステップ（階層深さと各階層での子インデックス）を収集
                        std::vector<int> stepIndices;
                        for (int k = static_cast<int>(p) - 1; k >= 1; --k) {
                            int32_t parentJ = path[k];
                            int32_t childJ = path[k - 1];
                            const auto& pj = skeleton->joints[parentJ];
                            int childIndexInParent = 0;
                            for (size_t ci = 0; ci < pj.children.size(); ++ci) {
                                if (pj.children[ci] == childJ) {
                                    childIndexInParent = static_cast<int>(ci);
                                    break;
                                }
                            }
                            stepIndices.push_back(childIndexInParent);
                        }

                        // bestOppChild から同じステップを下降して辿る
                        int32_t oppCursor = bestOppChild;
                        bool walkSuccess = true;
                        for (int stepIdx : stepIndices) {
                            const auto& curOppJ = skeleton->joints[oppCursor];
                            if (curOppJ.children.empty()) {
                                walkSuccess = false;
                                break;
                            }
                            if (stepIdx < static_cast<int>(curOppJ.children.size())) {
                                oppCursor = curOppJ.children[stepIdx];
                            } else {
                                oppCursor = curOppJ.children[0];
                            }
                        }

                        if (walkSuccess && oppCursor >= 0 && oppCursor < static_cast<int>(skeleton->joints.size())) {
                            return skeleton->joints[oppCursor].name;
                        }
                    }
                }
            }
        }
    }

    // 2. 階層から見つからなかった場合のフォールバック（文字列置換パターン）
    if (!currentJointList_.empty()) {
        if (axisX) { // X軸対称 (左右: Left <-> Right)
            std::vector<std::pair<std::string, std::string>> patterns = {
                { "Left", "Right" }, { "left", "right" }, { "LEFT", "RIGHT" },
                { "_L", "_R" }, { "_l", "_r" },
                { ".L", ".R" }, { ".l", ".r" },
                { "L_", "R_" }, { "l_", "r_" }
            };

            for (const auto& [pL, pR] : patterns) {
                auto posL = jointName.find(pL);
                if (posL != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posL, pL.length(), pR);
                    for (const auto& j : currentJointList_) {
                        if (j == targetFull) return j;
                    }
                    std::string prefix = jointName.substr(0, posL) + pR;
                    for (const auto& j : currentJointList_) {
                        if (startsWith(j, prefix) && j != jointName) return j;
                    }
                }

                auto posR = jointName.find(pR);
                if (posR != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posR, pR.length(), pL);
                    for (const auto& j : currentJointList_) {
                        if (j == targetFull) return j;
                    }
                    std::string prefix = jointName.substr(0, posR) + pL;
                    for (const auto& j : currentJointList_) {
                        if (startsWith(j, prefix) && j != jointName) return j;
                    }
                }
            }
        }
        if (axisY) { // Y軸対称 (上下: Up <-> Down, Top <-> Bottom)
            std::vector<std::pair<std::string, std::string>> patterns = {
                { "Up", "Down" }, { "up", "down" }, { "UP", "DOWN" },
                { "Top", "Bottom" }, { "top", "bottom" },
                { "Upper", "Lower" }, { "upper", "lower" }
            };
            for (const auto& [pU, pD] : patterns) {
                auto posU = jointName.find(pU);
                if (posU != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posU, pU.length(), pD);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posU) + pD;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
                auto posD = jointName.find(pD);
                if (posD != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posD, pD.length(), pU);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posD) + pU;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
            }
        }
        if (axisZ) { // Z軸対称 (前後: Front <-> Back)
            std::vector<std::pair<std::string, std::string>> patterns = {
                { "Front", "Back" }, { "front", "back" }, { "FRONT", "BACK" },
                { "Forward", "Backward" }, { "forward", "backward" }
            };
            for (const auto& [pF, pB] : patterns) {
                auto posF = jointName.find(pF);
                if (posF != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posF, pF.length(), pB);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posF) + pB;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
                auto posB = jointName.find(pB);
                if (posB != std::string::npos) {
                    std::string targetFull = jointName;
                    targetFull.replace(posB, pB.length(), pF);
                    for (const auto& j : currentJointList_) if (j == targetFull) return j;
                    std::string prefix = jointName.substr(0, posB) + pF;
                    for (const auto& j : currentJointList_) if (startsWith(j, prefix) && j != jointName) return j;
                }
            }
        }
    }

    return "";
}

void EditorManager::DrawAnimationInspectorUI(SceneManager* sceneManager) {
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "[Animation Properties] ボーン SRT 設定");
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    bool canUndo = !animUndoStack_.empty();
    bool canRedo = !animRedoStack_.empty();

    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("戻る (Ctrl+Z)")) {
        PerformAnimUndo(sceneManager);
    }
    if (!canUndo) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canRedo) ImGui::BeginDisabled();
    if (ImGui::Button("進む (Ctrl+Y)")) {
        PerformAnimRedo(sceneManager);
    }
    if (!canRedo) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (isAnimLocked_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("ロック中 (L)")) isAnimLocked_ = false;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lキーでボーン選択固定を解除");
    } else {
        if (ImGui::Button("ロック (L)")) isAnimLocked_ = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lキーで選択ボーンを固定（誤選択防止）");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------------
    // アニメーション全体設定 (Duration / FPS / 総フレーム数)
    // ----------------------------------------------------
    if (ImGui::CollapsingHeader("アニメーション全体設定 (Duration / FPS)", ImGuiTreeNodeFlags_DefaultOpen)) {
        int totalFrames = static_cast<int>(std::round(editingAnimation_.duration * animEditorFps_));
        int curFrame = static_cast<int>(std::round(animEditorTime_ * animEditorFps_));

        ImGui::Text("フレーム情報: %d / %d F (%.3fs / %.3fs)", curFrame, totalFrames, animEditorTime_, editingAnimation_.duration);

        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragInt("現在フレーム (Current F)", &curFrame, 1.0f, 0, totalFrames, "%d F")) {
            if (curFrame < 0) curFrame = 0;
            if (curFrame > totalFrames) curFrame = totalFrames;
            animEditorTime_ = static_cast<float>(curFrame) / (animEditorFps_ > 0.0f ? animEditorFps_ : 60.0f);
            animTempOverrides_.clear();
            UpdateAnimationPosePreview(sceneManager);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragInt("最大フレーム数 (Duration)", &totalFrames, 1.0f, 1, 6000, "%d F")) {
            if (totalFrames < 1) totalFrames = 1;
            editingAnimation_.duration = static_cast<float>(totalFrames) / (animEditorFps_ > 0.0f ? animEditorFps_ : 60.0f);
        }
        if (ImGui::IsItemActivated()) {
            animDragPreSnapshot_.animation = editingAnimation_;
            animDragPreSnapshot_.time = animEditorTime_;
            animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
            animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
            animDragPreSnapshot_.description = "フレーム数変更";
            hasAnimDragPreSnapshot_ = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && hasAnimDragPreSnapshot_) {
            animUndoStack_.push_back(animDragPreSnapshot_);
            if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
            animRedoStack_.clear();
            hasAnimDragPreSnapshot_ = false;
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%.2f 秒]", editingAnimation_.duration);

        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("フレームレート (FPS)", &animEditorFps_, 1.0f, 10.0f, 120.0f, "%.0f fps");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    std::string targetName = "Player (プレイヤー)";
    if (selectedGameObject_) targetName = selectedGameObject_->GetName();
    else if (selectedObject_) targetName = selectedObject_->GetName();
    ImGui::Text("対象モデル: %s", targetName.c_str());

    ImGui::Spacing();
    ImGui::Text("選択ボーン (Joint):");
    ImGui::SetNextItemWidth(-1.0f);
    if (isAnimLocked_) ImGui::BeginDisabled();
    if (ImGui::BeginCombo("##SelectedJointCombo", animEditorSelectedJointName_.c_str())) {
        for (const auto& jName : currentJointList_) {
            bool isSel = (animEditorSelectedJointName_ == jName);
            if (ImGui::Selectable(jName.c_str(), isSel)) {
                animEditorSelectedJointName_ = jName;
                animEditorSelectedKeyIndex_ = -1;
                animTempOverrides_.clear();
                UpdateAnimationPosePreview(sceneManager);
            }
        }
        ImGui::EndCombo();
    }
    if (isAnimLocked_) ImGui::EndDisabled();

    ImGui::Spacing();

    // ギズモ操作モード切替ボタン群 (SRT & Lock)
    ImGui::Text("ギズモ操作モード:");
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    {
        bool isTrans = (animGizmoMode_ == 0);
        if (isTrans) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("移動 (T)", ImVec2(65, 24))) animGizmoMode_ = 0;
        if (isTrans) ImGui::PopStyleColor();

        ImGui::SameLine();
        bool isRot = (animGizmoMode_ == 1);
        if (isRot) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.75f, 0.3f, 1.0f));
        if (ImGui::Button("回転 (R)", ImVec2(65, 24))) animGizmoMode_ = 1;
        if (isRot) ImGui::PopStyleColor();

        ImGui::SameLine();
        bool isScale = (animGizmoMode_ == 2);
        if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
        if (ImGui::Button("拡縮 (S)", ImVec2(65, 24))) animGizmoMode_ = 2;
        if (isScale) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button(animGizmoSpace_ == 0 ? "Local" : "World", ImVec2(55, 24))) {
            animGizmoSpace_ = (animGizmoSpace_ == 0) ? 1 : 0;
        }

        ImGui::SameLine();
        if (isAnimLocked_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button("ロック中", ImVec2(80, 24))) isAnimLocked_ = false;
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("ロック (L)", ImVec2(80, 24))) isAnimLocked_ = true;
        }
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (isAnimLocked_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("[ボーン選択固定中] Lキーで解除");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    NodeAnimation& nodeAnim = editingAnimation_.nodeAnimations[animEditorSelectedJointName_];
    AnimatorComponent* anim = GetTargetAnimator(sceneManager);

    // ----------------------------------------------------
    // 対称編集モード (Symmetry Mode) 設定UI
    // ----------------------------------------------------
    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.95f, 1.0f), "[Symmetry] リアルタイム対称編集 (ミラー編集):");
    ImGui::Checkbox("対称編集を有効化", &animSymmetryMode_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("有効にすると、操作したボーンに対となるボーンが対称的に連動してリアルタイム更新されます");

    bool hasAnySymmetryAxis = animSymmetryAxisX_ || animSymmetryAxisY_ || animSymmetryAxisZ_;
    std::string oppJointName = (animSymmetryMode_ && hasAnySymmetryAxis) ? FindOppositeJointName(animEditorSelectedJointName_, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, anim ? &anim->GetSkeleton() : nullptr) : "";

    if (animSymmetryMode_) {
        ImGui::SameLine();
        ImGui::Text("  対称軸:");
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        // X ボタン
        if (animSymmetryAxisX_) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        if (ImGui::Button("X", ImVec2(28, 22))) {
            animSymmetryAxisX_ = !animSymmetryAxisX_;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("X軸対称 (左右ミラー): %s", animSymmetryAxisX_ ? "ON" : "OFF");

        ImGui::SameLine();
        // Y ボタン
        if (animSymmetryAxisY_) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.75f, 0.25f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        if (ImGui::Button("Y", ImVec2(28, 22))) {
            animSymmetryAxisY_ = !animSymmetryAxisY_;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Y軸対称 (上下ミラー): %s", animSymmetryAxisY_ ? "ON" : "OFF");

        ImGui::SameLine();
        // Z ボタン
        if (animSymmetryAxisZ_) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.85f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        if (ImGui::Button("Z", ImVec2(28, 22))) {
            animSymmetryAxisZ_ = !animSymmetryAxisZ_;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Z軸対称 (前後ミラー): %s", animSymmetryAxisZ_ ? "ON" : "OFF");

        ImGui::PopStyleVar(2);

        if (!hasAnySymmetryAxis) {
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "  -> (対称軸が選択されていません: XYZボタンをクリックして選択)");
        } else if (!oppJointName.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "  -> 対称ボーン: %s (連動中)", oppJointName.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  -> (単一/中央ボーン: 対称先なし)");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 対称ボーンへのリアルタイム連動更新ヘルパー
    auto syncOppositeBoneSRT = [&](const Vector3* newTrans, const Quaternion* newRot, const Vector3* newScale, bool isExplicitInsert = false) {
        if (!animSymmetryMode_ || !hasAnySymmetryAxis || oppJointName.empty() || oppJointName == animEditorSelectedJointName_) return;
        if (!anim || !anim->HasSkeleton()) return;

        const Skeleton& skeleton = anim->GetSkeleton();

        Vector3 curS = nodeAnim.scale.empty() ? Vector3{ 1.0f, 1.0f, 1.0f } : CalculateValue(nodeAnim.scale, animEditorTime_);
        Quaternion curR = nodeAnim.rotate.empty() ? Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } : CalculateValue(nodeAnim.rotate, animEditorTime_);
        Vector3 curT = nodeAnim.translate.empty() ? Vector3{ 0.0f, 0.0f, 0.0f } : CalculateValue(nodeAnim.translate, animEditorTime_);

        auto itTemp = animTempOverrides_.find(animEditorSelectedJointName_);
        if (itTemp != animTempOverrides_.end()) {
            if (itTemp->second.translate) curT = *itTemp->second.translate;
            if (itTemp->second.rotate) curR = *itTemp->second.rotate;
            if (itTemp->second.scale) curS = *itTemp->second.scale;
        }

        if (newTrans) curT = *newTrans;
        if (newRot) curR = *newRot;
        if (newScale) curS = *newScale;

        Vector3 oppS, oppT;
        Quaternion oppQ;
        if (!ComputeBlenderSymmetrySRT(skeleton, animEditorSelectedJointName_, oppJointName, curS, curR, curT, animSymmetryAxisX_, animSymmetryAxisY_, animSymmetryAxisZ_, oppS, oppQ, oppT)) {
            return;
        }

        NodeAnimation& oppNodeAnim = editingAnimation_.nodeAnimations[oppJointName];

        if (newTrans) {
            bool found = false;
            for (size_t idx = 0; idx < oppNodeAnim.translate.size(); ++idx) {
                if (std::abs(oppNodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                    oppNodeAnim.translate[idx].value = oppT;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (isExplicitInsert) {
                    KeyframeVector3 newKf{ animEditorTime_, oppT };
                    auto itK = oppNodeAnim.translate.begin();
                    while (itK != oppNodeAnim.translate.end() && itK->time < newKf.time) ++itK;
                    oppNodeAnim.translate.insert(itK, newKf);
                } else {
                    animTempOverrides_[oppJointName].translate = oppT;
                }
            }
        }

        if (newRot) {
            bool found = false;
            for (size_t idx = 0; idx < oppNodeAnim.rotate.size(); ++idx) {
                if (std::abs(oppNodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                    oppNodeAnim.rotate[idx].value = oppQ;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (isExplicitInsert) {
                    KeyframeQuaternion newKf{ animEditorTime_, oppQ };
                    auto itK = oppNodeAnim.rotate.begin();
                    while (itK != oppNodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
                    oppNodeAnim.rotate.insert(itK, newKf);
                } else {
                    animTempOverrides_[oppJointName].rotate = oppQ;
                }
            }
        }

        if (newScale) {
            bool found = false;
            for (size_t idx = 0; idx < oppNodeAnim.scale.size(); ++idx) {
                if (std::abs(oppNodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                    oppNodeAnim.scale[idx].value = oppS;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (isExplicitInsert) {
                    KeyframeVector3 newKf{ animEditorTime_, oppS };
                    auto itK = oppNodeAnim.scale.begin();
                    while (itK != oppNodeAnim.scale.end() && itK->time < newKf.time) ++itK;
                    oppNodeAnim.scale.insert(itK, newKf);
                } else {
                    animTempOverrides_[oppJointName].scale = oppS;
                }
            }
        }
    };

    // ----------------------------------------------------
    // 1. 平行移動 (Translation / T)
    // ----------------------------------------------------
    Vector3 curTrans = { 0.0f, 0.0f, 0.0f };
    auto itTempT = animTempOverrides_.find(animEditorSelectedJointName_);
    if (itTempT != animTempOverrides_.end() && itTempT->second.translate) {
        curTrans = *itTempT->second.translate;
    } else if (!nodeAnim.translate.empty()) {
        curTrans = CalculateValue(nodeAnim.translate, animEditorTime_);
    } else if (anim && anim->HasSkeleton()) {
        const auto& skel = anim->GetSkeleton();
        auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
        if (itJ != skel.jointMap.end()) {
            curTrans = skel.joints[itJ->second].transform.translate;
        }
    }

    float transArr[3] = { curTrans.x, curTrans.y, curTrans.z };
    ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "[T] 平行移動 (Translation):");
    if (ImGui::DragFloat3("位置 (X, Y, Z)##Trans", transArr, 0.01f, -100.0f, 100.0f, "%.3f")) {
        Vector3 newTrans{ transArr[0], transArr[1], transArr[2] };
        bool found = false;
        for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
            if (std::abs(nodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.translate[idx].value = newTrans;
                found = true;
                break;
            }
        }
        if (!found) {
            animTempOverrides_[animEditorSelectedJointName_].translate = newTrans;
        }
        syncOppositeBoneSRT(&newTrans, nullptr, nullptr);
        UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::IsItemActivated()) {
        animDragPreSnapshot_.animation = editingAnimation_;
        animDragPreSnapshot_.time = animEditorTime_;
        animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
        animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
        animDragPreSnapshot_.description = "位置変更";
        hasAnimDragPreSnapshot_ = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && hasAnimDragPreSnapshot_) {
        animUndoStack_.push_back(animDragPreSnapshot_);
        if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
        animRedoStack_.clear();
        hasAnimDragPreSnapshot_ = false;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##T")) {
        PushAnimUndoState("位置リセット");
        Vector3 defTrans = { 0.0f, 0.0f, 0.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
            if (itJ != skel.jointMap.end()) defTrans = skel.joints[itJ->second].defaultTransform.translate;
        }
        KeyframeVector3 newKf{ animEditorTime_, defTrans };
        nodeAnim.translate.push_back(newKf);
        syncOppositeBoneSRT(&defTrans, nullptr, nullptr);
        animTempOverrides_[animEditorSelectedJointName_].translate.reset();
        if (!oppJointName.empty()) animTempOverrides_[oppJointName].translate.reset();
        UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();

    // ----------------------------------------------------
    // 2. 回転 (Rotation / R)
    // ----------------------------------------------------
    Quaternion curQuat = { 0.0f, 0.0f, 0.0f, 1.0f };
    auto itTempR = animTempOverrides_.find(animEditorSelectedJointName_);
    if (itTempR != animTempOverrides_.end() && itTempR->second.rotate) {
        curQuat = *itTempR->second.rotate;
    } else if (!nodeAnim.rotate.empty()) {
        curQuat = CalculateValue(nodeAnim.rotate, animEditorTime_);
    } else if (anim && anim->HasSkeleton()) {
        const auto& skel = anim->GetSkeleton();
        auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
        if (itJ != skel.jointMap.end()) {
            curQuat = skel.joints[itJ->second].transform.rotate;
        }
    }

    Vector3 euler = curQuat.ToEulerAngles();
    float eulerDeg[3] = { euler.x * 180.0f / 3.14159265f, euler.y * 180.0f / 3.14159265f, euler.z * 180.0f / 3.14159265f };
    float eulerRad[3] = { euler.x, euler.y, euler.z };

    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.4f, 1.0f), "[R] 回転 (Rotation):");
    bool rotChanged = false;
    if (ImGui::DragFloat3("度 (Deg: X, Y, Z)##RotDeg", eulerDeg, 0.5f, -360.0f, 360.0f, "%.1f°")) {
        rotChanged = true;
    }
    if (ImGui::IsItemActivated()) {
        animDragPreSnapshot_.animation = editingAnimation_;
        animDragPreSnapshot_.time = animEditorTime_;
        animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
        animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
        animDragPreSnapshot_.description = "回転変更";
        hasAnimDragPreSnapshot_ = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && hasAnimDragPreSnapshot_) {
        animUndoStack_.push_back(animDragPreSnapshot_);
        if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
        animRedoStack_.clear();
        hasAnimDragPreSnapshot_ = false;
    }

    if (ImGui::DragFloat3("ラジアン (Rad)##RotRad", eulerRad, 0.01f, -6.283f, 6.283f, "%.3f rad")) {
        eulerDeg[0] = eulerRad[0] * 180.0f / 3.14159265f;
        eulerDeg[1] = eulerRad[1] * 180.0f / 3.14159265f;
        eulerDeg[2] = eulerRad[2] * 180.0f / 3.14159265f;
        rotChanged = true;
    }
    if (ImGui::IsItemActivated()) {
        animDragPreSnapshot_.animation = editingAnimation_;
        animDragPreSnapshot_.time = animEditorTime_;
        animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
        animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
        animDragPreSnapshot_.description = "回転変更";
        hasAnimDragPreSnapshot_ = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && hasAnimDragPreSnapshot_) {
        animUndoStack_.push_back(animDragPreSnapshot_);
        if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
        animRedoStack_.clear();
        hasAnimDragPreSnapshot_ = false;
    }

    if (rotChanged) {
        float rx = eulerDeg[0] * 3.14159265f / 180.0f;
        float ry = eulerDeg[1] * 3.14159265f / 180.0f;
        float rz = eulerDeg[2] * 3.14159265f / 180.0f;
        Quaternion newQ = MakeEulerQuat(rx, ry, rz);

        bool foundKey = false;
        for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
            if (std::abs(nodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.rotate[idx].value = newQ;
                animEditorSelectedKeyIndex_ = static_cast<int>(idx);
                foundKey = true;
                break;
            }
        }
        if (!foundKey) {
            animTempOverrides_[animEditorSelectedJointName_].rotate = newQ;
        }
        syncOppositeBoneSRT(nullptr, &newQ, nullptr);
        UpdateAnimationPosePreview(sceneManager);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##R")) {
        PushAnimUndoState("回転リセット");
        Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
            if (itJ != skel.jointMap.end()) defRot = skel.joints[itJ->second].defaultTransform.rotate;
        }
        KeyframeQuaternion newKf{ animEditorTime_, defRot };
        nodeAnim.rotate.push_back(newKf);
        syncOppositeBoneSRT(nullptr, &defRot, nullptr);
        animTempOverrides_[animEditorSelectedJointName_].rotate.reset();
        if (!oppJointName.empty()) animTempOverrides_[oppJointName].rotate.reset();
        UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();

    // ----------------------------------------------------
    // 3. 拡大縮小 (Scale / S)
    // ----------------------------------------------------
    Vector3 curScale = { 1.0f, 1.0f, 1.0f };
    auto itTempS = animTempOverrides_.find(animEditorSelectedJointName_);
    if (itTempS != animTempOverrides_.end() && itTempS->second.scale) {
        curScale = *itTempS->second.scale;
    } else if (!nodeAnim.scale.empty()) {
        curScale = CalculateValue(nodeAnim.scale, animEditorTime_);
    } else if (anim && anim->HasSkeleton()) {
        const auto& skel = anim->GetSkeleton();
        auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
        if (itJ != skel.jointMap.end()) {
            curScale = skel.joints[itJ->second].transform.scale;
        }
    }

    float scaleArr[3] = { curScale.x, curScale.y, curScale.z };
    ImGui::TextColored(ImVec4(0.4f, 0.65f, 0.95f, 1.0f), "[S] 拡大縮小 (Scale):");
    if (ImGui::DragFloat3("スケール (X, Y, Z)##Scale", scaleArr, 0.01f, 0.001f, 20.0f, "%.3f")) {
        Vector3 newSc{ scaleArr[0], scaleArr[1], scaleArr[2] };
        bool found = false;
        for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
            if (std::abs(nodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.scale[idx].value = newSc;
                found = true;
                break;
            }
        }
        if (!found) {
            animTempOverrides_[animEditorSelectedJointName_].scale = newSc;
        }
        syncOppositeBoneSRT(nullptr, nullptr, &newSc);
        UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::IsItemActivated()) {
        animDragPreSnapshot_.animation = editingAnimation_;
        animDragPreSnapshot_.time = animEditorTime_;
        animDragPreSnapshot_.selectedJointName = animEditorSelectedJointName_;
        animDragPreSnapshot_.selectedKeyIndex = animEditorSelectedKeyIndex_;
        animDragPreSnapshot_.description = "スケール変更";
        hasAnimDragPreSnapshot_ = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && hasAnimDragPreSnapshot_) {
        animUndoStack_.push_back(animDragPreSnapshot_);
        if (animUndoStack_.size() > 64) animUndoStack_.erase(animUndoStack_.begin());
        animRedoStack_.clear();
        hasAnimDragPreSnapshot_ = false;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##S")) {
        PushAnimUndoState("スケールリセット");
        Vector3 defSc = { 1.0f, 1.0f, 1.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
            if (itJ != skel.jointMap.end()) defSc = skel.joints[itJ->second].defaultTransform.scale;
        }
        KeyframeVector3 newKf{ animEditorTime_, defSc };
        nodeAnim.scale.push_back(newKf);
        syncOppositeBoneSRT(nullptr, nullptr, &defSc);
        animTempOverrides_[animEditorSelectedJointName_].scale.reset();
        if (!oppJointName.empty()) animTempOverrides_[oppJointName].scale.reset();
        UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------------
    // キー挿入・リセット ボタン群
    // ----------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.1f, 1.0f));
    if (ImGui::Button("[+] 全SRTをキー挿入 (I)", ImVec2(-1, 28))) {
        PushAnimUndoState("全SRTキー挿入");

        // Translation (重複チェック & 上書き)
        Vector3 targetTrans{ transArr[0], transArr[1], transArr[2] };
        bool foundT = false;
        for (size_t idx = 0; idx < nodeAnim.translate.size(); ++idx) {
            if (std::abs(nodeAnim.translate[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.translate[idx].value = targetTrans;
                foundT = true;
                break;
            }
        }
        if (!foundT) {
            KeyframeVector3 newKfT{ animEditorTime_, targetTrans };
            auto itT = nodeAnim.translate.begin();
            while (itT != nodeAnim.translate.end() && itT->time < newKfT.time) ++itT;
            nodeAnim.translate.insert(itT, newKfT);
        }

        // Rotation (curQuatを直接使用し、重複チェック & 上書き)
        Quaternion targetRot = curQuat;
        bool foundR = false;
        for (size_t idx = 0; idx < nodeAnim.rotate.size(); ++idx) {
            if (std::abs(nodeAnim.rotate[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.rotate[idx].value = targetRot;
                animEditorSelectedKeyIndex_ = static_cast<int>(idx);
                foundR = true;
                break;
            }
        }
        if (!foundR) {
            KeyframeQuaternion newKfR{ animEditorTime_, targetRot };
            auto itR = nodeAnim.rotate.begin();
            while (itR != nodeAnim.rotate.end() && itR->time < newKfR.time) ++itR;
            auto inserted = nodeAnim.rotate.insert(itR, newKfR);
            animEditorSelectedKeyIndex_ = static_cast<int>(std::distance(nodeAnim.rotate.begin(), inserted));
        }

        // Scale (重複チェック & 上書き)
        Vector3 targetScale{ scaleArr[0], scaleArr[1], scaleArr[2] };
        bool foundS = false;
        for (size_t idx = 0; idx < nodeAnim.scale.size(); ++idx) {
            if (std::abs(nodeAnim.scale[idx].time - animEditorTime_) < 0.005f) {
                nodeAnim.scale[idx].value = targetScale;
                foundS = true;
                break;
            }
        }
        if (!foundS) {
            KeyframeVector3 newKfS{ animEditorTime_, targetScale };
            auto itS = nodeAnim.scale.begin();
            while (itS != nodeAnim.scale.end() && itS->time < newKfS.time) ++itS;
            nodeAnim.scale.insert(itS, newKfS);
        }

        syncOppositeBoneSRT(&targetTrans, &targetRot, &targetScale, true);
        animTempOverrides_.erase(animEditorSelectedJointName_);
        if (!oppJointName.empty()) animTempOverrides_.erase(oppJointName);
        UpdateAnimationPosePreview(sceneManager);
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("[-] 選択ボーンの現在キー削除 (Del)", ImVec2(-1, 24))) {
        PushAnimUndoState("現在キー削除");
        float curT = animEditorTime_;
        nodeAnim.rotate.erase(
            std::remove_if(nodeAnim.rotate.begin(), nodeAnim.rotate.end(),
                [curT](const KeyframeQuaternion& kf) { return std::abs(kf.time - curT) < 0.005f; }),
            nodeAnim.rotate.end()
        );
        nodeAnim.translate.erase(
            std::remove_if(nodeAnim.translate.begin(), nodeAnim.translate.end(),
                [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
            nodeAnim.translate.end()
        );
        nodeAnim.scale.erase(
            std::remove_if(nodeAnim.scale.begin(), nodeAnim.scale.end(),
                [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
            nodeAnim.scale.end()
        );
        animEditorSelectedKeyIndex_ = -1;
        animTempOverrides_.erase(animEditorSelectedJointName_);
        if (!oppJointName.empty()) animTempOverrides_.erase(oppJointName);
        UpdateAnimationPosePreview(sceneManager);
    }

    if (ImGui::Button("[-] 全ボーンの現在キー削除 (Shift+Del)", ImVec2(-1, 24))) {
        PushAnimUndoState("全ボーン現在キー削除");
        float curT = animEditorTime_;
        for (auto& [nName, nAnim] : editingAnimation_.nodeAnimations) {
            nAnim.rotate.erase(
                std::remove_if(nAnim.rotate.begin(), nAnim.rotate.end(),
                    [curT](const KeyframeQuaternion& kf) { return std::abs(kf.time - curT) < 0.005f; }),
                nAnim.rotate.end()
            );
            nAnim.translate.erase(
                std::remove_if(nAnim.translate.begin(), nAnim.translate.end(),
                    [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
                nAnim.translate.end()
            );
            nAnim.scale.erase(
                std::remove_if(nAnim.scale.begin(), nAnim.scale.end(),
                    [curT](const KeyframeVector3& kf) { return std::abs(kf.time - curT) < 0.005f; }),
                nAnim.scale.end()
            );
        }
        animEditorSelectedKeyIndex_ = -1;
        animTempOverrides_.clear();
        UpdateAnimationPosePreview(sceneManager);
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    if (ImGui::Button("[T-Pose] 選択ボーンをTポーズ(0)に", ImVec2(-1, 24))) {
        PushAnimUndoState("Tポーズ設定");
        Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (anim && anim->HasSkeleton()) {
            const auto& skel = anim->GetSkeleton();
            auto itJ = skel.jointMap.find(animEditorSelectedJointName_);
            if (itJ != skel.jointMap.end()) defRot = skel.joints[itJ->second].defaultTransform.rotate;
        }

        bool foundR = false;
        for (auto& kf : nodeAnim.rotate) {
            if (std::abs(kf.time - animEditorTime_) < 0.005f) {
                kf.value = defRot;
                foundR = true;
                break;
            }
        }
        if (!foundR) {
            KeyframeQuaternion newKf{ animEditorTime_, defRot };
            auto itK = nodeAnim.rotate.begin();
            while (itK != nodeAnim.rotate.end() && itK->time < newKf.time) ++itK;
            auto inserted = nodeAnim.rotate.insert(itK, newKf);
            animEditorSelectedKeyIndex_ = static_cast<int>(std::distance(nodeAnim.rotate.begin(), inserted));
        }
        animTempOverrides_.erase(animEditorSelectedJointName_);
        if (!oppJointName.empty()) animTempOverrides_.erase(oppJointName);
        UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::Button("[T-Pose] 全ボーンをTポーズに (現在フレーム)", ImVec2(-1, 24))) {
        PushAnimUndoState("全ボーンTポーズ設定");
        const Skeleton* skelPtr = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

        for (const auto& jName : currentJointList_) {
            NodeAnimation& nAnim = editingAnimation_.nodeAnimations[jName];

            Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
            Vector3 defTrans{ 0.0f, 0.0f, 0.0f };
            Vector3 defScale{ 1.0f, 1.0f, 1.0f };

            if (skelPtr) {
                auto itJ = skelPtr->jointMap.find(jName);
                if (itJ != skelPtr->jointMap.end()) {
                    const auto& j = skelPtr->joints[itJ->second];
                    defRot = j.defaultTransform.rotate;
                    defTrans = j.defaultTransform.translate;
                    defScale = j.defaultTransform.scale;
                }
            }

            // 1. 回転をTポーズ(defaultTransform.rotate)に設定
            bool foundR = false;
            for (auto& kf : nAnim.rotate) {
                if (std::abs(kf.time - animEditorTime_) < 0.005f) {
                    kf.value = defRot;
                    foundR = true;
                    break;
                }
            }
            if (!foundR) {
                KeyframeQuaternion newKfR{ animEditorTime_, defRot };
                auto itR = nAnim.rotate.begin();
                while (itR != nAnim.rotate.end() && itR->time < newKfR.time) ++itR;
                nAnim.rotate.insert(itR, newKfR);
            }

            // 2. 移動 (すでに移動キーが存在する場合のみ、defaultTransform.translate に復帰)
            if (!nAnim.translate.empty()) {
                bool foundT = false;
                for (auto& kf : nAnim.translate) {
                    if (std::abs(kf.time - animEditorTime_) < 0.005f) {
                        kf.value = defTrans;
                        foundT = true;
                        break;
                    }
                }
                if (!foundT) {
                    KeyframeVector3 newKfT{ animEditorTime_, defTrans };
                    auto itT = nAnim.translate.begin();
                    while (itT != nAnim.translate.end() && itT->time < newKfT.time) ++itT;
                    nAnim.translate.insert(itT, newKfT);
                }
            }

            // 3. 拡縮 (すでに拡縮キーが存在する場合のみ、defaultTransform.scale に復帰)
            if (!nAnim.scale.empty()) {
                bool foundS = false;
                for (auto& kf : nAnim.scale) {
                    if (std::abs(kf.time - animEditorTime_) < 0.005f) {
                        kf.value = defScale;
                        foundS = true;
                        break;
                    }
                }
                if (!foundS) {
                    KeyframeVector3 newKfS{ animEditorTime_, defScale };
                    auto itS = nAnim.scale.begin();
                    while (itS != nAnim.scale.end() && itS->time < newKfS.time) ++itS;
                    nAnim.scale.insert(itS, newKfS);
                }
            }
        }
        animTempOverrides_.clear();
        UpdateAnimationPosePreview(sceneManager);
    }
    if (ImGui::Button("[T-Pose] 全キーフレームをTポーズに初期化", ImVec2(-1, 24))) {
        PushAnimUndoState("全キーフレームTポーズ初期化");
        const Skeleton* skelPtr = (anim && anim->HasSkeleton()) ? &anim->GetSkeleton() : nullptr;

        for (const auto& jName : currentJointList_) {
            NodeAnimation& nAnim = editingAnimation_.nodeAnimations[jName];
            nAnim.rotate.clear();
            nAnim.translate.clear();
            nAnim.scale.clear();

            Quaternion defRot{ 0.0f, 0.0f, 0.0f, 1.0f };
            Vector3 defTrans{ 0.0f, 0.0f, 0.0f };
            Vector3 defScale{ 1.0f, 1.0f, 1.0f };

            if (skelPtr) {
                auto itJ = skelPtr->jointMap.find(jName);
                if (itJ != skelPtr->jointMap.end()) {
                    const auto& j = skelPtr->joints[itJ->second];
                    defRot = j.defaultTransform.rotate;
                    defTrans = j.defaultTransform.translate;
                    defScale = j.defaultTransform.scale;
                }
            }

            nAnim.rotate.push_back({ 0.0f, defRot });
            nAnim.translate.push_back({ 0.0f, defTrans });
            nAnim.scale.push_back({ 0.0f, defScale });
        }
        animEditorSelectedKeyIndex_ = -1;
        animTempOverrides_.clear();
        UpdateAnimationPosePreview(sceneManager);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ----------------------------------------------------
    // キーフレーム一覧
    // ----------------------------------------------------
    std::vector<float> boneKeyTimes;
    {
        std::set<float> timeSet;
        for (const auto& k : nodeAnim.rotate) timeSet.insert(k.time);
        for (const auto& k : nodeAnim.translate) timeSet.insert(k.time);
        for (const auto& k : nodeAnim.scale) timeSet.insert(k.time);
        boneKeyTimes.assign(timeSet.begin(), timeSet.end());
        std::sort(boneKeyTimes.begin(), boneKeyTimes.end());
    }

    ImGui::Text("登録キーフレーム一覧 (%zu 個):", boneKeyTimes.size());
    if (boneKeyTimes.empty()) {
        ImGui::TextDisabled("キーフレームがありません。");
    } else {
        if (ImGui::BeginTable("##KeyframeListTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("フレーム / 時間", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 60.0f);

            int toDeleteIndex = -1;
            float toDeleteTime = -1.0f;

            for (size_t k = 0; k < boneKeyTimes.size(); ++k) {
                float kTime = boneKeyTimes[k];
                int kFrame = static_cast<int>(std::round(kTime * animEditorFps_));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                char kBuf[64];
                snprintf(kBuf, sizeof(kBuf), "Key %zu: %d F (%.3fs)##KeyRow%zu", k, kFrame, kTime, k);
                bool isCurTime = (std::abs(animEditorTime_ - kTime) < 0.005f);

                if (ImGui::Selectable(kBuf, isCurTime)) {
                    animEditorTime_ = kTime;
                    animEditorSelectedKeyIndex_ = -1;
                    for (size_t ri = 0; ri < nodeAnim.rotate.size(); ++ri) {
                        if (std::abs(nodeAnim.rotate[ri].time - kTime) < 0.005f) {
                            animEditorSelectedKeyIndex_ = static_cast<int>(ri);
                            break;
                        }
                    }
                    UpdateAnimationPosePreview(sceneManager);
                }

                ImGui::TableSetColumnIndex(1);
                char delBuf[32];
                snprintf(delBuf, sizeof(delBuf), "削除##KBtn%zu", k);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
                if (ImGui::SmallButton(delBuf)) {
                    toDeleteIndex = static_cast<int>(k);
                    toDeleteTime = kTime;
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::EndTable();

            if (toDeleteIndex >= 0) {
                PushAnimUndoState("キーフレーム削除");

                nodeAnim.rotate.erase(
                    std::remove_if(nodeAnim.rotate.begin(), nodeAnim.rotate.end(),
                        [toDeleteTime](const KeyframeQuaternion& kf) { return std::abs(kf.time - toDeleteTime) < 0.005f; }),
                    nodeAnim.rotate.end()
                );

                nodeAnim.translate.erase(
                    std::remove_if(nodeAnim.translate.begin(), nodeAnim.translate.end(),
                        [toDeleteTime](const KeyframeVector3& kf) { return std::abs(kf.time - toDeleteTime) < 0.005f; }),
                    nodeAnim.translate.end()
                );

                nodeAnim.scale.erase(
                    std::remove_if(nodeAnim.scale.begin(), nodeAnim.scale.end(),
                        [toDeleteTime](const KeyframeVector3& kf) { return std::abs(kf.time - toDeleteTime) < 0.005f; }),
                    nodeAnim.scale.end()
                );

                animEditorSelectedKeyIndex_ = -1;
                UpdateAnimationPosePreview(sceneManager);
            }
        }
    }

    // ショートカットキー判定 (Ctrl+Z: 元に戻す, Ctrl+Y: やり直す, T/R/S: ギズモ切替, L: ロック切替)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (io.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io.KeyShift) PerformAnimRedo(sceneManager);
                    else PerformAnimUndo(sceneManager);
                } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    PerformAnimRedo(sceneManager);
                }
            } else {
                if (ImGui::IsKeyPressed(ImGuiKey_T, false)) animGizmoMode_ = 0; // Translate (移動)
                if (ImGui::IsKeyPressed(ImGuiKey_R, false)) animGizmoMode_ = 1; // Rotate (回転)
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) animGizmoMode_ = 2; // Scale (拡縮)
                if (ImGui::IsKeyPressed(ImGuiKey_L, false)) isAnimLocked_ = !isAnimLocked_; // Lock (ロック)
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) isAnimHudMinimized_ = !isAnimHudMinimized_; // HUD Minimize toggle
            }
        }
    }

    // 選択オブジェクト固有のインスペクター（Transform / Material 等）も下部に表示
    if (selectedGameObject_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("オブジェクト コンポーネント", ImGuiTreeNodeFlags_DefaultOpen)) {
            selectedGameObject_->DisplayImGui();
        }
    } else if (selectedPrimitive_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        selectedPrimitive_->DisplayImGui("プリミティブ プロパティ");
    } else if (selectedObject_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        selectedObject_->DisplayImGui("3Dオブジェクト プロパティ");
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