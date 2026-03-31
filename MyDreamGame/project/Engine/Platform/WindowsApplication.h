#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

#include "Scene/SceneManager.h"
#include "Graphics/DebugCamera.h" 
#include "Graphics/GameCamera.h" 
#include "Core/Utility/Utilityfunctions.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Renderer/DirectXCommon/D3DResourceLeakChecker.h"
#include "Resource/Sprite/SpriteCommon.h"
#include "Resource/Model/ModelCommon.h"

#include "Effect/ParticleCommon.h"
#include "Effect/ParticleManager.h"
#include "Effect/SnowParticle.h"

class WindowsApplication {
public:
	// ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	// 定数
	static const int kWindowWidth_ = 1280;
	static const int kWindowHeight_ = 720;

public:
	void Initialize();
	void Run();
	void Finalize();

private:
	HWND hwnd_ = nullptr;
	WNDCLASS wc_{};

	// DirectX関連の処理をまとめたクラス
	std::unique_ptr<DirectXCommon> dxCommon_;

	// スプライト共通部のメンバ変数
	std::unique_ptr<SpriteCommon> spriteCommon_;

	// モデル共通部のメンバ変数
	std::unique_ptr<ModelCommon> modelCommon_;

	// パーティクル共通部・個別パーティクル
	std::unique_ptr<ParticleCommon> particleCommon_;

	// --- DirectX関連以外のメンバ変数 ---
	std::unique_ptr<SceneManager> sceneManager_;

	// 2つのカメラの実体を持つ（メモリ管理用）
	std::unique_ptr<GameCamera> gameCamera_;
	std::unique_ptr<DebugCamera> debugCamera_;

	// 「現在アクティブなカメラ」を指すポインタ（借用）
	Camera *activeCamera_ = nullptr;

	// 今デバッグモードかどうか
	bool isDebugCameraActive_ = false;

	// ViewProjection用 (シーン全体で使うため残す)
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
	ViewProjection *viewProjectionData_ = nullptr;

	// DirectionalLight用 (シーン全体で使うため残す)
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight *directionalLightData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = {};
	Material *materialData = nullptr;

#ifdef USE_IMGUI
// リソースリークチェッカー
	std::unique_ptr<D3DResourceLeakChecker> leakChecker_;
#endif
};