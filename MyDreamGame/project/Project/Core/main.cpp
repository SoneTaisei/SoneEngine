#include "Platform/WindowsApplication.h"
#include "Renderer/DirectXCommon/D3DResourceLeakChecker.h"
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <memory> // std::unique_ptr を使うために追加

#include "Scene/SceneManager.h"
#include "Scene/SceneFactory.h"
#ifdef USE_IMGUI
#include "Editor/EditorManager.h"
#endif

#include <filesystem>

// windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Visual Studioの初期設定(カレントディレクトリがProjectDir)の場合でも動作するよう、
    // resourcesフォルダがあればカレントディレクトリを移動する
    // if (std::filesystem::exists("resources") && std::filesystem::is_directory("resources")) {
    //     std::filesystem::current_path("resources");
    // }
	// メモリリークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    D3DResourceLeakChecker leakChecker;
    {
        // WindowsApplicationクラスのインスタンスを生成
        auto app = std::make_unique<WindowsApplication>();

        // 初期化
        app->Initialize();

        // 起動シーンの決定
        SceneType startScene = SceneType::kGame;
#ifdef USE_IMGUI
        // エディターの場合はJSON設定から前回のシーンを復元
        EditorManager* editor = app->GetEditorManager();
        if (editor) {
            editor->LoadSceneConfig();
            startScene = editor->GetCurrentSceneType();
        }
#endif
        app->GetSceneManager()->ChangeScene(SceneFactory::CreateScene(startScene));

        // メインループの実行
        app->Run();

        app->Finalize();
    }
	return 0;
}
