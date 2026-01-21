#include "Platform/WindowsApplication.h"
#include "Utility/Utilityfunctions.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"
#include "Graphics/TextureManager.h"
#include "Audio/AudioManager.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "Graphics/DebugCamera.h"
#include <cassert>
#include <format>
#include <chrono>
#include "Utility/TransformFunctions.h"

// ImGuiの外部リンケージ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "winmm.lib")


LRESULT CALLBACK WindowsApplication::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ImGuiへのメッセージ転送
	if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}

	// メッセージに応じてゲーム固有の処理を行う
	switch(msg) {
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

	RECT wrc = { 0, 0, kWindowWidth_, kWindowHeight_ };
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


	// ViewProjectionリソースの作成
	UINT viewProjectionSize = (sizeof(ViewProjection) + 255) & ~255;
	viewProjectionResource_ = CreateBufferResource(device, viewProjectionSize);
	viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void **>(&viewProjectionData_));

	// DirectionalLightリソースの作成
	const UINT directionalLightBufferSize = (sizeof(DirectionalLight) + 255) & ~255u;
	directionalLightResource_ = CreateBufferResource(device, directionalLightBufferSize);
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightData_));
	// 初期値を設定
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
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

	// ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd_);
	ImGui_ImplDX12_Init(device,
						dxCommon_->GetSwapChainDesc().BufferCount,
						dxCommon_->GetRtvDesc().Format,
						TextureManager::GetInstance()->GetSrvDescriptorHeap(),
						TextureManager::GetInstance()->GetSrvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
						TextureManager::GetInstance()->GetSrvDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
	);

	// 音声の初期化
	AudioManager::Initialize();

	// システムタイマーの分解能を上げる
	timeBeginPeriod(1);

	// TimeManager を初期化
	//TimeManager::GetInstance().Initialize();

	const UINT materialBufferSize = (sizeof(Material) + 255) & ~255u;
	materialResource = CreateBufferResource(device, materialBufferSize);
	materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->lightingType = 1;
	materialData->uvTransform = TransformFunctions::MakeIdentity4x4();
    materialData->shininess = 50.0f;
	materialResource->Unmap(0, nullptr);

#ifdef _DEBUG
// リソースリークチェッカーのインスタンスを作成
	leakChecker_ = std::make_unique<D3DResourceLeakChecker>();
#endif
}

void WindowsApplication::Run() {

	MSG msg{};
	// ウィンドウのxボタンが押されるまでループ
	while(msg.message != WM_QUIT) {
		// Windowにメッセージが来てたら最優先で処理させる
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {

			// デルタタイムを計算
			//TimeManager::GetInstance().Update();


			// --- 更新処理 (Update) ---
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			KeyboardInput::GetInstance()->Update();

		#ifdef _DEBUG
			if(KeyboardInput::GetInstance()->IsKeyPressed(DIK_F3)) {
				if(isDebugCameraActive_) {
					// ▼▼▼ デバッグ → ゲームに戻る時 ▼▼▼

					// 【重要】ここで座標のコピーをしてはいけません！
					// 何もしなければ、GameCameraはずっと待機していた場所（元の位置）にいます。

					activeCamera_ = gameCamera_.get(); // 指名を変えるだけ
					isDebugCameraActive_ = false;

				} else {
					// ▼▼▼ ゲーム → デバッグに行く時 ▼▼▼

					// こちらは「今のゲーム画面の位置」からデバッグ操作を始めたいので
					// GameCamera の場所を DebugCamera にコピーします。
					debugCamera_->SetTranslation(gameCamera_->GetTranslation());
					debugCamera_->SetRotation(gameCamera_->GetRotation());

					activeCamera_ = debugCamera_.get(); // 指名を変える
					isDebugCameraActive_ = true;
				}
			}

			ImGui::Begin("Debug Status");

			if(isDebugCameraActive_) {
				// ONなら緑色で表示
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Debug Camera: ON");
			} else {
				// OFFなら灰色で表示
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Debug Camera: OFF");
			}

			ImGui::End();

			ImGui::Begin("Global Lighting Manager"); // ウィンドウ名！

            // 1. ライトの向き (X, Y, Z) をスライダーでいじる！
            // 光の向きは "Direction" だから、直感的！
            ImGui::DragFloat3("Sun Direction", &directionalLightData_->direction.x, 0.01f, -5.0f, 5.0f);

            // 2. ライトの色 (R, G, B, A) をカラーピッカーで変える！
            ImGui::ColorEdit4("Sun Color", &directionalLightData_->color.x);

            // 3. ライトの強さ (Intensity) もあるならここで！
            // ImGui::DragFloat("Sun Intensity", &directionalLightData_->intensity, 0.01f, 0.0f, 10.0f);

            // ⚠ 重要：方向ベクトルは正規化（長さを1に）しておかないと計算がおかしくなることがあるので、
            // Vector3のNormalize関数などがあれば通しておくと完璧です！
            // directionalLightData_->direction = Normalize(directionalLightData_->direction);

            ImGui::End();

		#endif

		// ★ 5. アクティブなカメラだけを更新する
		// デバッグカメラならマウス操作、本番カメラなら追従処理が走る
			if(isDebugCameraActive_) {
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
				{ 1.0f, 1.0f, 1.0f },             // Scale
				activeCamera_->GetRotation(),     // Rotate (現在アクティブなカメラの回転)
				activeCamera_->GetTranslation()   // Translate (現在アクティブなカメラの座標)
			);


			ID3D12GraphicsCommandList *commandList = dxCommon_->GetCommandList();

			ID3D12DescriptorHeap *descriptorHeaps[] = {TextureManager::GetInstance()->GetSrvDescriptorHeap()};
            commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// 定数バッファの設定 (これはゲーム固有の描画処理)
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
            //commandList->SetGraphicsRootConstantBufferView(4, pointLightResource_->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootConstantBufferView(3, viewProjectionResource_->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootConstantBufferView(4, directionalLightResource_->GetGPUVirtualAddress());

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
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

			// ImGuiの描画
			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

			// 描画後処理
			dxCommon_->PostDraw();
		}
	}
}

void WindowsApplication::Finalize() {
	// ImGui解放
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	debugCamera_.reset();
	gameCamera_.reset();
	sceneManager_.reset();

	if(spriteCommon_) {
		spriteCommon_->Finalize();
		spriteCommon_.reset();
	}

	// パーティクルの解放 (unique_ptrなので自動解放されるが明示的にも書ける)
	particleCommon_.reset();

	// その他マネージャクラスの解放
	AudioManager::Finalize();
	TextureManager::GetInstance()->Finalize();

	// DirectXCommonの終了処理
	if(dxCommon_) {
		dxCommon_->Finalize();
		dxCommon_.reset();
	}

	// COMの終了処理
	CoUninitialize();
}