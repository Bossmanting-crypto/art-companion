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
// Loading priority per state, tried in order:
//   1. assets/<StateName>.gif       -- a single animated GIF (recommended)
//   2. assets/<StateName>/*.png     -- a folder of numbered still frames
//   3. procedurally generated placeholder, so the app still runs/animates
//      before you've supplied any real art.
//
// Each state can ALSO have an optional "actively drawing" variant, tried
// the same way but named "<StateName>_Active" -- e.g. Brush_Active.gif.
// This plays instead of the normal idle animation while the mouse is held
// down over CSP's canvas with that tool selected (see SetDrawing()). If no
// "_Active" art exists for a state, it silently falls back to the normal
// idle animation for that state -- nothing breaks if you haven't made
// drawing-specific art yet.
class AnimationController {
public:
    bool LoadAssets(const std::wstring& assetsDir, int spriteWidth, int spriteHeight);

    void SetState(ToolState state);
    void SetDrawing(bool drawing);  // true while a stroke is actively being made
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
    // Loads a state's frames from assets/<name>.gif or assets/<name>/*.png,
    // trying both in that order. Returns false (leaves outFrames empty) if
    // neither exists -- caller decides what to do (placeholder vs. skip).
    bool LoadStateFrames(const std::wstring& assetsDir, const std::wstring& name,
                          std::vector<FrameData>& outFrames);
    const std::vector<FrameData>* ActiveSequence() const;

    std::unordered_map<ToolState, std::vector<FrameData>> frames_;
    std::unordered_map<ToolState, std::vector<FrameData>> activeFrames_;  // "_Active" variants, optional
    ToolState currentState_ = ToolState::Idle;
    bool isDrawing_ = false;
    size_t frameIndex_ = 0;
    int spriteWidth_ = 128;
    int spriteHeight_ = 128;
};
