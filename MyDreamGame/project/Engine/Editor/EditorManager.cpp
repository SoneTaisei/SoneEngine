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
#include "Graphics/TextureManager.h"
#include "Core/Utility/LogManager.h"
#include "GameObject/MapObject2D.h"
#include "Resource/Primitive/PrimitiveManager.h"

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
            ImGui::DockBuilderDockWindow("マップチップ画面", dock_id_main);
            ImGui::DockBuilderDockWindow("マップ設定", dock_id_bottom);
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
                if (ImGui::CollapsingHeader("GameObjects", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto &obj : activeScene->GetGameObjects()) {
                        bool isSelected = (selectedGameObject_ == obj);
                        if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                            selectedGameObject_ = obj;
                            selectedObject_ = nullptr;
                            selectedParticle_ = nullptr;
                            selectedPrimitive_ = nullptr;
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
                        }
                    }
                }
                if (ImGui::CollapsingHeader("パーティクル (Particles)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto *particle : activeScene->GetParticles()) {
                        bool isSelected = (selectedParticle_ == particle);
                        if (ImGui::Selectable(particle->GetName().c_str(), isSelected)) {
                            selectedGameObject_ = nullptr;
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
                            selectedGameObject_ = nullptr;
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
            bool isMapChipSelected = (mapEditorSelectedTool_ >= 100 || (mapEditorSelectedTool_ >= 1 && mapEditorSelectedTool_ <= 9));
            if (selectedGameObject_ || selectedObject_ || selectedParticle_ || selectedPrimitive_ || isMapChipSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.35f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
                if (ImGui::Button("グローバル設定を表示", ImVec2(-1, 0))) {
                    selectedGameObject_ = nullptr;
                    selectedObject_ = nullptr;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                    mapEditorSelectedTool_ = 0;
                }
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            if (selectedGameObject_) {
                selectedGameObject_->DisplayImGui();
            } else if (selectedObject_) {
                selectedObject_->DisplayImGui("Object Properties");
            } else if (selectedParticle_) {
                selectedParticle_->DrawImGui();
            } else if (selectedPrimitive_) {
                selectedPrimitive_->DisplayImGui("Primitive Properties");
            } else {
                IScene *activeScene = sceneManager->GetCurrentScene();
                bool handled = false;
                if (activeScene) {
                    MapChip2D* mapChip = activeScene->GetMapChip();
                    if (mapChip && (mapEditorSelectedTool_ >= 100 || (mapEditorSelectedTool_ >= 1 && mapEditorSelectedTool_ <= 9))) {
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

                            const char* types[] = { "NormalBlock", "DeathBlock", "GoalBlock", "CoinBlock", "OneWayBlock", "LiftBlock", "RailBlock", "JumpBlock" };
                            int currentType = -1;
                            for (int i = 0; i < 8; ++i) {
                                if (targetDef->type == types[i]) {
                                    currentType = i;
                                    break;
                                }
                            }
                            if (ImGui::Combo("種類 (Type)", &currentType, types, 8)) {
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
                                    mapChip->SaveTemplatesToFile("resources/json/templates_config.json");
                                } else {
                                    std::string name = stageFilename_;
                                    if (name.length() < 4 || name.substr(name.length() - 4) != ".txt") name += ".txt";
                                    mapChip->SaveToFile("resources/json/MapData/" + name);
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
                                bool isSelectableTool = (mapEditorSelectedTool_ == 0 || mapEditorSelectedTool_ == 6 || mapEditorSelectedTool_ == 10 || mapEditorSelectedTool_ >= 100);
                                
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

    // --- マップ設定 ウィンドウ ---
    if (showMapSettings_) {
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
                        if (std::filesystem::exists("resources/json/MapData")) {
                            for (const auto& entry : std::filesystem::directory_iterator("resources/json/MapData")) {
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
                        return std::string("resources/json/MapData/") + name;
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
                        { 9, "Jump",  ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 1.0f }
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
                            
                            if (i < numTools) {
                                const ToolIcon& tool = tools[i];
                                bool isSelected = (mapEditorSelectedTool_ == tool.id);

                                // 当たり判定 (InvisibleButton)
                                ImGui::SetNextItemAllowOverlap();
                                if (ImGui::InvisibleButton("##Tool", ImVec2(itemSize, totalHeight))) {
                                    mapEditorSelectedTool_ = tool.id;
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
                                    mapChip->SaveToFile(GetFullFilePath(stageFilename_));
                                }
                            }

                            ImGui::PopID();

                            // 折り返し処理 (ウィンドウ幅を超える場合は次の行へ)
                            float lastButtonX2 = ImGui::GetItemRectMax().x;
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
                    const auto& history = replayMgr->GetHistory();
                    
                    // TreeNodeEx で階層化（デフォルト開く）
                    if (ImGui::TreeNodeEx("一時履歴データ (未保存)", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (history.empty()) {
                            ImGui::Text("履歴データはありません。");
                        } else {
                            // 再帰的にツリーを描画するラムダ式
                            std::function<void(int)> DrawHistoryNode = [&](int parentId) {
                                for (size_t i = 0; i < history.size(); ++i) {
                                    bool isRoot = false;
                                    if (parentId == -1) {
                                        isRoot = (history[i].parentId == -1);
                                        if (!isRoot) {
                                            bool parentExists = false;
                                            for (const auto& h : history) {
                                                if (h.id == history[i].parentId) {
                                                    parentExists = true; break;
                                                }
                                            }
                                            if (!parentExists) isRoot = true; // 親が消えている場合はルート扱い
                                        }
                                    }

                                    if ((parentId == -1 && isRoot) || (parentId != -1 && history[i].parentId == parentId)) {
                                        ImGui::PushID(static_cast<int>(i));

                                        bool hasChild = false;
                                        for (const auto& h : history) {
                                            if (h.parentId == history[i].id) { hasChild = true; break; }
                                        }

                                        bool nodeOpen = true;
                                        if (isRoot) {
                                            ImGui::BulletText("履歴 #%d (%s) - %d F", history[i].id, history[i].dateStr.c_str(), history[i].totalFrames);
                                        } else {
                                            nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)history[i].id, ImGuiTreeNodeFlags_DefaultOpen, "履歴 #%d (%s) - %d F", history[i].id, history[i].dateStr.c_str(), history[i].totalFrames);
                                        }

                                        if (nodeOpen) {
                                            // インデントしてボタン類を描画（ルートの場合は自動インデントがないので手動）
                                            if (isRoot) ImGui::Indent();
                                            
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
                                                    sceneJustReset_ = true;
                                                }
                                            }

                                            ImGui::SameLine();
                                            if (ImGui::Button("選択(残像表示)")) {
                                                replayMgr->SelectReplay(static_cast<int>(i));
                                            }

                                            ImGui::SameLine();
                                            static char fileNameBuf[10][64] = {};
                                            if (fileNameBuf[i][0] == '\0') {
                                                sprintf_s(fileNameBuf[i], "replay_history_%d", history[i].id);
                                            }
                                            ImGui::SetNextItemWidth(150.0f);
                                            ImGui::InputText("##Name", fileNameBuf[i], IM_ARRAYSIZE(fileNameBuf[i]));

                                            ImGui::SameLine();
                                            if (ImGui::Button("★ 永久保存")) {
                                                std::string fname = fileNameBuf[i];
                                                if (fname.find(".mml") == std::string::npos) fname += ".mml";
                                                replayMgr->SaveToFile(history[i], fname);
                                            }
                                            
                                            if (isRoot) ImGui::Unindent();

                                            // 子要素の描画
                                            bool hasChild = false;
                                            for (const auto& h : history) {
                                                if (h.parentId == history[i].id) { hasChild = true; break; }
                                            }

                                            if (hasChild) {
                                                if (isRoot) ImGui::Indent();
                                                DrawHistoryNode(history[i].id);
                                                if (isRoot) ImGui::Unindent();
                                            }
                                            
                                            if (!isRoot) {
                                                ImGui::TreePop();
                                            }
                                        }
                                        
                                        ImGui::PopID();
                                        if (isRoot) ImGui::Spacing();
                                    }
                                }
                            };

                            // ルート要素（parentId == -1 に相当するもの）の描画を開始
                            DrawHistoryNode(-1);
                        }
                        ImGui::TreePop();
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    const auto& saved = replayMgr->GetSavedList();
                    if (saved.empty()) {
                        ImGui::Text("保存済みのリプレイはありません。");
                    } else {
                        for (size_t i = 0; i < saved.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i + 100));
                            ImGui::Text("📁 %s", saved[i].c_str());
                            
                            if (ImGui::Button("ロード再生")) {
                                replayMgr->StartPlayback(-1, "resources/json/saved_replays/" + saved[i]);
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
                                replayMgr->SelectReplay(-1, "resources/json/saved_replays/" + saved[i]);
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
                    static char saveNameBuf[64] = "edited_replay.mml";
                    static int rangeStart = -1;
                    static int rangeEnd = -1;
                    static bool isSelecting = false;
                    
                    auto& activeReplay = replayMgr->GetCurrentReplay();
                    if (activeReplay.totalFrames == 0) {
                        ImGui::Text("編集対象のリプレイデータがロードされていません。");
                        ImGui::Text("履歴から再生するか、ファイルをロード再生してください。");
                    } else {
                        ImGui::Text("リプレイ編集 (タイムライン / TAS)");
                        ImGui::Text("総フレーム: %d | 録画日時: %s", activeReplay.totalFrames, activeReplay.dateStr.c_str());
                        ImGui::Spacing();

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

                        // (3) 範囲選択ロジック用の状態


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

                    } // if (activeReplay.totalFrames > 0) をここで閉じる

                    ImGui::Spacing();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
                    ImGui::Spacing();
                        
                        if (ImGui::CollapsingHeader("📋 入力マクロ (ワンクリック配置)", ImGuiTreeNodeFlags_DefaultOpen)) {
                            auto& macros = replayMgr->GetMacros();
                            static int selectedMacroIdx = 0;
                            
                            // マクロ一覧
                            if (!macros.empty()) {
                                if (selectedMacroIdx >= macros.size()) selectedMacroIdx = 0;
                                
                                std::string previewName = macros[selectedMacroIdx].name;
                                if (ImGui::BeginCombo("マクロ一覧", previewName.c_str())) {
                                    for (int i = 0; i < macros.size(); ++i) {
                                        bool is_selected = (selectedMacroIdx == i);
                                        if (ImGui::Selectable(macros[i].name.c_str(), is_selected)) {
                                            selectedMacroIdx = i;
                                        }
                                        if (is_selected) ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }
                                
                                ImGui::SameLine();
                                if (ImGui::Button("削除")) {
                                    replayMgr->RemoveMacro(selectedMacroIdx);
                                    if (selectedMacroIdx > 0) selectedMacroIdx--;
                                }
                            } else {
                                ImGui::TextDisabled("登録されたマクロがありません。");
                            }
                            
                            // 新規作成UI
                            static char newMacroName[64] = "NewMacro";
                            ImGui::InputText("新規マクロ名", newMacroName, IM_ARRAYSIZE(newMacroName));
                            ImGui::SameLine();
                            if (ImGui::Button("空から新規作成")) {
                                ReplayMacro rm;
                                rm.name = newMacroName;
                                rm.blocks.push_back({10, "-------"}); // デフォルトで1ブロック追加
                                replayMgr->AddMacro(rm);
                                selectedMacroIdx = (int)macros.size() - 1;
                            }
                            
                            // マクロ録画（実際のプレイを記録する）UI
                            ImGui::SameLine();
                            if (replayMgr->IsRecordingMacro()) {
                                ImGui::TextColored(ImVec4(1, 0, 0, 1), "🔴 マクロ録画待機中...");
                                ImGui::SameLine();
                                if (ImGui::Button("キャンセル")) {
                                    replayMgr->CancelMacroRecording();
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("⏹ 録画を終了して保存")) {
                                    // 録画を強制停止（停止時に自動的にマクロに抽出・保存される）
                                    replayMgr->StopRecord();
                                }
                            } else {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                                if (ImGui::Button("🔴 実際のプレイをマクロとして録画開始")) {
                                    replayMgr->ReserveMacroRecording(newMacroName);
                                    
                                    // 録画のためにエディタのカメラとポーズを解除し、ゲームをリセットして操作可能にする
                                    isPlaying_ = true; // ← ここを追加（ゲームを開始させる）
                                    useDebugCamera_ = false;
                                    sceneJustReset_ = true;
                                }
                                ImGui::PopStyleColor(3);
                            }
                            
                            if (replayMgr->IsRecordingMacro()) {
                                ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "※ゲームをプレイして録画を完了してください。\n「⏹ 録画を終了して保存」か、ゲーム中のRキーでマクロとして保存されます。");
                            }
                            
                            // 選択範囲から作成するUI (リプレイがある時のみ)
                            if (activeReplay.totalFrames > 0 && rangeStart != -1 && rangeEnd != -1) {
                                int r0 = (std::min)(rangeStart, rangeEnd);
                                int r1 = (std::max)(rangeStart, rangeEnd);
                                
                                ImGui::SameLine();
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                                if (ImGui::Button("選択範囲から抽出して作成")) {
                                    ReplayMacro rm;
                                    rm.name = std::string(newMacroName);
                                    
                                    if (r1 < activeReplay.totalFrames) {
                                        MacroBlock currentBlock;
                                        bool isFirst = true;
                                        
                                        for (int i = r0; i <= r1; ++i) {
                                            char currentKeys[8];
                                            for(int k=0; k<7; ++k) currentKeys[k] = activeReplay.frames[i].keys[k];
                                            currentKeys[7] = '\0';
                                            
                                            if (isFirst) {
                                                currentBlock.duration = 1;
                                                strncpy_s(currentBlock.keys, currentKeys, sizeof(currentBlock.keys));
                                                isFirst = false;
                                            } else {
                                                // キーが同じならdurationを増やす
                                                bool same = true;
                                                for(int k=0; k<7; ++k) {
                                                    if(currentBlock.keys[k] != currentKeys[k]) { same = false; break; }
                                                }
                                                if (same) {
                                                    currentBlock.duration++;
                                                } else {
                                                    // 変わったら今のブロックを保存して新しいブロックへ
                                                    rm.blocks.push_back(currentBlock);
                                                    currentBlock.duration = 1;
                                                    strncpy_s(currentBlock.keys, currentKeys, sizeof(currentBlock.keys));
                                                }
                                            }
                                        }
                                        if (!isFirst) {
                                            rm.blocks.push_back(currentBlock);
                                        }
                                    }
                                    
                                    // 万が一抽出に失敗して空になった場合はダミーを入れる
                                    if (rm.blocks.empty()) rm.blocks.push_back({10, "-------"});
                                    
                                    replayMgr->AddMacro(rm);
                                    selectedMacroIdx = (int)macros.size() - 1;
                                }
                                ImGui::PopStyleColor();
                            }
                            
                            // 選択中マクロの編集と配置
                            if (!macros.empty() && selectedMacroIdx < macros.size()) {
                                ImGui::Separator();
                                ImGui::Text("【%s】のブロック構成", macros[selectedMacroIdx].name.c_str());
                                
                                auto& curMacro = macros[selectedMacroIdx];
                                
                                for (int i = 0; i < curMacro.blocks.size(); ++i) {
                                    auto& b = curMacro.blocks[i];
                                    ImGui::PushID(i);
                                    
                                    ImGui::SetNextItemWidth(100.0f);
                                    if (ImGui::InputInt("F (フレーム)", &b.duration)) {
                                        if (b.duration < 1) b.duration = 1;
                                        replayMgr->SaveMacros();
                                    }
                                    
                                    ImGui::SameLine();
                                    ImGui::Text(" キー:");
                                    ImGui::SameLine();
                                    
                                    // 7つのキーON/OFFトグル
                                    const char* keyNames[7] = {"L", "R", "J", "D", "C", "W", "S"};
                                    const char keyChars[7] = {'L', 'R', 'J', 'D', 'C', 'W', 'S'};
                                    
                                    for (int k = 0; k < 7; ++k) {
                                        bool isOn = (b.keys[k] != '-');
                                        if (ImGui::Checkbox(keyNames[k], &isOn)) {
                                            b.keys[k] = isOn ? keyChars[k] : '-';
                                            replayMgr->SaveMacros(); // 即時保存
                                        }
                                        if (k < 6) ImGui::SameLine();
                                    }
                                    
                                    ImGui::SameLine();
                                    if (ImGui::Button("X")) {
                                        curMacro.blocks.erase(curMacro.blocks.begin() + i);
                                        replayMgr->SaveMacros();
                                        ImGui::PopID();
                                        break; // ループを抜けて再描画
                                    }
                                    
                                    ImGui::PopID();
                                }
                                
                                if (ImGui::Button("+ ブロック追加")) {
                                    curMacro.blocks.push_back({10, "-------"});
                                    replayMgr->SaveMacros();
                                }
                                
                                ImGui::Spacing();
                                ImGui::Separator();
                                
                                // 配置ボタン (リプレイがある時のみ)
                                if (activeReplay.totalFrames > 0) {
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.7f, 1.0f));
                                    
                                    // 配置先は、選択範囲の始点、または現在のフレーム
                                    int placeStart = (rangeStart != -1) ? (std::min)(rangeStart, rangeEnd) : replayMgr->GetCurrentFrame();
                                    
                                    if (ImGui::Button("⇒ タイムラインに配置 (流し込む)")) {
                                        replayMgr->ApplyMacro(placeStart, curMacro);
                                        
                                        // 適用後、現在のアクティブリプレイを保存する
                                        if (!activeReplay.filename.empty()) {
                                            replayMgr->SaveToFile(activeReplay, activeReplay.filename);
                                        } else {
                                            replayMgr->SaveToFile(activeReplay, saveNameBuf);
                                        }
                                    }
                                    ImGui::PopStyleColor(3);
                                    ImGui::SameLine();
                                    ImGui::Text("配置開始フレーム: F%04d", placeStart);
                                } else {
                                    ImGui::TextDisabled("※タイムラインに配置するには、履歴からリプレイを選択してください。");
                                }
                            }
                        }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
    
    LogManager::GetInstance()->Draw();
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

    std::ofstream ofs("resources/json/editor_config.json");
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
    std::ifstream ifs("resources/json/editor_config.json");
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
    std::filesystem::create_directories("resources/json");
    std::ofstream ofs("resources/json/lighting_config.json");
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
    std::ifstream ifs("resources/json/lighting_config.json");
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
#endif