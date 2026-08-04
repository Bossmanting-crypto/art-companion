#include "TransparentWindow.h"

using namespace Gdiplus;

namespace {
constexpr wchar_t kClassName[] = L"ArtCompanionWindowClass";
constexpr UINT ID_MENU_EXIT = 1001;
constexpr UINT ID_MENU_TOGGLE_PIN = 1002;
}

TransparentWindow::TransparentWindow() {
    GdiplusStartupInput input;
    GdiplusStartup(&gdiplusToken_, &input, nullptr);
}

TransparentWindow::~TransparentWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
    GdiplusShutdown(gdiplusToken_);
}

LRESULT CALLBACK TransparentWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_MENU_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool TransparentWindow::Create(HINSTANCE hInstance, int width, int height) {
    width_ = width;
    height_ = height;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // WS_EX_LAYERED    -> per-pixel alpha compositing via UpdateLayeredWindow
    // WS_EX_TRANSPARENT-> click-through: mouse input passes to the window below
    // WS_EX_TOOLWINDOW -> keeps it off the taskbar / alt-tab
    // WS_EX_TOPMOST    -> always floats above host app + other windows
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    hwnd_ = CreateWindowExW(
        exStyle,
        kClassName,
        L"ArtCompanion",
        WS_POPUP,
        100, 100, width_, height_,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd_) return false;

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void TransparentWindow::SetPosition(int x, int y) {
    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void TransparentWindow::GetPosition(int& x, int& y) const {
    RECT r;
    GetWindowRect(hwnd_, &r);
    x = r.left;
    y = r.top;
}

RECT TransparentWindow::GetBounds() const {
    RECT r;
    GetWindowRect(hwnd_, &r);
    return r;
}

void TransparentWindow::PresentFrame(Gdiplus::Bitmap* frame) {
    if (!frame) return;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    HBITMAP hBitmap = nullptr;
    frame->GetHBITMAP(Color(0, 0, 0, 0), &hBitmap);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    RECT wr;
    GetWindowRect(hwnd_, &wr);
    POINT ptSrc = { 0, 0 };
    POINT ptDst = { wr.left, wr.top };
    SIZE size = { width_, height_ };

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd_, screenDC, &ptDst, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void TransparentWindow::SetClickThrough(bool enabled) {
    LONG_PTR ex = GetWindowLongPtr(hwnd_, GWL_EXSTYLE);
    if (enabled) {
        ex |= WS_EX_TRANSPARENT;
    } else {
        ex &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd_, GWL_EXSTYLE, ex);
}

void TransparentWindow::ShowContextMenu(POINT screenPt) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_MENU_TOGGLE_PIN, L"Pin to top (always on)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_MENU_EXIT, L"Exit Companion");

    // Menu needs a foreground window to behave correctly and to dismiss
    // when clicking away.
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}
