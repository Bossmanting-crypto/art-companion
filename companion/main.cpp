#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>   // must come before gdiplus.h, see TransparentWindow.h
#include <gdiplus.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "shlwapi.lib")

#include "TransparentWindow.h"
#include "InputWatcher.h"
#include "AnimationController.h"

namespace {
constexpr int kSpriteSize = 128;
constexpr UINT_PTR kAnimTimerId = 1;
constexpr UINT_PTR kWatchdogTimerId = 2;
constexpr UINT kWatchdogIntervalMs = 2000;  // how often to check CSP is still running

TransparentWindow g_window;
InputWatcher g_input;
AnimationController g_anim;

// Tracks whether we've ever seen CSP running yet. Companion is normally
// launched *together with* CSP (see launch_with_csp.bat), so at startup CSP
// might take a moment to appear -- we don't want to instantly self-close
// just because it isn't up in the first tick. Once we've seen it running at
// least once, disappearing after that means "the user closed CSP" and we
// close ourselves too.
bool g_everSeenTarget = false;

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}

// Scans all running processes for kTargetProcessName, independent of which
// window currently has focus (unlike InputWatcher's foreground-only check,
// which is only meant for "is CSP the active window right now").
bool IsTargetProcessRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, kTargetProcessName) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    if (!g_window.Create(hInstance, kSpriteSize, kSpriteSize)) {
        MessageBoxW(nullptr, L"Failed to create companion window.", L"ArtCompanion", MB_ICONERROR);
        return 1;
    }

    std::wstring exeDir = ExeDir();
    g_anim.LoadAssets(exeDir + L"\\assets", kSpriteSize, kSpriteSize);

    // Stay hidden (tray icon only) until CSP is actually running -- see the
    // watchdog handler below, which reveals the sprite once CSP is detected
    // and closes the whole app once CSP is detected as closed. This means
    // launching ArtCompanion any time (e.g. from Windows' Startup folder)
    // is safe: it just waits quietly until you open CSP.
    ShowWindow(g_window.Handle(), SW_HIDE);

    // Park the companion in the bottom-right corner of the primary monitor
    // by default.
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    g_window.SetPosition(screenW - kSpriteSize - 40, screenH - kSpriteSize - 80);

    g_input.LoadToolMapConfig(exeDir + L"\\toolmap.ini");

    g_input.SetToolChangeCallback([](ToolState state) {
        g_anim.SetState(state);
    });

    g_input.SetRightClickCallback([](POINT pt) {
        // Post to the window's own message queue rather than showing the
        // menu directly here -- this callback runs on the low-level mouse
        // hook's call stack, and TrackPopupMenu's blocking modal loop can
        // make Windows silently disable a hook that doesn't return quickly.
        PostMessage(g_window.Handle(), TransparentWindow::kShowContextMenuMsg,
            static_cast<WPARAM>(pt.x), static_cast<LPARAM>(pt.y));
    });

    // Left-click-drag: same click-through toggle trick, held for the
    // duration of the drag instead of just a menu popup.
    g_input.SetDragStartCallback([](POINT) {
        g_window.SetClickThrough(false);
    });
    g_input.SetDragMoveCallback([](int dx, int dy) {
        int x, y;
        g_window.GetPosition(x, y);
        g_window.SetPosition(x + dx, y + dy);
    });
    g_input.SetDragEndCallback([]() {
        g_window.SetClickThrough(true);
    });

    // Wire the tray/context-menu "Enabled" checkbox to InputWatcher's gate.
    g_window.SetEnabledQuery([]() { return g_input.IsEnabled(); });
    g_window.SetEnabledCommand([](bool enabled) { g_input.SetEnabled(enabled); });

    if (!g_input.Install(g_window.Handle())) {
        MessageBoxW(nullptr,
            L"Failed to install global hooks. Try running as administrator "
            L"if CSP is elevated, since hooks can't cross privilege levels.",
            L"ArtCompanion", MB_ICONWARNING);
    }

    // Present the first frame immediately, then schedule the next tick using
    // that frame's own delay -- each SetTimer call below re-arms itself with
    // the *new* current frame's delay, so playback follows the GIF's actual
    // per-frame timing instead of a fixed fps.
    g_window.PresentFrame(g_anim.CurrentFrame());
    SetTimer(g_window.Handle(), kAnimTimerId, g_anim.CurrentFrameDelayMs(), nullptr);
    SetTimer(g_window.Handle(), kWatchdogTimerId, kWatchdogIntervalMs, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == kAnimTimerId) {
            g_anim.Tick();
            g_window.PresentFrame(g_anim.CurrentFrame());
            SetTimer(g_window.Handle(), kAnimTimerId, g_anim.CurrentFrameDelayMs(), nullptr);
        } else if (msg.message == WM_TIMER && msg.wParam == kWatchdogTimerId) {
            bool running = IsTargetProcessRunning();
            if (running && !g_everSeenTarget) {
                // First time we've seen CSP running -- reveal the sprite.
                g_everSeenTarget = true;
                ShowWindow(g_window.Handle(), SW_SHOWNOACTIVATE);
            } else if (!running && g_everSeenTarget) {
                // CSP was running before and now isn't -- the user closed it.
                DestroyWindow(g_window.Handle());
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_input.Uninstall();
    return static_cast<int>(msg.wParam);
}
