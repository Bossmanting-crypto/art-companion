#include "TransparentWindow.h"
#include <shellapi.h>

using namespace Gdiplus;

namespace {
constexpr wchar_t kClassName[] = L"ArtCompanionWindowClass";
constexpr UINT ID_MENU_EXIT = 1001;
constexpr UINT ID_MENU_ENABLED = 1002;
constexpr UINT ID_MENU_SIZE_SMALL = 1003;
constexpr UINT ID_MENU_SIZE_MEDIUM = 1004;
constexpr UINT ID_MENU_SIZE_LARGE = 1005;
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
}

TransparentWindow* TransparentWindow::instance_ = nullptr;

TransparentWindow::TransparentWindow() {
    GdiplusStartupInput input;
    GdiplusStartup(&gdiplusToken_, &input, nullptr);
}

TransparentWindow::~TransparentWindow() {
    RemoveTrayIcon();
    if (hwnd_) DestroyWindow(hwnd_);
    GdiplusShutdown(gdiplusToken_);
}

LRESULT CALLBACK TransparentWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                if (instance_) instance_->ShowTrayMenu(pt);
            }
            return 0;

        case TransparentWindow::kShowContextMenuMsg: {
            if (!instance_) return 0;
            POINT pt{ static_cast<LONG>(wParam), static_cast<LONG>(lParam) };
            instance_->SetClickThrough(false);
            instance_->ShowContextMenu(pt);
            instance_->SetClickThrough(true);
            return 0;
        }

        case WM_COMMAND: {
            if (!instance_) break;
            switch (LOWORD(wParam)) {
                case ID_MENU_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
                case ID_MENU_ENABLED:
                    if (instance_->isEnabledQuery_ && instance_->setEnabledCommand_) {
                        bool current = instance_->isEnabledQuery_();
                        instance_->setEnabledCommand_(!current);
                    }
                    return 0;
                case ID_MENU_SIZE_SMALL:
                    instance_->SetScale(0.75f);
                    return 0;
                case ID_MENU_SIZE_MEDIUM:
                    instance_->SetScale(1.0f);
                    return 0;
                case ID_MENU_SIZE_LARGE:
                    instance_->SetScale(1.5f);
                    return 0;
            }
            break;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool TransparentWindow::Create(HINSTANCE hInstance, int width, int height) {
    baseWidth_ = width;
    baseHeight_ = height;
    instance_ = this;

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
        100, 100, baseWidth_, baseHeight_,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd_) return false;

    CreateTrayIcon();
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

void TransparentWindow::ApplyWindowSize() {
    int w = static_cast<int>(baseWidth_ * scale_);
    int h = static_cast<int>(baseHeight_ * scale_);
    SetWindowPos(hwnd_, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void TransparentWindow::SetScale(float scale) {
    scale_ = scale;
    ApplyWindowSize();
}

void TransparentWindow::PresentFrame(Gdiplus::Bitmap* frame) {
    if (!frame) return;

    int displayW = static_cast<int>(baseWidth_ * scale_);
    int displayH = static_cast<int>(baseHeight_ * scale_);

    // Scale the source frame into a buffer matching the current on-screen
    // size, so resizing via the tray/context menu doesn't require reloading
    // or re-decoding any art -- we just stretch at composite time.
    Bitmap scaled(displayW, displayH, PixelFormat32bppPARGB);
    {
        Graphics g(&scaled);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.Clear(Color(0, 0, 0, 0));
        g.DrawImage(frame, Rect(0, 0, displayW, displayH));
    }

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    HBITMAP hBitmap = nullptr;
    scaled.GetHBITMAP(Color(0, 0, 0, 0), &hBitmap);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    RECT wr;
    GetWindowRect(hwnd_, &wr);
    POINT ptSrc = { 0, 0 };
    POINT ptDst = { wr.left, wr.top };
    SIZE size = { displayW, displayH };

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

    bool enabled = isEnabledQuery_ ? isEnabledQuery_() : true;
    AppendMenuW(menu, MF_STRING | (enabled ? MF_CHECKED : 0), ID_MENU_ENABLED, L"Enabled");

    HMENU sizeMenu = CreatePopupMenu();
    AppendMenuW(sizeMenu, MF_STRING | (scale_ == 0.75f ? MF_CHECKED : 0), ID_MENU_SIZE_SMALL, L"Small");
    AppendMenuW(sizeMenu, MF_STRING | (scale_ == 1.0f ? MF_CHECKED : 0), ID_MENU_SIZE_MEDIUM, L"Medium");
    AppendMenuW(sizeMenu, MF_STRING | (scale_ == 1.5f ? MF_CHECKED : 0), ID_MENU_SIZE_LARGE, L"Large");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sizeMenu), L"Size");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_MENU_EXIT, L"Close ArtCompanion");

    // Menu needs a foreground window to behave correctly and to dismiss
    // when clicking away.
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

bool TransparentWindow::CreateTrayIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"ArtCompanion");

    trayIconAdded_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    return trayIconAdded_;
}

void TransparentWindow::RemoveTrayIcon() {
    if (!trayIconAdded_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayIconAdded_ = false;
}

void TransparentWindow::ShowTrayMenu(POINT screenPt) {
    // Same menu as right-clicking the sprite -- the tray icon is just a
    // second, always-reachable entry point to the same settings.
    ShowContextMenu(screenPt);
}
