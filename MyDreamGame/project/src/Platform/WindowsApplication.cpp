#include "Platform/WindowsApplication.h"
#include "Audio/AudioManager.h"
#include "Graphics/DebugCamera.h"
#include "Graphics/TextureManager.h"
#include "Input/GamepadInput.h"
#include "Input/KeyboardInput.h"
#include "Model/Model.h"
#include "Scene/TitleScene.h"
#include "Sprite/Sprite.h"
#include "Utility/TransformFunctions.h"
#include "Utility/Utilityfunctions.h"
#include <cassert>
#include <chrono>
#include <format>

// 枠を借りるための関数
static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
    // 前回の回答で TextureManager に追加した関数を呼び出す
    TextureManager::GetInstance()->AllocateDescriptor(out_cpu_handle, out_gpu_handle);
}

// 枠を返すための関数 (今は何もしなくてOKですが、定義だけは必要です)
static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
    // 解放処理が必要な場合はここに書きますが、今は空で大丈夫です
}

// ImGuiの外部リンケージ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "winmm.lib")

LRESULT CALLBACK WindowsApplication::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
    // ImGuiへのメッセージ転送
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
#endif

    // メッセージに応じてゲーム固有の処理を行う
    switch (msg) {
        // ウィンドウが破棄された
    case WM_DESTROY:
        // OSに対して、アプリの終了を伝える
        PostQuitMessage(0);
        return 0;
    }

    // 標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WindowsApplication::Initialize() {

    // COMの初期化
    CoInitializeEx(0, COINIT_MULTITHREADED);

    /*********************************************************
     *WindowsAPIの初期化
     *********************************************************/
    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"MyDreamGame";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc_);

    RECT wrc = {0, 0, kWindowWidth_, kWindowHeight_};
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    hwnd_ = CreateWindow(
        wc_.lpszClassName,
        L"LE2B_13_ソネ_タイセイ",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc_.hInstance,
        nullptr);

    ShowWindow(hwnd_, SW_SHOW);

    /*********************************************************
     *DirectX初期化処理
     *********************************************************/
    // DirectXCommonクラスのインスタンスを作成し、初期化
    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(hwnd_, kWindowWidth_, kWindowHeight_);

    // dxCommon_から必要なポインタを取得
    ID3D12Device *device = dxCommon_->GetDevice();
    ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();

    // キーボードとコントローラーの初期化
    KeyboardInput::GetInstance()->Initialize(wc_.hInstance, hwnd_);
    GamepadInput::GetInstance()->Initialize(wc_.hInstance, hwnd_);

    // SceneManager の生成
    sceneManager_ = std::make_unique<SceneManager>();

    // ModelCommonの生成と初期化
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(device);

    // SceneManagerに渡す
    sceneManager_->SetModelCommon(modelCommon_.get());
    TextureManager::GetInstance()->Initialize(device);

    // SpriteCommon の生成と初期化
    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_->GetDevice(), kWindowWidth_, kWindowHeight_);

    // SpriteCommon を SceneManager に渡す
    sceneManager_->SetSpriteCommon(spriteCommon_.get());

    // SceneManager初期化
    sceneManager_->Initialize(commandList);

    // ParticleCommon の生成と初期化
    particleCommon_ = std::make_unique<ParticleCommon>();
    particleCommon_->Initialize(device);
    sceneManager_->SetParticleCommon(particleCommon_.get());

    // 初期シーンはアプリ層で生成して渡す
    sceneManager_->ChangeScene(new TitleScene());

    // ViewProjectionリソースの作成
    UINT viewProjectionSize = (sizeof(ViewProjection) + 255) & ~255;
    viewProjectionResource_ = CreateBufferResource(device, viewProjectionSize);
    viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void **>(&viewProjectionData_));

    // DirectionalLightリソースの作成
    const UINT directionalLightBufferSize = (sizeof(DirectionalLight) + 255) & ~255u;
    directionalLightResource_ = CreateBufferResource(device, directionalLightBufferSize);
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightData_));
    // 初期値を設定
    directionalLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
    directionalLightData_->direction = {0.0f, -1.0f, 0.0f};
    directionalLightData_->intensity = 1.0f;

    // 1. 本番カメラ生成
    gameCamera_ = std::make_unique<GameCamera>();
    gameCamera_->Initialize(kWindowWidth_, kWindowHeight_);

    // 2. デバッグカメラ生成
    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(kWindowWidth_, kWindowHeight_);

    // 3. 最初は「本番カメラ」をアクティブにする
    activeCamera_ = gameCamera_.get();
    isDebugCameraActive_ = false;

#ifdef USE_IMGUI

    // ImGuiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = device;
    init_info.CommandQueue = dxCommon_->GetCommandQueue();
    init_info.NumFramesInFlight = dxCommon_->GetSwapChainDesc().BufferCount;
    init_info.RTVFormat = dxCommon_->GetRtvDesc().Format;
    init_info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    init_info.SrvDescriptorHeap = TextureManager::GetInstance()->GetSrvDescriptorHeap();

    // ★ここがポイント！定義した関数を教える
    init_info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
    init_info.SrvDescriptorFreeFn = ImGuiSrvFree;

    ImGui_ImplDX12_Init(&init_info);
    unsigned char *pixels;
    int width, height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGuiIO &io = ImGui::GetIO();
    // ドッキング機能を有効化
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ウィンドウの外に飛び出させたい場合（マルチビューポート）も有効化
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        // ウィンドウの角を丸くしない（透過時に境界が綺麗に見えます）
        style.WindowRounding = 0.0f;
        // 背景の「黒さ」の原因であるアルファ値を調整（0.0fで完全透明、1.0fで不透明）
        // 0.8f くらいにすると、デスクトップが少し透けて「ツールっぽさ」が増します！✨
        style.Colors[ImGuiCol_WindowBg].w = 0.8f;
    }
#endif // USE_IMGUI

    // 音声の初期化
    AudioManager::Initialize();

    // システムタイマーの分解能を上げる
    timeBeginPeriod(1);

    // TimeManager を初期化
    // TimeManager::GetInstance().Initialize();

    const UINT materialBufferSize = (sizeof(Material) + 255) & ~255u;
    materialResource = CreateBufferResource(device, materialBufferSize);
    materialData = nullptr;
    materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
    materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
    materialData->lightingType = 1;
    materialData->uvTransform = TransformFunctions::MakeIdentity4x4();
    materialData->shininess = 50.0f;
    materialResource->Unmap(0, nullptr);

#ifdef USE_IMGUI
    // リソースリークチェッカーのインスタンスを作成
    //leakChecker_ = std::make_unique<D3DResourceLeakChecker>();
#endif
}

void WindowsApplication::Run() {

    MSG msg{};
    // ウィンドウのxボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        // Windowにメッセージが来てたら最優先で処理させる
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {

            // デルタタイムを計算
            // TimeManager::GetInstance().Update();

#ifdef USE_IMGUI
            // --- 更新処理 (Update) ---
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
#endif

            KeyboardInput::GetInstance()->Update();

#ifdef USE_IMGUI
            // --- 1. デバッグカメラとステータス表示 ---
            if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) {
                if (isDebugCameraActive_) {
                    activeCamera_ = gameCamera_.get();
                    isDebugCameraActive_ = false;
                } else {
                    debugCamera_->SetTranslation(gameCamera_->GetTranslation());
                    debugCamera_->SetRotation(gameCamera_->GetRotation());
                    activeCamera_ = debugCamera_.get();
                    isDebugCameraActive_ = true;
                }
            }

            ImGui::Begin("Debug Status");
            if (isDebugCameraActive_) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Debug Camera: ON");
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Debug Camera: OFF");
            }
            ImGui::End();

            // --- 2. ライティング管理 (ここがバグ修正の肝です) ---
            static int activeLightType = 2; // 0:Directional, 1:Point, 2:Spot
            static bool enableFog = false;

            // 各ライトの「本来の明るさ」を保持する変数 (これがないと切り替え時に値が消えます)
            static float dIntensity = 1.0f;
            static float pIntensity = 1.0f;
            static float sIntensity = 4.0f;

            // スポットライトの操作用度数法変数
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

            // ライトのポインタを取得
            DirectionalLight *dLight = modelCommon_->GetDirectionalLight();
            PointLight *pLight = modelCommon_->GetPointLight();
            SpotLight *sLight = modelCommon_->GetSpotLight();

            // 💡 選択されたライト以外を 0 にし、本来の値を static 変数から復元する
            if (activeLightType == 0) { // 平行光源
                dLight->intensity = dIntensity;
                pLight->intensity = 0.0f;
                sLight->intensity = 0.0f;

                ImGui::Text("Directional Light Settings");
                ImGui::ColorEdit4("Color", &dLight->color.x);
                ImGui::DragFloat("Intensity", &dIntensity, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat3("Direction", &dLight->direction.x, 0.01f, -1.0f, 1.0f);
                dLight->direction = TransformFunctions::Normalize(dLight->direction);
            } else if (activeLightType == 1) { // ポイントライト
                pLight->intensity = pIntensity;
                dLight->intensity = 0.0f;
                sLight->intensity = 0.0f;

                ImGui::Text("Point Light Settings");
                ImGui::ColorEdit4("Color", &pLight->color.x);
                ImGui::DragFloat("Intensity", &pIntensity, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat3("Position", &pLight->position.x, 0.1f);
                ImGui::DragFloat("Radius", &pLight->radius, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Decay", &pLight->decay, 0.01f, 0.0f, 10.0f);
            } else if (activeLightType == 2) { // スポットライト
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

                // 💡 角度とFalloffの調整（image_60f761.png の実装）
                ImGui::SliderFloat("Total Angle", &spotAngleDeg, 0.0f, 90.0f);
                ImGui::SliderFloat("Falloff Start", &spotFalloffDeg, 0.0f, spotAngleDeg);

                sLight->cosAngle = std::cos(spotAngleDeg * (std::numbers::pi_v<float> / 180.0f));
                sLight->cosFalloffStart = std::cos(spotFalloffDeg * (std::numbers::pi_v<float> / 180.0f));
            }

            ImGui::Separator();
            ImGui::Checkbox("Enable Fog Effect", &enableFog);
            if (enableFog) {
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "Fog is Active! (Add lerp in Pixel Shader)");
            }
            ImGui::End();
#endif

            // ★ 5. アクティブなカメラだけを更新する
            // デバッグカメラならマウス操作、本番カメラなら追従処理が走る
            if (isDebugCameraActive_) {
                debugCamera_->Update();
            } else {
                gameCamera_->Update();
            }
            // ViewProjectionを更新
            Matrix4x4 viewMatrix = activeCamera_->GetViewMatrix();
            Matrix4x4 projectionMatrix = debugCamera_->GetProjectionMatrix();
            viewProjectionData_->viewProjectionMatrix = TransformFunctions::Multiply(viewMatrix, projectionMatrix);
            viewProjectionData_->cameraPosition = debugCamera_->GetTranslation();
            // --- 描画処理 (Draw) ---
            dxCommon_->PreDraw();

            sceneManager_->Update();

            Matrix4x4 cameraMatrix = TransformFunctions::MakeAffineMatrix(
                {1.0f, 1.0f, 1.0f},             // Scale
                activeCamera_->GetRotation(),   // Rotate (現在アクティブなカメラの回転)
                activeCamera_->GetTranslation() // Translate (現在アクティブなカメラの座標)
            );

            ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();

            ID3D12DescriptorHeap *descriptorHeaps[] = {TextureManager::GetInstance()->GetSrvDescriptorHeap()};
            commandList->SetDescriptorHeaps(1, descriptorHeaps);

            // 定数バッファの設定 (これはゲーム固有の描画処理)
            // commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            // commandList->SetGraphicsRootConstantBufferView(4, pointLightResource_->GetGPUVirtualAddress());
            // commandList->SetGraphicsRootConstantBufferView(3, viewProjectionResource_->GetGPUVirtualAddress());
            // commandList->SetGraphicsRootConstantBufferView(4, directionalLightResource_->GetGPUVirtualAddress());

            // ModelCommonの描画前準備
            modelCommon_->PreDraw(commandList);

            // SceneManagerによる描画
            sceneManager_->Draw(viewProjectionData_->viewProjectionMatrix);

            particleCommon_->SetViewProjection(viewProjectionData_->viewProjectionMatrix);

            // パーティクルの描画
            // 1. 共通設定 (RootSignature, PSO, Mesh, DescriptorHeap)
            particleCommon_->PreDraw(commandList);

            // 2. 共通パラメータ (ViewProjection, Light) をセット
            // ※ ParticleCommon::CreateRootSignature の定義順序に合わせる
            // [3] DirectionalLight (CBV b1)
            // commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

#ifdef USE_IMGUI
            // ImGuiの描画
            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

            ImGuiIO &io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
#endif

            // 描画後処理
            dxCommon_->PostDraw();
        }
    }
}

void WindowsApplication::Finalize() {
#ifdef USE_IMGUI
    // ImGui解放
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif
    debugCamera_.reset();
    gameCamera_.reset();
    sceneManager_.reset();

    if (spriteCommon_) {
        spriteCommon_->Finalize();
        spriteCommon_.reset();
    }

    // パーティクルの解放 (unique_ptrなので自動解放されるが明示的にも書ける)
    particleCommon_.reset();

    // その他マネージャクラスの解放
    AudioManager::Finalize();
    TextureManager::GetInstance()->Finalize();

    // DirectXCommonの終了処理
    if (dxCommon_) {
        dxCommon_->Finalize();
        dxCommon_.reset();
    }

    // COMの終了処理
    CoUninitialize();
}