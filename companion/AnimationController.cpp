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

bool AnimationController::LoadAssets(const std::wstring& assetsDir, int spriteWidth, int spriteHeight) {
    spriteWidth_ = spriteWidth;
    spriteHeight_ = spriteHeight;

    const ToolState allStates[] = {
        ToolState::Idle, ToolState::Brush, ToolState::Eraser, ToolState::Selection,
        ToolState::Transform, ToolState::Fill, ToolState::Zoom, ToolState::Text
    };

    for (ToolState state : allStates) {
        std::wstring stateDir = assetsDir + L"\\" + ToolStateName(state);
        std::vector<std::unique_ptr<Bitmap>> seq;

        if (fs::exists(stateDir) && fs::is_directory(stateDir)) {
            std::vector<fs::path> pngFiles;
            for (auto& entry : fs::directory_iterator(stateDir)) {
                if (entry.path().extension() == L".png") pngFiles.push_back(entry.path());
            }
            std::sort(pngFiles.begin(), pngFiles.end());

            for (auto& p : pngFiles) {
                auto bmp = std::make_unique<Bitmap>(p.wstring().c_str());
                if (bmp->GetLastStatus() == Ok) {
                    seq.push_back(std::move(bmp));
                }
            }
        }

        if (seq.empty()) {
            // No art provided yet for this state -- fall back to a
            // procedurally drawn placeholder so the app is testable.
            frames_[state] = {};
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
    std::vector<std::unique_ptr<Bitmap>> seq;
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

        seq.push_back(std::move(bmp));
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
    return it->second[frameIndex_ % it->second.size()].get();
}
