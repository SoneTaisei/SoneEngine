#ifdef USE_IMGUI
#include "EditorManager.h"
#include "EditorMenuBar.h"
#include "EditorHierarchy.h"
#include "EditorInspector.h"
#include "EditorReplayUI.h"
#include "EditorLayoutManager.h"
#include "EditorConfigManager.h"
#include "EditorCommandManager.h"

#include "Input/KeyboardInput.h"
#include "Core/Utility/LogManager.h"
#include "Core/TimeManager.h"
#include "Core/Utility/ParameterManager.h"
#include "Editor/Animation/AnimationEditor.h"
#include "Editor/Common/ModelSelectModal.h"
#include "Editor/GPUParticle/GPUParticleEditor.h"
#include "Editor/LightEditor/LightEditor.h"
#include "Editor/MapEditor/MapEditor.h"
#include "Editor/MapEditor/MapEditorContext.h"
#include "Editor/Model3DEditor/Model3DEditor.h"
#include "Editor/Model3DEditor/Model3DEditorContext.h"
#include "Editor/PostEffectEditor/PostEffectEditor.h"
#include "Editor/SpriteAnimationEditor/SpriteAnimationEditor.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include "Editor/Replay/ReplayManager.h"
#include "GameObject/WorldGif.h"
#include "Graphics/Camera/DebugCamera.h"
#include "Graphics/Camera/GameCamera.h"
#include "Core/Utility/TransformFunctions.h"
#include "Resource/Model/ModelCommon.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/SrvManager.h"
#include "Editor/Animation/AnimationPreviewScene.h"
#include "Scene/SceneFactory.h"
#include "Editor/GPUParticle/GPUParticlePreviewScene.h"
#include "Scene/SceneManager.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>

static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    (void)info;
    SrvManager::GetInstance()->Allocate(out_cpu_handle, out_gpu_handle);
}
static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    (void)info;
    SrvManager::GetInstance()->Free(cpu_handle, gpu_handle);
}

bool EditorManager::isPlaying_ = false;
bool EditorManager::isPaused_ = false;
bool EditorManager::showObjects_ = true;
bool EditorManager::showEffects_ = true;
EditorManager* EditorManager::s_Instance = nullptr;
ImVec2 EditorManager::gameViewPos_ = ImVec2(0.0f, 0.0f);
ImVec2 EditorManager::gameViewSize_ = ImVec2(1280.0f, 720.0f);

EditorManager::EditorManager() { s_Instance = this; }
EditorManager::~EditorManager() { if (s_Instance == this) s_Instance = nullptr; }

void EditorManager::Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue) {
    (void)commandQueue;
    s_Instance = this;
    layoutManager_ = std::make_unique<EditorLayoutManager>();
    configManager_ = std::make_unique<EditorConfigManager>();
    commandManager_ = std::make_unique<EditorCommandManager>();
    hierarchyUI_ = std::make_unique<EditorHierarchy>();
    inspectorUI_ = std::make_unique<EditorInspector>();
    menuBarUI_ = std::make_unique<EditorMenuBar>();
    replayUI_ = std::make_unique<EditorReplayUI>();
    animationEditor_ = std::make_unique<AnimationEditor>();
    gpuParticleEditor_ = std::make_unique<GPUParticleEditor>();
    gpuParticleEditor_->Initialize(device);
    mapEditor_ = std::make_unique<MapEditor>();
    mapEditor_->Initialize();
    if (strlen(stageFilename_) > 0) mapEditor_->SetStageFilename(stageFilename_);
    model3DEditor_ = std::make_unique<Model3DEditor>();
    model3DEditor_->Initialize(device);
    if (!lightEditor_) {
        ownedLightEditor_ = std::make_unique<LightEditor>();
        lightEditor_ = ownedLightEditor_.get();
        lightEditor_->Initialize(nullptr);
    }
    postEffectEditor_ = std::make_unique<PostEffectEditor>();
    postEffectEditor_->Initialize();
    spriteAnimEditor_ = std::make_unique<SpriteAnimationEditor>();
    spriteAnimEditor_->Initialize();
    animModelSelectModal_ = std::make_unique<ModelSelectModal>();
    animModelSelectModal_->Initialize();
    postEffectEditor_->SetOnSelectCallback([this]() {
        selectedGameObject_ = nullptr; selectedObject_ = nullptr; selectedParticle_ = nullptr; selectedPrimitive_ = nullptr;
        if (model3DEditor_) model3DEditor_->SetSelectedObject(nullptr);
        showPostEffect_ = true;
    });
    postEffectEditor_->SetOnFileChangedCallback([this]() { SaveSceneConfig(); });
    ScanLayoutPresets();

    layoutManager_->ApplyStyle();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    // 日本語基本文字に加え、矢印記号や幾何学図形（■, ▲, ▼, ◆等）のグリフ範囲もマージして文字化けを防止
    static const ImWchar extra_ranges[] = {
        0x2000, 0x206F, // 一般句読点
        0x2190, 0x21FF, // 矢印記号 (↑, ↓, ←, → 等)
        0x2200, 0x22FF, // 数学記号 (±, ×, ÷, ∞ 等)
        0x25A0, 0x25FF, // 幾何学図形 (■, □, ▲, △, ▼, ▽, ◆, ◇ 等)
        0x2600, 0x26FF, // その他の記号 (★, ☆ 等)
        0,
    };
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    builder.AddRanges(extra_ranges);
    static ImVector<ImWchar> glyph_ranges;
    glyph_ranges.clear();
    builder.BuildRanges(&glyph_ranges);

    bool fontLoaded = false;
    const std::vector<std::pair<std::string, float>> fontCandidates = {
        { "resources/Fonts/GenEiNuGothic-EB.ttf", 14.0f },
        { "C:\\Windows\\Fonts\\meiryo.ttc", 18.0f },
        { "C:\\Windows\\Fonts\\msgothic.ttc", 18.0f },
        { "C:\\Windows\\Fonts\\YuGothM.ttc", 18.0f },
    };

    for (const auto& [pathStr, size] : fontCandidates) {
        if (std::filesystem::exists(pathStr)) {
            io.Fonts->AddFontFromFileTTF(pathStr.c_str(), size, nullptr, glyph_ranges.Data);
            fontLoaded = true;
            break;
        }
    }

    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
    }

    ImGui_ImplWin32_Init(hwnd);
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
}

void EditorManager::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EditorManager::PopPreviewScenes(SceneManager* sceneManager) {
    if (!sceneManager) return;
    if (animationEditor_ && animationEditor_->IsAnimScenePushed()) {
        sceneManager->PopScene();
        animationEditor_->SetAnimScenePushed(false);
    }
    if (gpuParticleEditor_ && gpuParticleEditor_->IsParticleScenePushed()) {
        sceneManager->PopScene();
        gpuParticleEditor_->SetParticleScenePushed(false);
    }
}

void EditorManager::DrawGameViewportTexture(D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle) {
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    float aspect = 1280.0f / 720.0f;
    float windowAspect = contentSize.x / contentSize.y;
    ImVec2 imageSize = (windowAspect > aspect) ? ImVec2(contentSize.y * aspect, contentSize.y) : ImVec2(contentSize.x, contentSize.x / aspect);
    ImVec2 currentPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(currentPos.x + (contentSize.x - imageSize.x) * 0.5f, currentPos.y + (contentSize.y - imageSize.y) * 0.5f));
    gameViewPos_ = ImGui::GetCursorScreenPos();
    gameViewSize_ = imageSize;
    ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);
}

void EditorManager::UpdateUI(ModelCommon *modelCommon, GameCamera *gameCamera, DebugCamera *debugCamera,
                             Camera **activeCamera, bool &isDebugCameraActive,
                             D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, SceneManager *sceneManager) {
    (void)isDebugCameraActive;
    currentDebugCamera_ = debugCamera;

    // テイクオーバーカウントダウン処理
    if (takeoverCountdown_ > 0.0f) {
        takeoverCountdown_ -= TimeManager::GetInstance().GetDeltaTime();
        if (takeoverCountdown_ <= 0.0f) {
            isPlaying_ = true;
        } else {
            auto dxCommon = DirectXCommon::GetInstance();
            ImGui::SetNextWindowPos(ImVec2(dxCommon->GetWindowWidth() * 0.5f, dxCommon->GetWindowHeight() / 3.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Takeover Countdown", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
            ImGui::SetWindowFontScale(6.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "READY... %.1f", takeoverCountdown_);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }
    }

    const char* curStageName = (mapEditor_ && strlen(mapEditor_->GetStageFilename()) > 0) ? mapEditor_->GetStageFilename() : stageFilename_;
    ReplayManager::GetInstance()->SetCurrentStageFilename(curStageName);

    if (configManager_) {
        configManager_->InitializeMapOnStart(this, sceneManager, gameCamera, debugCamera);
        configManager_->RestoreMapOnReset(this, sceneManager, gameCamera, debugCamera);
    }

    static bool wasReplayingLastFrame = false;
    bool isReplayingNow = ReplayManager::GetInstance()->IsPlaying();
    if (wasReplayingLastFrame && !isReplayingNow && takeoverCountdown_ <= 0.0f) {
        useDebugCamera_ = true;
        if (gameCamera && debugCamera) {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
        }
    }
    wasReplayingLastFrame = isReplayingNow;

    if (mapEditor_) mapEditor_->SetVisible(false);

    if (menuBarUI_) {
        menuBarUI_->Draw(this, sceneManager, modelCommon, gameCamera, debugCamera);
        menuBarUI_->SetupDockspace(this);
    }

    // ゲームビュー
    if (showGameView_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "ゲームビュー") ImGui::SetNextWindowFocus();
        if (ImGui::Begin("ゲームビュー", &showGameView_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && activeMainTab_ != "ゲームビュー") {
                activeMainTab_ = "ゲームビュー"; currentMode_ = EditorMode::Normal; SaveSceneConfig();
            }
            if (activeMainTab_ == "ゲームビュー") {
                currentMode_ = EditorMode::Normal; selectedReplaySeekbar_ = false; PopPreviewScenes(sceneManager);
            }
            isGameViewHovered_ = ImGui::IsWindowHovered();
            DrawGameViewportTexture(renderTextureSrvHandle);
        }
        ImGui::End();
    }

    // リプレイエディター
    isReplayEditorHovered_ = false;
    if (showReplayEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "リプレイエディター") ImGui::SetNextWindowFocus();
        if (ImGui::Begin("リプレイエディター", &showReplayEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && activeMainTab_ != "リプレイエディター") {
                activeMainTab_ = "リプレイエディター"; currentMode_ = EditorMode::Replay; SaveSceneConfig();
            }
            if (activeMainTab_ == "リプレイエディター") { currentMode_ = EditorMode::Replay; PopPreviewScenes(sceneManager); }
            isReplayEditorHovered_ = ImGui::IsWindowHovered();
            DrawGameViewportTexture(renderTextureSrvHandle);
            IScene* activeScene = sceneManager->GetCurrentScene();
            if (activeScene && activeCamera && *activeCamera) {
                Matrix4x4 viewProj = TransformFunctions::Multiply((*activeCamera)->GetViewMatrix(), (*activeCamera)->GetProjectionMatrix());
                activeScene->DrawEditorOverlay(viewProj);
            }
        }
        ImGui::End();
    }

    // アニメーションエディター
    if (animationEditor_) animationEditor_->SetHovered(false);
    if (showAnimEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "アニメーションエディター") ImGui::SetNextWindowFocus();
        if (ImGui::Begin("アニメーションエディター", &showAnimEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && activeMainTab_ != "アニメーションエディター") {
                activeMainTab_ = "アニメーションエディター"; currentMode_ = EditorMode::Animation; SaveSceneConfig();
            }
            if (activeMainTab_ == "アニメーションエディター") {
                currentMode_ = EditorMode::Animation;
                if (gpuParticleEditor_ && gpuParticleEditor_->IsParticleScenePushed()) { sceneManager->PopScene(); gpuParticleEditor_->SetParticleScenePushed(false); }
                if (animationEditor_ && !animationEditor_->IsAnimScenePushed()) {
                    sceneManager->PushScene(std::make_unique<AnimationPreviewScene>());
                    animationEditor_->SetAnimScenePushed(true);
                    selectedObject_ = nullptr; selectedPrimitive_ = nullptr;
                    auto* pushedScene = dynamic_cast<AnimationPreviewScene*>(sceneManager->GetCurrentScene());
                    if (pushedScene && !pushedScene->GetGameObjects().empty()) {
                        selectedGameObject_ = pushedScene->GetGameObjects()[0]; pushedScene->SetSelectedGameObject(selectedGameObject_);
                    }
                    animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_);
                    animationEditor_->RefreshAnimationJointList(sceneManager);
                }
            }
            if (animationEditor_) {
                animationEditor_->SetHovered(ImGui::IsWindowHovered());
                auto* animScene = dynamic_cast<AnimationPreviewScene*>(sceneManager->GetCurrentScene());
                if (animScene) {
                    const auto& animObjs = animScene->GetGameObjects();
                    bool isValidSelection = false;
                    for (const auto& o : animObjs) { if (o && o == selectedGameObject_) { isValidSelection = true; break; } }
                    if (!isValidSelection && !animObjs.empty()) selectedGameObject_ = animObjs[0];
                    animScene->SetSelectedGameObject(selectedGameObject_);
                }
                animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_);
                animationEditor_->DrawMainView(sceneManager, activeCamera, renderTextureSrvHandle);
            }
        }
        ImGui::End();
    }

    // GPUパーティクルエディター
    if (gpuParticleEditor_) gpuParticleEditor_->SetHovered(false);
    if (showGPUParticleEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "GPUパーティクルエディター") ImGui::SetNextWindowFocus();
        if (ImGui::Begin("GPUパーティクルエディター", &showGPUParticleEditor_)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && activeMainTab_ != "GPUパーティクルエディター") {
                activeMainTab_ = "GPUパーティクルエディター"; currentMode_ = EditorMode::GPUParticle; SaveSceneConfig();
            }
            if (activeMainTab_ == "GPUパーティクルエディター") {
                currentMode_ = EditorMode::GPUParticle;
                if (animationEditor_ && animationEditor_->IsAnimScenePushed()) { sceneManager->PopScene(); animationEditor_->SetAnimScenePushed(false); }
                if (gpuParticleEditor_ && !gpuParticleEditor_->IsParticleScenePushed()) {
                    auto previewScene = std::make_unique<GPUParticlePreviewScene>();
                    previewScene->SetParticleSystem(gpuParticleEditor_->GetContext()->GetSystem());
                    sceneManager->PushScene(std::move(previewScene));
                    gpuParticleEditor_->SetParticleScenePushed(true);
                    if (activeCamera && *activeCamera) {
                        (*activeCamera)->SetTranslation({ 0.0f, 0.0f, -10.0f }); (*activeCamera)->SetRotation({ 0.0f, 0.0f, 0.0f }); (*activeCamera)->UpdateMatrix();
                    }
                }
            }
            if (gpuParticleEditor_) {
                gpuParticleEditor_->SetHovered(ImGui::IsWindowHovered());
                gpuParticleEditor_->DrawMainView(sceneManager, activeCamera, renderTextureSrvHandle);
            }
        }
        ImGui::End();
    }

    if (postEffectEditor_) postEffectEditor_->Update(TimeManager::GetInstance().GetDeltaTime());
    if (spriteAnimEditor_) spriteAnimEditor_->Update(TimeManager::GetInstance().GetDeltaTime());

    // ライトエディター
    if (showLightEditor_ && lightEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "ライトエディター") ImGui::SetNextWindowFocus();
        if (ImGui::Begin("ライトエディター", &showLightEditor_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || ImGui::IsWindowAppearing() || (focusActiveTabCountdown_ > 0 && activeMainTab_ == "ライトエディター")) {
                if (activeMainTab_ != "ライトエディター") { activeMainTab_ = "ライトエディター"; focusSpotLightTabCountdown_ = 5; SaveSceneConfig(); }
                currentMode_ = EditorMode::Light; PopPreviewScenes(sceneManager);
            }
            if (activeMainTab_ == "ライトエディター") currentMode_ = EditorMode::Light;
            Matrix4x4 vpMatrix = (activeCamera && *activeCamera) ? 
                TransformFunctions::Multiply((*activeCamera)->GetViewMatrix(), (*activeCamera)->GetProjectionMatrix()) : TransformFunctions::MakeIdentity4x4();
            lightEditor_->DrawViewportContent(renderTextureSrvHandle, &vpMatrix);
        }
        ImGui::End();
    }

    // 3Dモデル配置エディター
    if (model3DEditor_) {
        model3DEditor_->Update();
        if (showModelPlacementEditor_) {
            if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "3Dモデル配置") ImGui::SetNextWindowFocus();
            model3DEditor_->DrawMainViewport(showModelPlacementEditor_, sceneManager, activeCamera, renderTextureSrvHandle, [&]() {
                currentMode_ = EditorMode::ModelPlacement; PopPreviewScenes(sceneManager);
                if (focusActiveTabCountdown_ == 0 && activeMainTab_ != "3Dモデル配置") { activeMainTab_ = "3Dモデル配置"; SaveSceneConfig(); }
            });
        }
    }

    // ヒエラルキー / マイメディア
    if (currentMode_ == EditorMode::Replay) {
        if (replayUI_) replayUI_->DrawHierarchyWindow(&showHierarchy_, this);
    } else {
        if (hierarchyUI_) hierarchyUI_->Draw(&showHierarchy_, this, sceneManager);
    }

    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) useDebugCamera_ = !useDebugCamera_;

    if (inspectorUI_) inspectorUI_->Draw(&showInspector_, this, sceneManager, modelCommon, gameCamera, debugCamera);

    // ポストエフェクト
    if (showPostEffect_) {
        if (postEffectEditor_ && postEffectEditor_->IsShouldFocusWindow()) { ImGui::SetNextWindowFocus(); postEffectEditor_->SetShouldFocusWindow(false); }
        if (ImGui::Begin("ポストエフェクト", &showPostEffect_)) {
            if (postEffectEditor_) postEffectEditor_->DrawUI(&showPostEffect_);
        }
        ImGui::End();
    }

    // スプライトアニメーション
    if (showSpriteAnimation_ && spriteAnimEditor_) spriteAnimEditor_->DrawUI(&showSpriteAnimation_, gameViewPos_, gameViewSize_);

    // パラメータマネージャー
    if (showParameterManager_) {
        if (ImGui::Begin("パラメータ調整 (ParameterManager)", &showParameterManager_)) ParameterManager::GetInstance()->DisplayImGui();
        ImGui::End();
    }

    // マップエディター
    if (showMapEditor_ && mapEditor_) {
        if (focusActiveTabCountdown_ > 0 && activeMainTab_ == "マップチップ画面") ImGui::SetNextWindowFocus();
        mapEditor_->SetVisible(showMapEditor_);
        mapEditor_->DrawCanvas(sceneManager, activeCamera, renderTextureSrvHandle, [&]() {
            currentMode_ = EditorMode::Normal; PopPreviewScenes(sceneManager);
            if (focusActiveTabCountdown_ == 0 && activeMainTab_ != "マップチップ画面") { activeMainTab_ = "マップチップ画面"; SaveSceneConfig(); }
        });
        showMapEditor_ = mapEditor_->IsVisible();
    }

    // 下部ペイン
    if (currentMode_ == EditorMode::Light || activeMainTab_ == "ライトエディター") {
        if (lightEditor_) {
            if (focusSpotLightTabCountdown_ > 0) { ImGui::SetNextWindowFocus(); focusSpotLightTabCountdown_--; }
            if (ImGui::Begin("スポットライト", &showSpotLightPanel_)) lightEditor_->DrawLightEditorUI(modelCommon);
            ImGui::End();
        }
    } else if (currentMode_ == EditorMode::Animation) {
        if (animationEditor_) {
            animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_);
            animationEditor_->DrawDopeSheetUI(sceneManager);
        }
    } else if (currentMode_ == EditorMode::GPUParticle || activeMainTab_ == "GPUパーティクルエディター") {
        if (gpuParticleEditor_) gpuParticleEditor_->DrawTimelineUI(sceneManager);
    } else if (currentMode_ == EditorMode::Replay) {
        if (replayUI_) replayUI_->DrawTimelineWindow(&showMapSettings_, this, commandManager_.get());
    } else if (currentMode_ == EditorMode::ModelPlacement || activeMainTab_ == "3Dモデル配置") {
        if (model3DEditor_) model3DEditor_->DrawPalette(showModelPalette_, sceneManager);
    } else {
        if (showMapSettings_ && mapEditor_) {
            mapEditor_->DrawSettingsUI(sceneManager, showMapSettings_, [&](){ SaveSceneConfig(); }, [&](){
                selectedGameObject_ = nullptr; selectedObject_ = nullptr; selectedParticle_ = nullptr; selectedPrimitive_ = nullptr;
            });
        }
    }

    if (animationEditor_) {
        animationEditor_->SetSelectedTargets(selectedObject_, selectedGameObject_, selectedPrimitive_);
        animationEditor_->UpdateAnimationPosePreview(sceneManager);
    }
    if (gpuParticleEditor_) gpuParticleEditor_->Update(TimeManager::GetInstance().GetDeltaTime());

    LogManager::GetInstance()->Draw();

    if (layoutManager_) layoutManager_->DrawSavePresetModal(&showSavePresetWindow_);
    if (commandManager_) commandManager_->UpdateShortcuts(this, sceneManager);

    if (focusActiveTabCountdown_ > 0) {
        if (!activeMainTab_.empty()) ImGui::SetWindowFocus(activeMainTab_.c_str());
        focusActiveTabCountdown_--;
    }
}

void EditorManager::Draw() {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectXCommon::GetInstance()->GetCommandList());
}

void EditorManager::Draw3D() {
    if (model3DEditor_) {
        bool isPlaying = isPlaying_ || (ReplayManager::GetInstance() && ReplayManager::GetInstance()->IsPlaying());
        model3DEditor_->Draw(showModelPlacementEditor_ && (activeMainTab_ == "3Dモデル配置") && !isPlaying);
    }
}

bool EditorManager::IsAnimationEditorActive() const {
    return (currentMode_ == EditorMode::Animation) || (activeMainTab_ == "アニメーションエディター") ||
           (animationEditor_ && animationEditor_->IsAnimScenePushed());
}

bool EditorManager::IsGPUParticleEditorActive() const {
    return (currentMode_ == EditorMode::GPUParticle) || (activeMainTab_ == "GPUパーティクルエディター") ||
           (gpuParticleEditor_ && gpuParticleEditor_->IsParticleScenePushed());
}

bool EditorManager::IsAnimationOrParticleEditorActive() const {
    return IsAnimationEditorActive() || IsGPUParticleEditorActive();
}

void EditorManager::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorManager::SaveSceneConfig() { if (configManager_) configManager_->SaveSceneConfig(this); }
void EditorManager::LoadSceneConfig() { if (configManager_) configManager_->LoadSceneConfig(this); }
void EditorManager::SaveLightingConfig(ModelCommon* modelCommon) { if (configManager_) configManager_->SaveLightingConfig(modelCommon, this); }
void EditorManager::LoadLightingConfig(ModelCommon* modelCommon) { if (configManager_) configManager_->LoadLightingConfig(modelCommon, this); }
void EditorManager::LoadPlacedModelsForScene(IScene* scene) { if (configManager_) configManager_->LoadPlacedModelsForScene(scene); }
void EditorManager::Undo() { if (commandManager_) commandManager_->Undo(); }
void EditorManager::Redo() { if (commandManager_) commandManager_->Redo(); }
void EditorManager::UpdateAStarPositionsFromMap(MapChip2D* mapChip, SceneManager* sceneManager) { if (mapEditor_) mapEditor_->UpdateAStarPositionsFromMap(mapChip, sceneManager); }
void EditorManager::ApplyDefaultLayout(ImGuiID explicitDockspaceId) { if (layoutManager_) layoutManager_->ApplyDefaultLayout(explicitDockspaceId); }
void EditorManager::ScanLayoutPresets() { if (layoutManager_) layoutManager_->ScanLayoutPresets(); }
void EditorManager::SaveLayoutPreset(const std::string& name) { if (layoutManager_) layoutManager_->SaveLayoutPreset(name); }
bool EditorManager::ApplyLayoutPreset(const std::string& name) { return layoutManager_ ? layoutManager_->ApplyLayoutPreset(name) : false; }
bool EditorManager::DeleteLayoutPreset(const std::string& name) { return layoutManager_ ? layoutManager_->DeleteLayoutPreset(name) : false; }
bool EditorManager::ExportLayoutPresetToFile(const std::string& name, const std::string& filePath) { return layoutManager_ ? layoutManager_->ExportLayoutPresetToFile(name, filePath) : false; }
bool EditorManager::ImportLayoutPresetFromFile(const std::string& filePath) { return layoutManager_ ? layoutManager_->ImportLayoutPresetFromFile(filePath) : false; }

void EditorManager::FocusMapEditor() {
    showMapEditor_ = true; showMapSettings_ = true; activeMainTab_ = "マップチップ画面"; focusActiveTabCountdown_ = 5; SaveSceneConfig();
}

void EditorManager::FocusGameView() {
    showGameView_ = true; activeMainTab_ = "ゲームビュー"; focusActiveTabCountdown_ = 5; SaveSceneConfig();
}

void EditorManager::ToggleMapEditor() {
    if (activeMainTab_ == "マップチップ画面") FocusGameView(); else FocusMapEditor();
}

void EditorManager::SyncPlayMapData(MapChip2D* mapChip) {
    if (!mapChip) return;
    mapDataStrToLoad_ = mapChip->GetMapDataAsString();
    savedRoomsForPlay_ = mapChip->GetRooms();
    mapChip->SaveInitialMapState();
    mapChip->SaveToFile("resources/json/local/temp_play_map.txt");
}

#endif
