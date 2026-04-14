#include "Platform/WindowsApplication.h"
#include "Renderer/DirectXCommon/D3DResourceLeakChecker.h"
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <memory> // std::unique_ptr を使うために追加

#include "Scene/SceneManager.h"
#include "Scenes/TitleScene.h"

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

        app->GetSceneManager()->ChangeScene(std::make_unique<TitleScene>());

        // メインループの実行
        app->Run();

        app->Finalize();
    }
	return 0;
}