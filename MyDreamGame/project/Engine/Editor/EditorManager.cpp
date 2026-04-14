#include "EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Input/KeyboardInput.h"
#include "Renderer/SrvManager.h"

// ImGuiのヘッダー (パスは環境に合わせてください)
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <cmath>
#include <numbers>

// 枠を借りるための関数 (WindowsApplication.cppからお引越し)
static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    SrvManager::GetInstance()->Allocate(out_cpu_handle, out_gpu_handle);
}

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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // マルチビューポート有効化

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

void EditorManager::UpdateUI(ModelCommon *modelCommon, GameCamera *gameCamera, DebugCamera *debugCamera, Camera **activeCamera, bool &isDebugCameraActive) {
    // --- デバッグカメラの切り替え制御 ---
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) {
        if (isDebugCameraActive) {
            *activeCamera = gameCamera;
            isDebugCameraActive = false;
        } else {
            debugCamera->SetTranslation(gameCamera->GetTranslation());
            debugCamera->SetRotation(gameCamera->GetRotation());
            *activeCamera = debugCamera;
            isDebugCameraActive = true;
        }
    }

    ImGui::Begin("Debug Status");
    if (isDebugCameraActive) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Debug Camera: ON");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Debug Camera: OFF");
    }
    ImGui::End();

    // --- ライティング管理 ---
    static int activeLightType = 2;
    static bool enableFog = false;
    static float dIntensity = 1.0f;
    static float pIntensity = 1.0f;
    static float sIntensity = 4.0f;
    static float spotAngleDeg = 30.0f;
    static float spotFalloffDeg = 20.0f;

    ImGui::Begin("Lighting & Fog Manager");
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
    ImGui::End();
}

void EditorManager::Draw(ID3D12GraphicsCommandList *commandList) {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

    // マルチビューポート（画面外ウィンドウ）の更新・描画
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void EditorManager::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}