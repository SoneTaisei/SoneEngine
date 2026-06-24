#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <set>

// UIから操作したいクラスのヘッダーをインクルード
#include "Graphics/DebugCamera.h"
#include "Graphics/GameCamera.h"
#include "Resource/Model/ModelCommon.h"
#include "Scene/SceneFactory.h"

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
    static bool IsPlaying() { return isPlaying_; }
    static void SetPlaying(bool isPlaying) { isPlaying_ = isPlaying; }

    bool IsGameViewHovered() const { return isGameViewHovered_; }
    bool IsMapEditorVisible() const { return isMapEditorVisible_; }
    bool IsMapEditorHovered() const { return isMapEditorHovered_; }
    bool IsBoundaryDragging() const { return draggingBoundaryIndexX_ != -1 || draggingBoundaryIndexY_ != -1; }

    static bool IsShowObjects() { return showObjects_; }
    static bool IsShowEffects() { return showEffects_; }

    bool UseDebugCamera() const { return useDebugCamera_; }
    void SetUseDebugCamera(bool use) { useDebugCamera_ = use; }

    bool IsTakeoverCountdown() const { return takeoverCountdown_ > 0.0f; }

    // シーン設定のJSON保存・読込み
    void SaveSceneConfig();
    void LoadSceneConfig();

    // 現在選択中のシーンタイプを取得
    SceneType GetCurrentSceneType() const { return currentSceneType_; }

    // 選択状態のクリア (シーン再生成時にワイルドポインタになるのを防ぐ)
    void ClearSelection() {
        selectedObject_ = nullptr;
        selectedParticle_ = nullptr;
        selectedPrimitive_ = nullptr;
        sceneJustReset_ = true; // シーンリセットのフラグを立てる
    }

    static ImVec2 GetGameViewPos() { return gameViewPos_; }
    static ImVec2 GetGameViewSize() { return gameViewSize_; }

private:
    static ImVec2 gameViewPos_;
    static ImVec2 gameViewSize_;

    bool sceneJustReset_ = false;
    bool loadMapDataStrNextFrame_ = false;
    std::string mapDataStrToLoad_ = "";
    char stageFilename_[128] = "map_data.txt";

    static bool isPlaying_; // ゲーム再生中かどうか
    bool useDebugCamera_ = true; // デバッグカメラを使用するかどうか
    float takeoverCountdown_ = 0.0f; // 操作引き継ぎ時のカウントダウン

    // 選択中のオブジェクト
    Object3D *selectedObject_ = nullptr;
    ParticleManager *selectedParticle_ = nullptr;
    PrimitiveObject *selectedPrimitive_ = nullptr;

    bool isGameViewHovered_ = false; // ゲームビューがホバーされているか
    bool isMapEditorVisible_ = false; // マップエディタがアクティブタブとして表示されているか
    bool wasMapEditorVisible_ = false; // 前フレームの表示状態
    bool isMapEditorHovered_ = false; // マップエディタがホバーされているか
    
    // マップエディタ用のツール状態
    int mapEditorSelectedTool_ = 100; // 0 = None, 100 = Custom Block 1
    int mapEditorInputWidth_ = -1;
    int mapEditorInputHeight_ = -1;

    // 境界線編集用
    bool isBoundaryEditMode_ = false;
    int boundaryAddMode_ = 0; // 0: 縦線, 1: 横線, 2: 両方(交点)
    int draggingBoundaryIndexX_ = -1;
    int draggingBoundaryIndexY_ = -1;

    std::set<std::string> customToolFilters_;
    std::vector<std::string> availableModels_; // "Object/..." のような相対パスを保持
    void ScanAvailableModels();

    // エディターで選択中のシーンタイプ
    SceneType currentSceneType_ = SceneType::kTitle;

    // 各ウィンドウの表示状態フラグ
    bool showInspector_ = true;
    bool showHierarchy_ = true;
    bool showGameView_ = true;
    bool showPostEffect_ = true;
    bool showReplayManager_ = true;
    bool showMapEditor_ = true;
    bool showMapSettings_ = true;

    static bool showObjects_;
    static bool showEffects_;
};
#endif