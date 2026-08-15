#include "InputWatcher.h"
#include <psapi.h>
#include <fstream>
#include <sstream>
#pragma comment(lib, "psapi.lib")

InputWatcher* InputWatcher::instance_ = nullptr;

bool InputWatcher::Install(HWND companionWnd) {
    companionWnd_ = companionWnd;
    keyMap_ = DefaultKeyToolMap();
    instance_ = this;

    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);

    return keyboardHook_ != nullptr && mouseHook_ != nullptr;
}

void InputWatcher::Uninstall() {
    if (keyboardHook_) UnhookWindowsHookEx(keyboardHook_);
    if (mouseHook_) UnhookWindowsHookEx(mouseHook_);
    keyboardHook_ = nullptr;
    mouseHook_ = nullptr;
    instance_ = nullptr;
}

void InputWatcher::LoadToolMapConfig(const std::wstring& iniPath) {
    std::wifstream file(iniPath);
    if (!file.is_open()) return;  // fine -- defaults already loaded

    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == L'#' || line[0] == L'[') continue;
        auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;

        std::wstring key = line.substr(0, eq);
        std::wstring val = line.substr(eq + 1);
        if (key.empty()) continue;

        UINT vk = static_cast<UINT>(towupper(key[0]));

        ToolState state = ToolState::Unknown;
        if (val == L"Brush") state = ToolState::Brush;
        else if (val == L"Eraser") state = ToolState::Eraser;
        else if (val == L"Selection") state = ToolState::Selection;
        else if (val == L"Transform") state = ToolState::Transform;
        else if (val == L"Fill") state = ToolState::Fill;
        else if (val == L"Zoom") state = ToolState::Zoom;
        else if (val == L"Text") state = ToolState::Text;

        if (state != ToolState::Unknown) {
            keyMap_[vk] = state;
        }
    }
}

bool InputWatcher::IsForegroundTargetProcess() const {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
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

LRESULT CALLBACK InputWatcher::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && instance_ && instance_->enabled_ && wParam == WM_KEYDOWN) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // Ignore combos (Ctrl/Alt held) -- CSP's single-key tool shortcuts
        // are unmodified presses; modified combos are usually other commands.
        bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        if (!ctrlDown && !altDown && instance_->IsForegroundTargetProcess()) {
            auto it = instance_->keyMap_.find(kb->vkCode);
            if (it != instance_->keyMap_.end() && it->second != instance_->lastState_) {
                instance_->lastState_ = it->second;
                if (instance_->onToolChange_) instance_->onToolChange_(it->second);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK InputWatcher::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && instance_) {
        auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        POINT pt = ms->pt;

        if (wParam == WM_RBUTTONDOWN) {
            RECT bounds;
            GetWindowRect(instance_->companionWnd_, &bounds);
            if (PtInRect(&bounds, pt) && instance_->onRightClick_) {
                instance_->onRightClick_(pt);
            }
        } else if (wParam == WM_LBUTTONDOWN) {
            RECT bounds;
            GetWindowRect(instance_->companionWnd_, &bounds);
            if (PtInRect(&bounds, pt) && !instance_->dragging_) {
                // Click landed on the sprite itself -- drag-to-reposition.
                instance_->dragging_ = true;
                instance_->lastDragPoint_ = pt;
                if (instance_->onDragStart_) instance_->onDragStart_(pt);
            } else if (!PtInRect(&bounds, pt) && !instance_->isPainting_
                       && instance_->IsForegroundTargetProcess()) {
                // Click landed elsewhere while CSP is the active app --
                // treat as "starting a brush stroke."
                instance_->isPainting_ = true;
                if (instance_->onDrawStart_) instance_->onDrawStart_();
            }
        } else if (wParam == WM_MOUSEMOVE && instance_->dragging_) {
            int dx = pt.x - instance_->lastDragPoint_.x;
            int dy = pt.y - instance_->lastDragPoint_.y;
            instance_->lastDragPoint_ = pt;
            if ((dx != 0 || dy != 0) && instance_->onDragMove_) {
                instance_->onDragMove_(dx, dy);
            }
        } else if (wParam == WM_LBUTTONUP) {
            if (instance_->dragging_) {
                instance_->dragging_ = false;
                if (instance_->onDragEnd_) instance_->onDragEnd_();
            } else if (instance_->isPainting_) {
                instance_->isPainting_ = false;
                if (instance_->onDrawEnd_) instance_->onDrawEnd_();
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
