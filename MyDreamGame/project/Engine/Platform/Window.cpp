#include "Window.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "WindowsApplication.h"

#ifdef USE_IMGUI
#include <imgui_impl_win32.h>
#endif

// ImGuiのプロシージャ宣言（外部にある前提）
#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

HWND Window::s_Hwnd_ = nullptr;
HHOOK Window::s_KeyboardHook_ = nullptr;
bool Window::s_EnableCapsLockSuppression_ = true;

LRESULT CALLBACK Window::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_EnableCapsLockSuppression_) {
        KBDLLHOOKSTRUCT *kbd = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (kbd) {
            // CapsLock(VK_CAPITAL) および IME関連キー(半角/全角、変換、無変換、かな等)
            bool isTargetKey = (kbd->vkCode == VK_CAPITAL) ||
                               (kbd->vkCode == VK_KANJI) ||
                               (kbd->vkCode == VK_OEM_AUTO) ||
                               (kbd->vkCode == VK_OEM_ENLW) ||
                               (kbd->vkCode == VK_OEM_COPY) ||
                               (kbd->vkCode == VK_CONVERT) ||
                               (kbd->vkCode == VK_NONCONVERT);

            if (isTargetKey) {
                HWND fgWnd = GetForegroundWindow();
                // 自ウィンドウまたはその子ウィンドウがフォアグラウンド（アクティブ）にある時だけブロック
                if (fgWnd && (fgWnd == s_Hwnd_ || IsChild(s_Hwnd_, fgWnd))) {
                    return 1; // メッセージを破棄してトグル・入力切替を防止
                }
            }
        }
    }
    return CallNextHookEx(s_KeyboardHook_, nCode, wParam, lParam);
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;
#endif

    switch (msg) {
    case WM_SETFOCUS:
        // フォーカス復帰時にも確実にIMEを無効化
        ImmAssociateContext(hwnd, nullptr);
        return 0;
    case WM_IME_SETCONTEXT:
        // OSデフォルトのIME UI（左上の「あ」インジケーターや入力候補枠）を表示させないため、lParam(UIフラグ)を0にして処理
        return DefWindowProc(hwnd, msg, wparam, 0);
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_NOTIFY:
        // IME関連の描画・通知メッセージを消費して表示させない
        return 0;
    case WM_SIZE: {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);
        if (width > 0 && height > 0) {
            if (WindowsApplication::GetInstance()) {
                WindowsApplication::GetInstance()->OnResize(width, height);
            } else if (DirectXCommon::GetInstance()) {
                DirectXCommon::GetInstance()->ResizeSwapchain(width, height);
            }
        }
        return 0;
    }
    case WM_CLOSE:
        // ウィンドウが破棄される前にメインループを抜け、終了処理（JSON保存など）を安全に行えるようにする
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        if (s_KeyboardHook_) {
            UnhookWindowsHookEx(s_KeyboardHook_);
            s_KeyboardHook_ = nullptr;
        }
        if (s_Hwnd_ == hwnd) {
            s_Hwnd_ = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}


void Window::Create(const wchar_t *title, int32_t width, int32_t height) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"MyDreamGameEngine";
    wc_.hInstance = hInst;
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc_);

    RECT wrc = {0, 0, width, height};
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    hwnd_ = CreateWindow(
        wc_.lpszClassName, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wrc.right - wrc.left, wrc.bottom - wrc.top,
        nullptr, nullptr, hInst, nullptr);

    s_Hwnd_ = hwnd_;

    // IMEコンテキストの関連付けを解除（自ウィンドウ上での「あ」「A」インジケーターや変換窓の表示を完全に無効化）
    ImmAssociateContext(hwnd_, nullptr);

    // ゲームウィンドウ操作中のみCapsLockおよびIME切替キーを無効化する低レベルフックを設定
    if (!s_KeyboardHook_) {
        s_KeyboardHook_ = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInst, 0);
    }

    ShowWindow(hwnd_, SW_SHOW);

}

bool Window::ProcessMessage() {
    MSG msg{};
    // 「if」ではなく「while」にして、溜まっているメッセージを全て処理しきる！
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        // もし「終了メッセージ」が来たら、ただちに false を返してゲームループを止める
        if (msg.message == WM_QUIT) {
            return false;
        }
    }
    // メッセージが空になったら true を返して、ゲームの更新（Update/Draw）に進む
    return true;
}

Window::~Window() {
    // 低レベルキーボードフックを解除
    if (s_KeyboardHook_) {
        UnhookWindowsHookEx(s_KeyboardHook_);
        s_KeyboardHook_ = nullptr;
    }
    if (s_Hwnd_ == hwnd_) {
        s_Hwnd_ = nullptr;
    }

    // 1. ウィンドウ本体を破棄する
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    // 2. ウィンドウクラスの登録を解除する
    UnregisterClass(wc_.lpszClassName, wc_.hInstance);
}

void Window::SetFullscreen(bool fullscreen) {
    if (!hwnd_) return;
    if (isFullscreen_ == fullscreen) return;

    if (fullscreen) {
        // 現在のウィンドウの位置とサイズを記憶
        GetWindowRect(hwnd_, &windowRect_);

        // ボーダレスウィンドウに変更
        SetWindowLong(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        // モニターのサイズを取得して全画面に広げる
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info = { sizeof(info) };
        if (GetMonitorInfo(monitor, &info)) {
            SetWindowPos(hwnd_, HWND_TOP,
                info.rcMonitor.left, info.rcMonitor.top,
                info.rcMonitor.right - info.rcMonitor.left,
                info.rcMonitor.bottom - info.rcMonitor.top,
                SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        isFullscreen_ = true;
    } else {
        // 元のウィンドウスタイルに戻す
        SetWindowLong(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        // 元の位置とサイズに戻す
        SetWindowPos(hwnd_, HWND_TOP,
            windowRect_.left, windowRect_.top,
            windowRect_.right - windowRect_.left,
            windowRect_.bottom - windowRect_.top,
            SWP_NOZORDER | SWP_FRAMECHANGED);
        isFullscreen_ = false;
    }
}

void Window::ToggleFullscreen() {
    SetFullscreen(!isFullscreen_);
}

bool Window::IsMaximized() const {
    if (!hwnd_) return false;
    WINDOWPLACEMENT wp = { sizeof(wp) };
    GetWindowPlacement(hwnd_, &wp);
    return wp.showCmd == SW_SHOWMAXIMIZED;
}

void Window::SetMaximized(bool maximized) {
    if (!hwnd_) return;
    ShowWindow(hwnd_, maximized ? SW_MAXIMIZE : SW_RESTORE);
}

int32_t Window::GetNormalWindowWidth() const {
    if (!hwnd_) return 1280;
    WINDOWPLACEMENT wp = { sizeof(wp) };
    GetWindowPlacement(hwnd_, &wp);
    return wp.rcNormalPosition.right - wp.rcNormalPosition.left;
}

int32_t Window::GetNormalWindowHeight() const {
    if (!hwnd_) return 720;
    WINDOWPLACEMENT wp = { sizeof(wp) };
    GetWindowPlacement(hwnd_, &wp);
    return wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
}

void Window::SetWindowSize(int32_t width, int32_t height) {
    if (!hwnd_) return;
    SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
}
