#include "AnimationController.h"
#include <filesystem>
#include <algorithm>

using namespace Gdiplus;
namespace fs = std::filesystem;

namespace {
// Distinct placeholder color per state so tool switching is visibly obvious
// during testing, before real sprite art is dropped in.
Color ColorForState(ToolState s) {
    switch (s) {
        case ToolState::Idle:      return Color(255, 200, 200, 200);
        case ToolState::Brush:     return Color(255, 90, 170, 250);
        case ToolState::Eraser:    return Color(255, 250, 120, 120);
        case ToolState::Selection: return Color(255, 250, 210, 90);
        case ToolState::Transform: return Color(255, 170, 120, 250);
        case ToolState::Fill:      return Color(255, 120, 220, 150);
        case ToolState::Zoom:      return Color(255, 250, 160, 220);
        case ToolState::Text:      return Color(255, 160, 160, 250);
        default:                   return Color(255, 180, 180, 180);
    }
}
}

bool AnimationController::LoadGifFrames(const std::wstring& path, std::vector<FrameData>& outFrames) {
    Bitmap gif(path.c_str());
    if (gif.GetLastStatus() != Ok) return false;

    UINT dimCount = gif.GetFrameDimensionsCount();
    if (dimCount == 0) return false;

    std::vector<GUID> dims(dimCount);
    gif.GetFrameDimensionsList(dims.data(), dimCount);

    UINT frameCount = gif.GetFrameCount(&dims[0]);
    if (frameCount == 0) return false;

    // PropertyTagFrameDelay: one UINT per frame, in 1/100-second units.
    UINT propSize = gif.GetPropertyItemSize(PropertyTagFrameDelay);
    std::vector<BYTE> propBuffer(propSize);
    UINT* delaysCenti = nullptr;
    if (propSize > 0) {
        auto* item = reinterpret_cast<PropertyItem*>(propBuffer.data());
        if (gif.GetPropertyItem(PropertyTagFrameDelay, propSize, item) == Ok) {
            delaysCenti = reinterpret_cast<UINT*>(item->value);
        }
    }

    for (UINT i = 0; i < frameCount; ++i) {
        gif.SelectActiveFrame(&dims[0], i);

        // GDI+ composites GIF disposal/overlay logic internally when you
        // select a frame, so this DrawImage call already gives the full,
        // correctly-composited frame; no manual patch-blitting needed.
        auto frameBmp = std::make_unique<Bitmap>(spriteWidth_, spriteHeight_, PixelFormat32bppPARGB);
        Graphics g(frameBmp.get());
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.Clear(Color(0, 0, 0, 0));
        g.DrawImage(&gif, Rect(0, 0, spriteWidth_, spriteHeight_));

        FrameData fd;
        fd.bitmap = std::move(frameBmp);
        // Guard against malformed/zero-delay GIF frames (some editors emit
        // 0, which browsers/viewers treat as "fast" rather than "instant").
        fd.delayMs = delaysCenti ? std::max<UINT>(delaysCenti[i] * 10, 20) : 100;
        outFrames.push_back(std::move(fd));
    }

    return true;
}

bool AnimationController::LoadAssets(const std::wstring& assetsDir, int spriteWidth, int spriteHeight) {
    spriteWidth_ = spriteWidth;
    spriteHeight_ = spriteHeight;

    const ToolState allStates[] = {
        ToolState::Idle, ToolState::Brush, ToolState::Eraser, ToolState::Selection,
        ToolState::Transform, ToolState::Fill, ToolState::Zoom, ToolState::Text
    };

    for (ToolState state : allStates) {
        std::wstring name = ToolStateName(state);
        std::wstring gifPath = assetsDir + L"\\" + name + L".gif";
        std::wstring pngDir = assetsDir + L"\\" + name;

        std::vector<FrameData> seq;

        if (fs::exists(gifPath)) {
            LoadGifFrames(gifPath, seq);
        } else if (fs::exists(pngDir) && fs::is_directory(pngDir)) {
            std::vector<fs::path> pngFiles;
            for (auto& entry : fs::directory_iterator(pngDir)) {
                if (entry.path().extension() == L".png") pngFiles.push_back(entry.path());
            }
            std::sort(pngFiles.begin(), pngFiles.end());

            for (auto& p : pngFiles) {
                auto bmp = std::make_unique<Bitmap>(p.wstring().c_str());
                if (bmp->GetLastStatus() == Ok) {
                    FrameData fd;
                    fd.bitmap = std::move(bmp);
                    fd.delayMs = 100;  // fixed; use a .gif instead if you need real timing
                    seq.push_back(std::move(fd));
                }
            }
        }

        if (seq.empty()) {
            GeneratePlaceholder(state);
        } else {
            frames_[state] = std::move(seq);
        }
    }

    return true;
}

void AnimationController::GeneratePlaceholder(ToolState state) {
    // Two-frame "breathing" placeholder: a soft circle that pulses size
    // slightly, so idle/active states are visually distinguishable even
    // with zero art assets.
    std::vector<FrameData> seq;
    Color c = ColorForState(state);

    for (int f = 0; f < 2; ++f) {
        auto bmp = std::make_unique<Bitmap>(spriteWidth_, spriteHeight_, PixelFormat32bppPARGB);
        Graphics g(bmp.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.Clear(Color(0, 0, 0, 0));

        int inset = (f == 0) ? 10 : 16;
        SolidBrush brush(c);
        g.FillEllipse(&brush, inset, inset, spriteWidth_ - inset * 2, spriteHeight_ - inset * 2);

        Pen outline(Color(180, 40, 40, 40), 2.0f);
        g.DrawEllipse(&outline, inset, inset, spriteWidth_ - inset * 2, spriteHeight_ - inset * 2);

        FrameData fd;
        fd.bitmap = std::move(bmp);
        fd.delayMs = 400;
        seq.push_back(std::move(fd));
    }
    frames_[state] = std::move(seq);
}

void AnimationController::SetState(ToolState state) {
    if (state == currentState_) return;
    currentState_ = state;
    frameIndex_ = 0;
}

void AnimationController::Tick() {
    auto it = frames_.find(currentState_);
    if (it == frames_.end() || it->second.empty()) return;
    frameIndex_ = (frameIndex_ + 1) % it->second.size();
}

Gdiplus::Bitmap* AnimationController::CurrentFrame() const {
    auto it = frames_.find(currentState_);
    if (it == frames_.end() || it->second.empty()) return nullptr;
    return it->second[frameIndex_ % it->second.size()].bitmap.get();
}

UINT AnimationController::CurrentFrameDelayMs() const {
    auto it = frames_.find(currentState_);
    if (it == frames_.end() || it->second.empty()) return 100;
    return it->second[frameIndex_ % it->second.size()].delayMs;
}
