#pragma once
#include <Windows.h>
#include <imm.h>
#include <cstdint>

#pragma comment(lib, "imm32.lib")

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

    // CapsLockの無効化（ゲームウィンドウがアクティブな時のみブロック）の有効/無効
    static void SetCapsLockSuppression(bool enable) { s_EnableCapsLockSuppression_ = enable; }
    static bool IsCapsLockSuppressed() { return s_EnableCapsLockSuppression_; }

private:
    // 低レベルキーボードフック（ゲームウィンドウ操作中のみCapsLockをブロック）
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    static HWND s_Hwnd_;
    static HHOOK s_KeyboardHook_;
    static bool s_EnableCapsLockSuppression_;

    HWND hwnd_ = nullptr;
    WNDCLASS wc_{};
    bool isFullscreen_ = false;
    RECT windowRect_{};
};

