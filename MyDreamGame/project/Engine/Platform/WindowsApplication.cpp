#include "Platform/WindowsApplication.h"

// ★ ヘッダーから追い出したインクルードを、CPP側の一番上で読み込みます
#include "Editor/EditorManager.h"
#include "Effect/ParticleCommon.h"
#include "Graphics/DebugCamera.h"
#include "Graphics/GameCamera.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Resource/Model/ModelCommon.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "Scene/SceneManager.h"
#include "Window.h"
// (※もし足りないヘッダーがあって赤線が出たら、ここに追加してください)

#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/Utilityfunctions.h"
#include "Graphics/TextureManager.h"
#include "Input/GamepadInput.h"
#include "Input/KeyboardInput.h"
#include "Renderer/SrvManager.h"
#include "Resource/Audio/AudioManager.h"
#include "Resource/Model/ModelManager.h"
#include "Graphics/ViewProjection.h"

#pragma comment(lib, "winmm.lib")

WindowsApplication::WindowsApplication() = default;
WindowsApplication::~WindowsApplication() = default;

void WindowsApplication::Initialize() {

    // COMの初期化
    CoInitializeEx(0, COINIT_MULTITHREADED);

    // 窓の作成を任せる
    window_ = std::make_unique<Window>();
    window_->Create(L"MyDreamGameEngine", kWindowWidth_, kWindowHeight_);

    /*********************************************************
     *DirectX初期化処理
     *********************************************************/
    // DirectXCommonクラスのインスタンスを作成し、初期化
    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(window_->GetHwnd(), kWindowWidth_, kWindowHeight_);

    // dxCommon_から必要なポインタを取得
    ID3D12Device *device = dxCommon_->GetDevice();
    ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();

    // キーボードとコントローラーの初期化
    // HINSTANCE は GetModuleHandle(nullptr) で取得できる！
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HWND hwnd = window_->GetHwnd();

    // 2. 取得した hInstance と hwnd を渡す
    KeyboardInput::GetInstance()->Initialize(hInstance, hwnd);
    GamepadInput::GetInstance()->Initialize(hInstance, hwnd);

    // SceneManager の生成
    sceneManager_ = std::make_unique<SceneManager>();

    SrvManager::GetInstance()->Initialize(device);

    // SrvManagerの準備が完了したので、RenderTextureを作る！
    dxCommon_->InitializeRenderTexture();

    // ModelCommonの生成と初期化
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(device);

    ModelManager::GetInstance()->Initialize(modelCommon_.get());

    // SceneManagerに渡す
    sceneManager_->SetModelCommon(modelCommon_.get());
    TextureManager::GetInstance()->Initialize(device);

    // SpriteCommon の生成と初期化
    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_.get(), kWindowWidth_, kWindowHeight_);

    // SpriteCommon を SceneManager に渡す
    sceneManager_->SetSpriteCommon(spriteCommon_.get());

    // SceneManager初期化
    sceneManager_->Initialize(commandList);

    // ParticleCommon の生成と初期化
    particleCommon_ = std::make_unique<ParticleCommon>();
    particleCommon_->Initialize(device);
    sceneManager_->SetParticleCommon(particleCommon_.get());

    // ViewProjectionリソースの作成
    viewProjection_ = std::make_unique<ViewProjection>();
    viewProjection_->Initialize(dxCommon_->GetDevice());

    // 1. 本番カメラ生成
    gameCamera_ = std::make_unique<GameCamera>();
    gameCamera_->Initialize(kWindowWidth_, kWindowHeight_);

    // 2. デバッグカメラ生成
    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(kWindowWidth_, kWindowHeight_);

    // 3. 最初は「本番カメラ」をアクティブにする
    activeCamera_ = gameCamera_.get();
    isDebugCameraActive_ = false;

// Initialize() の中の #ifdef USE_IMGUI のブロックを以下に置き換え
#ifdef USE_IMGUI
    editorManager_ = std::make_unique<EditorManager>();
    // commandQueue は dxCommon から取得して渡します
    editorManager_->Initialize(hwnd, device, dxCommon_->GetCommandQueue());
#endif

    // 音声の初期化
    AudioManager::Initialize();

    // システムタイマーの分解能を上げる
    timeBeginPeriod(1);

    // TimeManager を初期化
    // TimeManager::GetInstance().Initialize();

#ifdef USE_IMGUI
    // リソースリークチェッカーのインスタンスを作成
    // leakChecker_ = std::make_unique<D3DResourceLeakChecker>();
#endif
}

void WindowsApplication::Run() {
    // 窓が「閉じていいよ」と言うまでループする
    while (window_->ProcessMessage()) {
        // 1. 全ての状態を計算・更新する
        Update();

        // 2. 更新された状態をもとに画面を描画する
        Draw();
    }
}

void WindowsApplication::Update() {
    // デルタタイムを計算
    // TimeManager::GetInstance().Update();

    // 入力の更新
    KeyboardInput::GetInstance()->Update();

#ifdef USE_IMGUI
    // 1. フレームの開始
    editorManager_->BeginFrame();

    // 2. UIの更新（巨大なコードがこの1行に！）
    // ※ activeCamera_ の書き換えができるようにアドレス(&)を渡します
    editorManager_->UpdateUI(
        modelCommon_.get(),
        gameCamera_.get(),
        debugCamera_.get(),
        &activeCamera_,
        isDebugCameraActive_);
#endif

    // ★ シーンの更新 (Drawから救出！)
    sceneManager_->Update();

    // カメラの更新
    if (isDebugCameraActive_) {
        debugCamera_->Update();
    } else {
        gameCamera_->Update();
    }

    // ViewProjectionの更新
    viewProjection_->UpdateMatrix(
        activeCamera_->GetViewMatrix(),
        activeCamera_->GetProjectionMatrix());
}

void WindowsApplication::Draw() {
    // 1. RenderTextureへの描画準備
    dxCommon_->PreDraw();

    // --- ここから RenderTexture への描画 ---
    ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap *descriptorHeaps[] = {SrvManager::GetInstance()->GetSrvDescriptorHeap()};
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    modelCommon_->PreDraw(commandList);
    sceneManager_->Draw(viewProjection_->GetMatrix());

    particleCommon_->SetViewProjection(viewProjection_->GetMatrix());
    particleCommon_->PreDraw(commandList);
    // ------------------------------------

    // 2. Swapchain（最終画面）への描画準備
    dxCommon_->PreDrawSwapchain();

    // --- ここから Swapchain への描画 ---

    // ★追加：RenderTextureに描かれた絵を、Swapchainにコピーして貼り付ける！
    dxCommon_->DrawRenderTexture();

#ifdef USE_IMGUI
    // メインウィンドウのImGuiを描画
    editorManager_->Draw(commandList);
#endif
    // ------------------------------------

    // コマンドの実行と画面の表示
    dxCommon_->ExecuteCommands();
    dxCommon_->Present();
}
void WindowsApplication::Finalize() {
#ifdef USE_IMGUI
    if (editorManager_) {
        editorManager_->Finalize();
        editorManager_.reset(); // ★ここで確実に破棄
    }
#endif

    ModelManager::GetInstance()->Finalize();

    // 2. ゲーム層のマネージャー・共通部の解放
    // 下にいくほど「土台」に近いものを消す順番にする
    sceneManager_.reset();
    modelCommon_.reset(); // ★追加：Model共通部の実体を消す

    if (spriteCommon_) {
        spriteCommon_->Finalize();
        spriteCommon_.reset();
    }
    particleCommon_.reset();

    viewProjection_.reset();

    // 4. 入力マネージャーの終了処理 (もし実装があれば呼ぶ)
    // KeyboardInput::GetInstance()->Finalize();
    // GamepadInput::GetInstance()->Finalize();

    // 5. その他システムの解放
    AudioManager::Finalize();
    TextureManager::GetInstance()->Finalize();

    // 6. Windows API 関連のクリーンアップ
    // timeBeginPeriod(1) に対応する解除
    timeEndPeriod(1); // ★追加：タイマー精度を元に戻す

    // 7. 最後にすべての土台である DirectXCommon を消す
    if (dxCommon_) {
        dxCommon_->Finalize();
        // ★重要：ここで reset() すると、この瞬間に Device が消えるため、
        // 上記の 1〜5 がすべて終わっている必要があります。
        dxCommon_.reset();
    }

    // 8. COMの終了処理
    CoUninitialize();
}
