#include "Window.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"

#ifdef USE_IMGUI
#include <imgui_impl_win32.h>
#endif

// ImGuiのプロシージャ宣言（外部にある前提）
#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;
#endif

    switch (msg) {
    case WM_SIZE: {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);
        if (DirectXCommon::GetInstance() && width > 0 && height > 0) {
            DirectXCommon::GetInstance()->ResizeSwapchain(width, height);
        }
        return 0;
    }
    case WM_DESTROY:
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
    // 1. ウィンドウ本体を破棄する
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    // 2. ウィンドウクラスの登録を解除する
    UnregisterClass(wc_.lpszClassName, wc_.hInstance);
}

void Window::ToggleFullscreen() {
    if (!hwnd_) return;

    if (!isFullscreen_) {
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
