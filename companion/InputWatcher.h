#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include "ToolMap.h"

// Installs global low-level keyboard + mouse hooks and:
//  1. Detects tool-switch keystrokes while the target app (CSP) is foreground
//     and reports them as ToolState changes.
//  2. Detects a right-click landing inside the companion window's bounds so
//     the caller can temporarily disable click-through and show a menu.
//
// NOTE: WH_KEYBOARD_LL / WH_MOUSE_LL hooks must be installed from a thread
// that runs a standard Win32 message loop (GetMessage/DispatchMessage) --
// they are called back on that thread. We install from the main thread.
class InputWatcher {
public:
    using ToolChangeCallback = std::function<void(ToolState)>;
    using RightClickCallback = std::function<void(POINT)>;

    bool Install(HWND companionWnd);
    void Uninstall();

    void SetToolChangeCallback(ToolChangeCallback cb) { onToolChange_ = std::move(cb); }
    void SetRightClickCallback(RightClickCallback cb) { onRightClick_ = std::move(cb); }

    // Loads key->tool overrides from toolmap.ini next to the executable.
    // Falls back to DefaultKeyToolMap() for any key not present in the file.
    void LoadToolMapConfig(const std::wstring& iniPath);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    bool IsForegroundTargetProcess() const;

    static InputWatcher* instance_;  // hooks are process-global; one watcher at a time
    HWND companionWnd_ = nullptr;
    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;

    std::unordered_map<UINT, ToolState> keyMap_;
    ToolState lastState_ = ToolState::Idle;

    ToolChangeCallback onToolChange_;
    RightClickCallback onRightClick_;
};
