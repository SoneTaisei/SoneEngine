#include "Platform/WindowsApplication.h"
#include "Editor/ReplayManager.h"

// ★ ヘッダーから追い出したインクルードを、CPP側の一番上で読み込みます
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif
#include "Effect/ParticleCommon.h"
#include "Graphics/DebugCamera.h"
#include "Graphics/GameCamera.h"
#ifdef USE_IMGUI
#include "Graphics/MapEditorCamera.h"
#endif
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "../../Project/Scenes/GameScene.h"
#include "Resource/Model/ModelCommon.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneFactory.h"
#include "Window.h"
#include "Core/TimeManager.h"
// (※もし足りないヘッダーがあって赤線が出たら、ここに追加してください)
#include "GameObject/Object3D.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "GameObject/PrimitiveObject.h"

#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/Utilityfunctions.h"
#include "Graphics/TextureManager.h"
#include "Input/GamepadInput.h"
#include "Input/KeyboardInput.h"
#include "Renderer/SrvManager.h"
#include "Resource/Audio/AudioManager.h"
#include "Resource/Model/ModelManager.h"
#include "Graphics/ViewProjection.h"
#include <fstream>
#include <filesystem>

#pragma comment(lib, "winmm.lib")

WindowsApplication::WindowsApplication() = default;
WindowsApplication::~WindowsApplication() = default;

void WindowsApplication::Initialize() {

    // COMの初期化
    CoInitializeEx(0, COINIT_MULTITHREADED);

    // 窓の作成を任せる
    window_ = std::make_unique<Window>();
    window_->Create(L"MyDreamGameEngine", kWindowWidth_, kWindowHeight_);

    LoadWindowConfig();

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
    PrimitiveManager::GetInstance()->Initialize(device);

    // デフォルトの環境マップ（スカイボックス用テクスチャ）をロードして設定
    uint32_t defaultSkyboxHandle = TextureManager::GetInstance()->Load("resources/Sprite/school/rostock_laage_airport_4k.dds", commandList);
    Object3D::SetEnvironmentMapHandle(TextureManager::GetInstance()->GetGpuHandle(defaultSkyboxHandle));

    // 1x1ピクセルのデフォルト白テクスチャをロードして PrimitiveObject に設定
    uint32_t defaultWhiteHandle = TextureManager::GetInstance()->Load("white", commandList);
    PrimitiveObject::SetDefaultTextureHandle(TextureManager::GetInstance()->GetGpuHandle(defaultWhiteHandle));

    // ディゾルブ用のマスクテクスチャをロードして設定
    uint32_t dissolveMaskHandle = TextureManager::GetInstance()->Load("resources/Sprite/School/noise0.png", commandList);
    dxCommon_->SetDissolveMaskTexture(TextureManager::GetInstance()->GetGpuHandle(dissolveMaskHandle));

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

#ifdef USE_IMGUI
    // 3. マップエディタカメラ生成
    mapEditorCamera_ = std::make_unique<MapEditorCamera>();
    mapEditorCamera_->Initialize(kWindowWidth_, kWindowHeight_);
#endif

    // GameCameraをSceneManagerにセット（シーンからカメラモードを切り替え可能にする）
    sceneManager_->SetGameCamera(gameCamera_.get());

#ifdef USE_IMGUI
    // 3. 最初は「停止中（PLAYボタン表示）」としてデバッグカメラを有効にする
    activeCamera_ = debugCamera_.get();
    isDebugCameraActive_ = true;
#else
    // 3. ImGuiを使わない場合は最初から本番カメラ
    activeCamera_ = gameCamera_.get();
    isDebugCameraActive_ = false;
#endif

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
    TimeManager::GetInstance().Initialize();

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
    TimeManager::GetInstance().Update();

    frameCount_++;
    if (frameCount_ == 2) {
        if (pendingMaximized_) {
            window_->SetMaximized(true);
        }
        if (pendingFullscreen_) {
            window_->SetFullscreen(true);
        }
    }

    // 入力の更新
    KeyboardInput::GetInstance()->Update();

    // フルスクリーン切り替え
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_F11)) {
        window_->ToggleFullscreen();
        SaveWindowConfig();
    }

#ifdef USE_IMGUI
    // ESCキーの処理 (閉じる / 最小化) - リリース版では無効化
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_ESCAPE)) {
        bool isShiftDown = KeyboardInput::GetInstance()->IsKeyDown(DIK_LSHIFT) || 
                           KeyboardInput::GetInstance()->IsKeyDown(DIK_RSHIFT);
        if (isShiftDown) {
            // Shift + ESC で最小化
            ShowWindow(window_->GetHwnd(), SW_MINIMIZE);
        } else {
            // ESC のみで終了
            SendMessage(window_->GetHwnd(), WM_CLOSE, 0, 0);
        }
    }
#endif

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
        isDebugCameraActive_,
        dxCommon_->GetPostProcessSrvHandleGPU(),
        sceneManager_.get());
#endif

    // --- エディターの状態に応じて更新処理を切り替え ---
#ifdef USE_IMGUI
    static bool wasActive = false;
    bool isCurrentlyActive = editorManager_->IsPlaying() || ReplayManager::GetInstance()->IsPlaying();
    bool isTakeoverPausing = editorManager_->IsTakeoverCountdown();

    if (isCurrentlyActive && !isTakeoverPausing) {
        if (!wasActive) {
            // アクティブになった瞬間：現在の未保存のマップ状態を一時保存する
            if (sceneManager_->GetCurrentScene()) {
                auto* map = sceneManager_->GetCurrentScene()->GetMapChip();
                if (map) {
                    map->SaveToFile("resources/json/MapData/temp_play_map.txt");
                }
            }
        }

        // 【再生中 / リプレイ中】シーンを更新する（遷移処理も含む）
        sceneManager_->Update();
        wasActive = true;
    } else if (isTakeoverPausing) {
        // カウントダウン中はシーンを更新しないが、wasActiveは維持する
        wasActive = true; 
        if (sceneManager_->GetCurrentScene()) {
            sceneManager_->GetCurrentScene()->UpdateEditor();
        }
    } else {
        if (wasActive) {
            // アクティブから停止状態に切り替わった瞬間：シーンを再生成して初期化リセット！
            
            // プレイ開始前の未保存の変更（temp_play_map）を読み込むため、パスを一時的に差し替える
            std::string originalPath = GameScene::s_TargetMapFilePath;
            GameScene::s_TargetMapFilePath = "resources/json/MapData/temp_play_map.txt";
            
            sceneManager_->ChangeScene(SceneFactory::CreateScene(editorManager_->GetCurrentSceneType()));
            
            // シーン遷移を即時処理してマップをロードさせる
            sceneManager_->ProcessSceneTransition();
            
            // パスを元に戻す（次回の正常なロードやSaveなどのため）
            GameScene::s_TargetMapFilePath = originalPath;
            
            // 新しいシーンが再生成されるため、古いオブジェクトの参照（選択状態）を安全にクリアする
            editorManager_->ClearSelection();
            
            wasActive = false;
        } else {
            // 【停止中】トランスフォーム等の行列再計算のみ実行
            if (sceneManager_->GetCurrentScene()) {
                sceneManager_->GetCurrentScene()->UpdateEditor();
            }
            // シーン遷移のみ処理する（エディターからのシーン切替に対応）
            sceneManager_->ProcessSceneTransition();
        }
    }
    
    // ゲームカメラは常に更新しておく（ViewProjectionへの反映のため）
    gameCamera_->Update();

    // カメラの切り替え（チェックボックスの状態を優先）
    if (editorManager_->IsMapEditorVisible()) {
        activeCamera_ = mapEditorCamera_.get();
        isDebugCameraActive_ = false; // デバッグカメラのUI操作を無効にするため
        bool allowCameraInput = editorManager_->IsMapEditorHovered() && !editorManager_->IsBoundaryDragging();
        mapEditorCamera_->Update(allowCameraInput);
    } else if (editorManager_->UseDebugCamera()) {
        activeCamera_ = debugCamera_.get();
        isDebugCameraActive_ = true;
        
        bool allowCameraInput = editorManager_->IsGameViewHovered() || !ImGui::GetIO().WantCaptureMouse;
        debugCamera_->Update(allowCameraInput);
    } else {
        activeCamera_ = gameCamera_.get();
        isDebugCameraActive_ = false;
    }
#else
    // IMGUI未使用時は通常通り更新
    sceneManager_->Update();
    gameCamera_->Update();
#endif

    // 現在のアクティブカメラの行列をViewProjectionに反映
    viewProjection_->UpdateMatrix(
        activeCamera_->GetViewMatrix(),
        activeCamera_->GetProjectionMatrix());
    
    // 他のオブジェクトが使うCameraManagerも同期させる
    activeCamera_->UpdateMatrix(); 
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

    // ★ ポストエフェクトを実行 (RenderTexture -> PostProcessTexture)
    dxCommon_->ExecutePostEffect();

    // 2. Swapchain（最終画面）への描画準備
    dxCommon_->PreDrawSwapchain();

    // --- ここから Swapchain への描画 ---

#ifdef USE_IMGUI
    // メインウィンドウのImGuiを描画
    editorManager_->Draw(commandList);
#else
    // ImGuiを使わない場合はRenderTextureを直接画面に描画する
    dxCommon_->DrawRenderTexture();
#endif
    // ------------------------------------

    // コマンドの実行と画面の表示
    dxCommon_->ExecuteCommands();
    dxCommon_->Present();
}
void WindowsApplication::Finalize() {
#ifdef USE_IMGUI
    if (editorManager_) {
        // ウィンドウ閉じ時に現在のシーンをJSONに保存する
        editorManager_->SaveSceneConfig();
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

    // 終了前に現在のウィンドウ状態を保存する
    SaveWindowConfig();

    // 8. COMの終了処理
    CoUninitialize();
}

void WindowsApplication::LoadWindowConfig() {
    std::ifstream ifs("resources/json/window_config.json");
    if (!ifs.is_open()) {
        return;
    }

    std::string content;
    std::string line;
    while (std::getline(ifs, line)) {
        content += line;
    }
    ifs.close();

    bool isFullscreen = false;
    bool isMaximized = false;

    auto getBool = [&](const std::string& key, bool& out) {
        size_t pos = content.find(key);
        if (pos != std::string::npos) {
            size_t colon = content.find(':', pos);
            size_t valStart = content.find_first_not_of(" \t", colon + 1);
            if (content.substr(valStart, 4) == "true") out = true;
            else if (content.substr(valStart, 5) == "false") out = false;
        }
    };

    getBool("\"isFullscreen\"", isFullscreen);
    getBool("\"isMaximized\"", isMaximized);

    pendingFullscreen_ = isFullscreen;
    pendingMaximized_ = isMaximized;
}

void WindowsApplication::SaveWindowConfig() {
    std::filesystem::create_directories("json");

    std::ofstream ofs("resources/json/window_config.json");
    if (ofs.is_open()) {
        ofs << "{" << std::endl;
        ofs << "  \"isFullscreen\": " << (window_->IsFullscreen() ? "true" : "false") << "," << std::endl;
        ofs << "  \"isMaximized\": " << (window_->IsMaximized() ? "true" : "false") << std::endl;
        ofs << "}" << std::endl;
        ofs.close();
    }
}
