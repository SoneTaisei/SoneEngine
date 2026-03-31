#include "Platform/WindowsApplication.h"
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <memory> // std::unique_ptr を使うために追加

// windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	// メモリリークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    {
        // WindowsApplicationクラスのインスタンスを生成
        auto app = std::make_unique<WindowsApplication>();

        // 初期化
        app->Initialize();

        // メインループの実行
        app->Run();

        app->Finalize();
    }
	return 0;
}