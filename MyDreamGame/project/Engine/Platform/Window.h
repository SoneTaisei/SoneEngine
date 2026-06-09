#pragma once
#include <Windows.h>
#include <cstdint>

class Window {
public:
    // ウィンドウプロシージャ（OSからのメッセージを受け取る窓口）
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    // ウィンドウの作成
    void Create(const wchar_t *title, int32_t width, int32_t height);

    // メッセージの処理（ループの中で呼ぶ）
    bool ProcessMessage();

    // ゲッター
    HWND GetHwnd() const { return hwnd_; }

    // デストラクタを追加（後片付け用）
    ~Window();

    // フルスクリーンの切り替え
    void ToggleFullscreen();
    void SetFullscreen(bool fullscreen);
    bool IsFullscreen() const { return isFullscreen_; }

    // 最大化・ウィンドウサイズの取得・設定
    bool IsMaximized() const;
    void SetMaximized(bool maximized);
    int32_t GetNormalWindowWidth() const;
    int32_t GetNormalWindowHeight() const;
    void SetWindowSize(int32_t width, int32_t height);

private:
    HWND hwnd_ = nullptr;
    WNDCLASS wc_{};
    bool isFullscreen_ = false;
    RECT windowRect_{};
};
