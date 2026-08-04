#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

// Owns the borderless, layered, click-through companion window and
// handles compositing the current animation frame with UpdateLayeredWindow.
class TransparentWindow {
public:
    TransparentWindow();
    ~TransparentWindow();

    bool Create(HINSTANCE hInstance, int width, int height);
    void SetPosition(int x, int y);
    void GetPosition(int& x, int& y) const;
    RECT GetBounds() const;

    // Renders `frame` (premultiplied-alpha ARGB bitmap) to the layered window.
    void PresentFrame(Gdiplus::Bitmap* frame);

    // Toggles WS_EX_TRANSPARENT off/on so we can catch a right-click for
    // the context menu, then immediately restore click-through behavior.
    void SetClickThrough(bool enabled);

    void ShowContextMenu(POINT screenPt);

    HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    ULONG_PTR gdiplusToken_ = 0;
};
