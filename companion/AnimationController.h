#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "ToolMap.h"

// Holds one frame sequence per ToolState and advances playback on tick().
// Frames are loaded from assets/<StateName>/frame_XXXX.png if present;
// otherwise a placeholder is generated procedurally so the project runs
// and animates before you've supplied real art.
class AnimationController {
public:
    bool LoadAssets(const std::wstring& assetsDir, int spriteWidth, int spriteHeight);

    void SetState(ToolState state);
    void Tick();  // call on a timer, e.g. every 80-120ms

    Gdiplus::Bitmap* CurrentFrame() const;

    int Width() const { return spriteWidth_; }
    int Height() const { return spriteHeight_; }

private:
    void GeneratePlaceholder(ToolState state);

    std::unordered_map<ToolState, std::vector<std::unique_ptr<Gdiplus::Bitmap>>> frames_;
    ToolState currentState_ = ToolState::Idle;
    size_t frameIndex_ = 0;
    int spriteWidth_ = 128;
    int spriteHeight_ = 128;
};
