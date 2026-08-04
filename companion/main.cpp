#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include "TransparentWindow.h"
#include "InputWatcher.h"
#include "AnimationController.h"

namespace {
constexpr int kSpriteSize = 128;
constexpr UINT_PTR kAnimTimerId = 1;
constexpr UINT_PTR kPollTimerId = 2;   // fallback foreground/idle check
constexpr UINT kAnimIntervalMs = 100;  // ~10 fps placeholder playback
constexpr UINT kPollIntervalMs = 500;

TransparentWindow g_window;
InputWatcher g_input;
AnimationController g_anim;

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    if (!g_window.Create(hInstance, kSpriteSize, kSpriteSize)) {
        MessageBoxW(nullptr, L"Failed to create companion window.", L"ArtCompanion", MB_ICONERROR);
        return 1;
    }

    std::wstring exeDir = ExeDir();
    g_anim.LoadAssets(exeDir + L"\\assets", kSpriteSize, kSpriteSize);

    // Park the companion in the bottom-right corner of the primary monitor
    // by default; make this draggable/configurable later.
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    g_window.SetPosition(screenW - kSpriteSize - 40, screenH - kSpriteSize - 80);

    g_input.LoadToolMapConfig(exeDir + L"\\toolmap.ini");

    g_input.SetToolChangeCallback([](ToolState state) {
        g_anim.SetState(state);
    });

    g_input.SetRightClickCallback([](POINT pt) {
        // Briefly disable click-through so TrackPopupMenu can receive input,
        // then restore it once the menu closes.
        g_window.SetClickThrough(false);
        g_window.ShowContextMenu(pt);
        g_window.SetClickThrough(true);
    });

    if (!g_input.Install(g_window.Handle())) {
        MessageBoxW(nullptr,
            L"Failed to install global hooks. Try running as administrator "
            L"if CSP is elevated, since hooks can't cross privilege levels.",
            L"ArtCompanion", MB_ICONWARNING);
    }

    SetTimer(g_window.Handle(), kAnimTimerId, kAnimIntervalMs, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == kAnimTimerId) {
            g_anim.Tick();
            g_window.PresentFrame(g_anim.CurrentFrame());
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_input.Uninstall();
    return static_cast<int>(msg.wParam);
}
