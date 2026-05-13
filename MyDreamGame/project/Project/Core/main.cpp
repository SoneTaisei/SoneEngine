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

// windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	// メモリリークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    D3DResourceLeakChecker leakChecker;
    {
        // WindowsApplicationクラスのインスタンスを生成
        auto app = std::make_unique<WindowsApplication>();

        // 初期化
        app->Initialize();

        // 起動シーンの決定
        SceneType startScene = SceneType::kTitle;
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