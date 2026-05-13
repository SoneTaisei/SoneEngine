#ifdef USE_IMGUI
#include "EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Input/KeyboardInput.h"
#include "Renderer/SrvManager.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "GameObject/Object3D.h"
#include "GameObject/PrimitiveObject.h"
#include "Effect/ParticleManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"

// ImGuiのヘッダー (パスは環境に合わせてください)
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <cmath>
#include <numbers>

// 枠を借りるための関数 (WindowsApplication.cppからお引越し)
static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    SrvManager::GetInstance()->Allocate(out_cpu_handle, out_gpu_handle);
}

bool EditorManager::showObjects_ = true;
bool EditorManager::showEffects_ = true;

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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // ドッキング有効化

    // 3. Win32バックエンドの初期化
    ImGui_ImplWin32_Init(hwnd);

    // 4. DirectX12バックエンドの初期化
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = device;
    init_info.CommandQueue = commandQueue;
    // BufferCount は決め打ち(通常2か3)にするか、引数で貰うかします（ここでは一般的な2とします）
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
    // --- ドッキングの設定 ---
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // 初回起動時にレイアウトを自動的に構成する
    static bool first_time = true;
    if (first_time) {
        first_time = false;

        // 一度ノードをクリアして再構築
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID dock_id_main = dockspace_id;
        // 左側に「デバッグステータス」
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, NULL, &dock_id_main);
        // 右側に「ライティング・フォグ」
        ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, NULL, &dock_id_main);

        // 中央エリアをさらに上下に分割：上部に「再生コントロール」、残りに「ゲームビュー」
        ImGuiID dock_id_center_top = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.10f, NULL, &dock_id_main);

        // 各ウィンドウを各ノードに割り当てる
        ImGui::DockBuilderDockWindow("Game Control", dock_id_center_top);
        ImGui::DockBuilderDockWindow("Game View", dock_id_main);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
        ImGui::DockBuilderDockWindow("PostEffect", dock_id_right); // Inspectorと同じ場所にタブとして追加

        ImGui::DockBuilderFinish(dockspace_id);
    }

    // --- Game View ウィンドウ ---
    ImGui::Begin("Game View");
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
        ImGui::Image((ImTextureID)renderTextureSrvHandle.ptr, imageSize);
    }
    ImGui::End();

    // --- Hierarchy ウィンドウ ---
    ImGui::Begin("Hierarchy");
    IScene *activeScene = sceneManager->GetCurrentScene();
    if (activeScene) {
        if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto *obj : activeScene->GetObjects()) {
                bool isSelected = (selectedObject_ == obj);
                if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
                    selectedObject_ = obj;
                    selectedParticle_ = nullptr;
                    selectedPrimitive_ = nullptr;
                }
            }
        }
        if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto *particle : activeScene->GetParticles()) {
                bool isSelected = (selectedParticle_ == particle);
                if (ImGui::Selectable(particle->GetName().c_str(), isSelected)) {
                    selectedParticle_ = particle;
                    selectedObject_ = nullptr;
                    selectedPrimitive_ = nullptr;
                }
            }
        }
        if (ImGui::CollapsingHeader("Primitives", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    ImGui::End();

    // --- デバッグカメラの切り替え制御 ---
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) {
        useDebugCamera_ = !useDebugCamera_;
        if (useDebugCamera_) {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
        }
    }

    // --- ゲーム再生制御 (Play/Stop) ---
    // ドッキングを有効にし、タイトルバーを表示
    ImGui::Begin("Game Control");

    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    // ボタン(100) + マージン(10) + チェックボックス(約120) = 230
    float totalWidth = 230.0f;
    ImGui::SetCursorPosX((contentRegion.x - totalWidth) * 0.5f);

    ImGui::BeginGroup();

    if (!isPlaying_) {
        // 停止中：再生ボタンを表示
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
        if (ImGui::Button("PLAY", ImVec2(100, 30))) {
            isPlaying_ = true;
            // 再生開始：デバッグカメラをOFFにする
            useDebugCamera_ = false;
        }
        ImGui::PopStyleColor(3);
    } else {
        // 再生中：停止ボタンを表示
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Button("STOP", ImVec2(100, 30))) {
            isPlaying_ = false;
            // 停止：デバッグカメラをONにする
            useDebugCamera_ = true;
            // デバッグカメラに切り替わった時にゲームカメラの位置を引き継ぐ
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0, 10.0f);
    
    // チェックボックスをボタンの中央の高さに合わせる
    float currentY = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(currentY + 5.0f);
    ImGui::Checkbox("Debug Camera", &useDebugCamera_);

    ImGui::EndGroup();

    ImGui::End();

    // --- Inspector ウィンドウ ---
    ImGui::Begin("Inspector");
    if (selectedObject_) {
        selectedObject_->DisplayImGui("Object Properties");
    } else if (selectedParticle_) {
        selectedParticle_->DrawImGui();
    } else if (selectedPrimitive_) {
        selectedPrimitive_->DisplayImGui("Primitive Properties");
    } else {
        ImGui::Text("Global Visibility Settings");
        ImGui::Separator();
        ImGui::Checkbox("Show Objects (Models)", &showObjects_);
        ImGui::Checkbox("Show Effects (Particles/Primitives)", &showEffects_);
        ImGui::Spacing();


        ImGui::Text("Global Settings (Lighting)");
        ImGui::Separator();

        static int activeLightType = 2;
        static bool enableFog = false;
        static float dIntensity = 1.0f;
        static float pIntensity = 1.0f;
        static float sIntensity = 4.0f;
        static float spotAngleDeg = 30.0f;
        static float spotFalloffDeg = 20.0f;

        ImGui::Text("Active Light Source");
    ImGui::RadioButton("Directional", &activeLightType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Point", &activeLightType, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Spot", &activeLightType, 2);
    ImGui::Separator();

    DirectionalLight *dLight = modelCommon->GetDirectionalLight();
    PointLight *pLight = modelCommon->GetPointLight();
    SpotLight *sLight = modelCommon->GetSpotLight();

    if (activeLightType == 0) {
        dLight->intensity = dIntensity;
        pLight->intensity = 0.0f;
        sLight->intensity = 0.0f;
        ImGui::Text("Directional Light Settings");
        ImGui::ColorEdit4("Color", &dLight->color.x);
        ImGui::DragFloat("Intensity", &dIntensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat3("Direction", &dLight->direction.x, 0.01f, -1.0f, 1.0f);
        dLight->direction = TransformFunctions::Normalize(dLight->direction);
    } else if (activeLightType == 1) {
        pLight->intensity = pIntensity;
        dLight->intensity = 0.0f;
        sLight->intensity = 0.0f;
        ImGui::Text("Point Light Settings");
        ImGui::ColorEdit4("Color", &pLight->color.x);
        ImGui::DragFloat("Intensity", &pIntensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat3("Position", &pLight->position.x, 0.1f);
        ImGui::DragFloat("Radius", &pLight->radius, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Decay", &pLight->decay, 0.01f, 0.0f, 10.0f);
    } else if (activeLightType == 2) {
        sLight->intensity = sIntensity;
        dLight->intensity = 0.0f;
        pLight->intensity = 0.0f;
        ImGui::Text("Spot Light Settings");
        ImGui::ColorEdit4("Color", &sLight->color.x);
        ImGui::DragFloat("Intensity", &sIntensity, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat3("Position", &sLight->position.x, 0.1f);
        if (ImGui::DragFloat3("Direction", &sLight->direction.x, 0.01f, -1.0f, 1.0f)) {
            sLight->direction = TransformFunctions::Normalize(sLight->direction);
        }
        ImGui::DragFloat("Distance", &sLight->distance, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Decay", &sLight->decay, 0.01f, 0.0f, 10.0f);
        ImGui::SliderFloat("Total Angle", &spotAngleDeg, 0.0f, 90.0f);
        ImGui::SliderFloat("Falloff Start", &spotFalloffDeg, 0.0f, spotAngleDeg);
        sLight->cosAngle = std::cos(spotAngleDeg * (std::numbers::pi_v<float> / 180.0f));
        sLight->cosFalloffStart = std::cos(spotFalloffDeg * (std::numbers::pi_v<float> / 180.0f));
    }

    ImGui::Separator();
    ImGui::Checkbox("Enable Fog Effect", &enableFog);
    }
    ImGui::End();

    // --- PostEffect ウィンドウ ---
    ImGui::Begin("PostEffect");
    {
        ImGui::Text("Post Effect Settings");
        ImGui::Separator();
        const char* postEffectItems[] = { "None", "Grayscale", "Sepia", "Vignette" };
        int currentEffect = (int)DirectXCommon::GetInstance()->GetPostEffect();
        if (ImGui::Combo("Effect Type", &currentEffect, postEffectItems, IM_ARRAYSIZE(postEffectItems))) {
            DirectXCommon::GetInstance()->SetPostEffect((DirectXCommon::PostEffect)currentEffect);
        }

        if (currentEffect == (int)DirectXCommon::PostEffect::kVignette) {
            ImGui::Spacing();
            ImGui::Text("Vignette Settings");
            ImGui::Separator();
            auto params = DirectXCommon::GetInstance()->GetVignetteParamsData();
            if (params) {
                ImGui::ColorEdit4("Color", params->color);
                ImGui::DragFloat("Scale", &params->scale, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Power", &params->power, 0.01f, 0.0f, 10.0f);
            }
        }
    }
    ImGui::End();
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
#endif