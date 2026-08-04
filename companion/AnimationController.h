#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>   // must come before gdiplus.h, see TransparentWindow.h
#include <gdiplus.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "ToolMap.h"

// Holds one frame sequence per ToolState and advances playback on tick().
// Loading priority per state:
//   1. assets/<StateName>.gif       -- a single animated GIF (recommended)
//   2. assets/<StateName>/*.png     -- a folder of numbered still frames
//   3. procedurally generated placeholder, so the app still runs/animates
//      before you've supplied any real art.
class AnimationController {
public:
    bool LoadAssets(const std::wstring& assetsDir, int spriteWidth, int spriteHeight);

    void SetState(ToolState state);
    void Tick();  // call on a timer; interval should be CurrentFrameDelayMs()

    Gdiplus::Bitmap* CurrentFrame() const;
    UINT CurrentFrameDelayMs() const;  // per-frame delay, taken from the GIF itself

    int Width() const { return spriteWidth_; }
    int Height() const { return spriteHeight_; }

private:
    struct FrameData {
        std::unique_ptr<Gdiplus::Bitmap> bitmap;
        UINT delayMs = 100;
    };

    void GeneratePlaceholder(ToolState state);
    // Decodes every frame of an animated GIF (scaled to spriteWidth_/spriteHeight_)
    // plus each frame's real delay, taken straight from the GIF's own timing.
    bool LoadGifFrames(const std::wstring& path, std::vector<FrameData>& outFrames);

    std::unordered_map<ToolState, std::vector<FrameData>> frames_;
    ToolState currentState_ = ToolState::Idle;
    size_t frameIndex_ = 0;
    int spriteWidth_ = 128;
    int spriteHeight_ = 128;
};
