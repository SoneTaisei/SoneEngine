#pragma once

#include <Windows.h>
#include <memory>

// 前方宣言
class Window;
class EditorManager;
class DirectXCommon;
class SpriteCommon;
class ModelCommon;
class ParticleCommon;
class SceneManager;
class GameCamera;
class DebugCamera;
class Camera;
class ViewProjection;

class WindowsApplication {
public:
    // 定数
    static const int kWindowWidth_ = 1280;
    static const int kWindowHeight_ = 720;

public:
    // コンストラクタとデストラクタ（前方宣言を使っているので明記が必要）
    WindowsApplication();
    ~WindowsApplication();

    void Initialize();
    void Run();
    void Finalize();

    void Update();
    void Draw();

    SceneManager *GetSceneManager() const { return sceneManager_.get(); }

private:
    // --- システム管理 ---
    std::unique_ptr<Window> window_;
    std::unique_ptr<EditorManager> editorManager_;
    std::unique_ptr<DirectXCommon> dxCommon_;

    // --- 描画共通部 ---
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<ModelCommon> modelCommon_;
    std::unique_ptr<ParticleCommon> particleCommon_;
    std::unique_ptr<ViewProjection> viewProjection_; // ★これだけでOK！

    // --- ゲームロジック・カメラ ---
    std::unique_ptr<SceneManager> sceneManager_;
    std::unique_ptr<GameCamera> gameCamera_;
    std::unique_ptr<DebugCamera> debugCamera_;

    // 「現在アクティブなカメラ」を指すポインタ（借用）
    Camera *activeCamera_ = nullptr;

    // 今デバッグモードかどうか
    bool isDebugCameraActive_ = false;
};