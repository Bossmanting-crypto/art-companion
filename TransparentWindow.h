#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>   // must come before gdiplus.h, see note below
                       // WIN32_LEAN_AND_MEAN strips the OLE headers that
                       // declare IStream/PROPID, which GDI+'s own headers
                       // assume are already available.
#include <gdiplus.h>
#include <memory>
#include <functional>
#include "ToolMap.h"

// Owns the borderless, layered, click-through companion window, its tray
// icon, and handles compositing the current animation frame with
// UpdateLayeredWindow. Also owns right-click menu (Close/Resize) and
// left-click-drag repositioning, both routed in from InputWatcher's global
// mouse hook since the window is click-through most of the time.
class TransparentWindow {
public:
    TransparentWindow();
    ~TransparentWindow();

    bool Create(HINSTANCE hInstance, int width, int height);
    void SetPosition(int x, int y);
    void GetPosition(int& x, int& y) const;
    RECT GetBounds() const;

    // Renders `frame` (premultiplied-alpha ARGB bitmap) to the layered window,
    // stretched to the current scale set via SetScale().
    void PresentFrame(Gdiplus::Bitmap* frame);

    // Toggles WS_EX_TRANSPARENT off/on so we can catch clicks for the
    // context menu or a drag, then immediately restore click-through.
    void SetClickThrough(bool enabled);

    // Resizes the on-screen sprite. 1.0 = the size passed to Create().
    void SetScale(float scale);
    float Scale() const { return scale_; }

    void ShowContextMenu(POINT screenPt);

    // Tray icon (Shell_NotifyIcon), doubles as the app's on/off + settings
    // surface without needing a separate settings window.
    bool CreateTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu(POINT screenPt);

    // Wired to the "Enabled" tray/context-menu checkbox; caller (main.cpp)
    // supplies the getter/setter since InputWatcher owns the actual flag.
    void SetEnabledQuery(std::function<bool()> getter) { isEnabledQuery_ = std::move(getter); }
    void SetEnabledCommand(std::function<void(bool)> setter) { setEnabledCommand_ = std::move(setter); }

    HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void ApplyWindowSize();

    HWND hwnd_ = nullptr;
    int baseWidth_ = 0;
    int baseHeight_ = 0;
    float scale_ = 1.0f;
    ULONG_PTR gdiplusToken_ = 0;
    bool trayIconAdded_ = false;

    std::function<bool()> isEnabledQuery_;
    std::function<void(bool)> setEnabledCommand_;

    static TransparentWindow* instance_;  // single companion window per process
};
