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
//  3. Detects a left-click-and-drag landing inside the companion window's
//     bounds so the caller can reposition the window while held.
//  4. Detects a left-click-and-hold landing OUTSIDE the companion window's
//     bounds while CSP is the foreground app -- treated as "the user is
//     actively drawing a stroke," distinct from tool selection. This is a
//     heuristic (we can't see CSP's actual canvas vs toolbar/panel areas
//     without a real CSP API), but works well in practice since most of
//     CSP's window is canvas.
//
// NOTE: WH_KEYBOARD_LL / WH_MOUSE_LL hooks must be installed from a thread
// that runs a standard Win32 message loop (GetMessage/DispatchMessage) --
// they are called back on that thread. We install from the main thread.
class InputWatcher {
public:
    using ToolChangeCallback = std::function<void(ToolState)>;
    using RightClickCallback = std::function<void(POINT)>;
    using DragStartCallback = std::function<void(POINT)>;
    using DragMoveCallback = std::function<void(int dx, int dy)>;
    using DragEndCallback = std::function<void()>;
    using DrawStartCallback = std::function<void()>;
    using DrawEndCallback = std::function<void()>;

    bool Install(HWND companionWnd);
    void Uninstall();

    void SetToolChangeCallback(ToolChangeCallback cb) { onToolChange_ = std::move(cb); }
    void SetRightClickCallback(RightClickCallback cb) { onRightClick_ = std::move(cb); }
    void SetDragStartCallback(DragStartCallback cb) { onDragStart_ = std::move(cb); }
    void SetDragMoveCallback(DragMoveCallback cb) { onDragMove_ = std::move(cb); }
    void SetDragEndCallback(DragEndCallback cb) { onDragEnd_ = std::move(cb); }
    void SetDrawStartCallback(DrawStartCallback cb) { onDrawStart_ = std::move(cb); }
    void SetDrawEndCallback(DrawEndCallback cb) { onDrawEnd_ = std::move(cb); }

    // Pauses/resumes tool-change reactions (used by the tray "Enabled" toggle).
    // Right-click menu, drag, and the CSP-foreground check keep working either way.
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

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

    bool dragging_ = false;
    POINT lastDragPoint_{};
    bool isPainting_ = false;
    bool enabled_ = true;

    ToolChangeCallback onToolChange_;
    RightClickCallback onRightClick_;
    DragStartCallback onDragStart_;
    DragMoveCallback onDragMove_;
    DragEndCallback onDragEnd_;
    DrawStartCallback onDrawStart_;
    DrawEndCallback onDrawEnd_;
};
