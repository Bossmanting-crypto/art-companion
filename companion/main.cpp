#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>   // must come before gdiplus.h, see TransparentWindow.h
#include <gdiplus.h>
#include <shlwapi.h>
#include <psapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")

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

// Tracks whether we've ever seen CSP running yet. Companion is meant to be
// added to Windows' Startup folder and just wait quietly -- at startup CSP
// might not be open yet, and that's fine, we just stay hidden until it is.
// Once we've seen it running at least once, disappearing after that means
// "the user closed CSP" and we close ourselves too.
bool g_everSeenTarget = false;

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}

bool WindowBelongsToTargetProcess(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return false;

    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;

    wchar_t exeName[MAX_PATH]{};
    DWORD size = MAX_PATH;
    bool ok = QueryFullProcessImageNameW(proc, 0, exeName, &size);
    CloseHandle(proc);
    if (!ok) return false;

    std::wstring path(exeName);
    auto pos = path.find_last_of(L"\\/");
    std::wstring fileName = (pos == std::wstring::npos) ? path : path.substr(pos + 1);
    return _wcsicmp(fileName.c_str(), kTargetProcessName) == 0;
}

BOOL CALLBACK EnumTargetWindowProc(HWND hwnd, LPARAM lParam) {
    // Skip invisible/titleless windows -- background helper processes and
    // CELSYS's separate "CLIP STUDIO" launcher/hub app can have windows or
    // processes that share naming with the Paint app, but they don't have
    // a real, visible, titled canvas window the way an open document does.
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthW(hwnd) == 0) return TRUE;

    if (WindowBelongsToTargetProcess(hwnd)) {
        *reinterpret_cast<bool*>(lParam) = true;
        return FALSE;  // found it, stop enumerating
    }
    return TRUE;
}

// Checks for an actual visible, titled CLIP STUDIO PAINT window -- not just
// a same-named process existing somewhere. This specifically avoids
// triggering off CELSYS's separate account/launcher "CLIP STUDIO" hub app,
// or any idle background helper process, since neither has a real canvas
// window open.
bool IsTargetProcessRunning() {
    bool found = false;
    EnumWindows(EnumTargetWindowProc, reinterpret_cast<LPARAM>(&found));
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

    // "Actively drawing" detection: fires when the mouse is pressed
    // somewhere other than the sprite itself while CSP is the active app
    // (see InputWatcher's mouse hook for the exact heuristic).
    g_input.SetDrawStartCallback([]() {
        g_anim.SetDrawing(true);
    });
    g_input.SetDrawEndCallback([]() {
        g_anim.SetDrawing(false);
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
