#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>

// UIから操作したいクラスのヘッダーをインクルード
#include "Graphics/DebugCamera.h"
#include "Graphics/GameCamera.h"
#include "Resource/Model/ModelCommon.h"

class SceneManager;
class ParticleManager;
class Object3D;
class PrimitiveObject;

class EditorManager {
public:
    // 初期化 (ImGuiのセットアップ)
    void Initialize(HWND hwnd, ID3D12Device *device, ID3D12CommandQueue *commandQueue);

    // 毎フレームのUI構築前処理
    void BeginFrame();

    // 実際のUI構築 (ライトやカメラの調整)
    // 💡 値を書き換えるため、ポインタや参照を受け取ります
    void UpdateUI(ModelCommon *modelCommon, GameCamera *gameCamera, DebugCamera *debugCamera, Camera **activeCamera, bool &isDebugCameraActive, D3D12_GPU_DESCRIPTOR_HANDLE renderTextureSrvHandle, SceneManager *sceneManager);

    // 描画処理 (コマンドリストへImGuiの描画命令を積む)
    void Draw(ID3D12GraphicsCommandList *commandList);

    // 終了処理 (ImGuiの解放)
    void Finalize();

    // 再生状態の取得・設定
    bool IsPlaying() const { return isPlaying_; }
    void SetPlaying(bool isPlaying) { isPlaying_ = isPlaying; }

    bool IsGameViewHovered() const { return isGameViewHovered_; }

    static bool IsShowObjects() { return showObjects_; }
    static bool IsShowEffects() { return showEffects_; }

    bool UseDebugCamera() const { return useDebugCamera_; }
    void SetUseDebugCamera(bool use) { useDebugCamera_ = use; }

private:
    bool isPlaying_ = false; // ゲーム再生中かどうか
    bool useDebugCamera_ = true; // デバッグカメラを使用するかどうか

    // 選択中のオブジェクト
    Object3D *selectedObject_ = nullptr;
    ParticleManager *selectedParticle_ = nullptr;
    PrimitiveObject *selectedPrimitive_ = nullptr;

    bool isGameViewHovered_ = false; // ゲームビューがホバーされているか

    static bool showObjects_;
    static bool showEffects_;
};
#endif