#pragma once
#include <string>
#include <unordered_map>

// Logical companion states. Extend this as you add real animations.
enum class ToolState {
    Idle,
    Brush,
    Eraser,
    Selection,
    Transform,
    Fill,
    Zoom,
    Text,
    Unknown
};

inline const wchar_t* ToolStateName(ToolState s) {
    switch (s) {
        case ToolState::Idle:      return L"Idle";
        case ToolState::Brush:     return L"Brush";
        case ToolState::Eraser:    return L"Eraser";
        case ToolState::Selection: return L"Selection";
        case ToolState::Transform: return L"Transform";
        case ToolState::Fill:      return L"Fill";
        case ToolState::Zoom:      return L"Zoom";
        case ToolState::Text:      return L"Text";
        default:                   return L"Unknown";
    }
}

// Maps a single virtual-key press (no modifiers) to a ToolState.
// IMPORTANT: these are CSP's *default* single-key tool shortcuts as of the
// current SDK-supported (Japanese) build. Users can and do rebind shortcuts
// (Ctrl+K in CSP's shortcut settings), so ship this as an editable config
// -- see toolmap.ini loading in InputWatcher.cpp -- rather than trusting
// these hardcoded defaults for every user.
inline const std::unordered_map<UINT, ToolState>& DefaultKeyToolMap() {
    static const std::unordered_map<UINT, ToolState> map = {
        { 'B', ToolState::Brush },      // Pencil/Brush
        { 'E', ToolState::Eraser },     // Eraser
        { 'M', ToolState::Selection },  // Selection (Marquee-ish)
        { 'T', ToolState::Transform },  // Transform / Text depending on context
        { 'G', ToolState::Fill },       // Fill (Bucket)
        { 'Z', ToolState::Zoom },       // Zoom
    };
    return map;
}

// The process image name checked by InputWatcher's foreground check and by
// main.cpp's "is CSP open" watchdog. IMPORTANT: CELSYS ships two separate
// programs -- the "CLIP STUDIO" account/launcher hub, and "CLIP STUDIO
// PAINT" itself (the actual drawing app). This must point at the Paint
// app's exe specifically, not the hub, or the companion will react to the
// launcher being open rather than an actual canvas being open.
// Verify the exact filename on your machine: open Task Manager while CSP is
// running, go to the Details tab, find the Paint app's row (its description
// should mention "CLIP STUDIO PAINT", not just "CLIP STUDIO"), and confirm
// the "Name" column matches this string exactly.
inline const wchar_t* kTargetProcessName = L"CLIPStudioPaint.exe";
