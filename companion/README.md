# ArtCompanion

A transparent, click-through desktop mascot that watches for CSP tool-switch
keystrokes (via a global low-level keyboard hook) and the foreground window
(to confirm CSP is active), and reacts with per-tool animation states.

This does **not** use CSP's plugin SDK — it works entirely from outside the
app, which means it also works on international (non-Japanese) CSP builds,
unlike a true SDK plugin.

## Why this can't be compiled in a sandbox / needs your Windows machine

This targets the native Win32 API (layered windows, `UpdateLayeredWindow`,
`WH_KEYBOARD_LL`/`WH_MOUSE_LL` hooks, GDI+) — there's no cross-compiling
this meaningfully outside Windows; build it directly on the machine you'll
run it on.

## Build option A — Visual Studio (recommended)

1. Install **Visual Studio 2022** (Community is fine) with the
   "Desktop development with C++" workload.
2. Install **CMake** if not already present (VS's installer can add it, or
   grab it from cmake.org).
3. From a "x64 Native Tools Command Prompt for VS 2022":

   ```bat
   cd C:\Users\user\Downloads\companion
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```

4. The compiled binary lands at `build\Release\ArtCompanion.exe`.

## Build option B — MSYS2 / mingw-w64

1. Install [MSYS2](https://www.msys2.org/), then in the MSYS2 UCRT64 shell:

   ```bash
   pacman -S mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake
   ```

2. From the same shell:

   ```bash
   cd /path/to/companion
   cmake -B build -G "Ninja"
   cmake --build build
   ```

## Running it

1. Copy `toolmap.ini` next to `ArtCompanion.exe` (CMake doesn't do this
   automatically yet — add a `file(COPY ...)` step or just copy manually
   for now).
2. Create an `assets\` folder next to the exe if you want real sprite art
   (see layout below). Without it, the app still runs using procedurally
   drawn placeholder circles per state, so you can verify the whole
   pipeline (hook detection → state change → render) before any art exists.
3. Launch CSP, then launch `ArtCompanion.exe`. Switch tools in CSP with
   `B`, `E`, `M`, `T`, `G`, `Z` and the companion should change color/state.
4. Right-click the companion for the exit menu.

## Asset layout (once you have real sprite frames)

```
assets/
  Idle/       frame_0001.png, frame_0002.png, ...
  Brush/      frame_0001.png, frame_0002.png, ...
  Eraser/     ...
  Selection/  ...
  Transform/  ...
  Fill/       ...
  Zoom/       ...
  Text/       ...
```

Each PNG should be the same dimensions (128x128 by default, matches
`kSpriteSize` in `main.cpp`) with a transparent (alpha) background — GDI+
loads these directly and `UpdateLayeredWindow` composites the alpha channel
for you, no chroma-keying needed.

## Known gaps / next steps

- **Verify `kTargetProcessName` in `ToolMap.h`** against your actual CSP
  install (Task Manager → Details tab → exact .exe filename) — this can
  vary slightly by version/edition.
- **Verify default single-key shortcuts** against your CSP's actual
  Shortcut Settings; CSP's Japanese build may differ from what's assumed
  here, and users can rebind keys anyway — that's what `toolmap.ini` is for.
- **Elevation mismatch**: if CSP runs elevated (as admin) and ArtCompanion
  doesn't, `SetWindowsHookEx` won't see its keystrokes (Windows blocks
  lower-privilege hooks from observing higher-privilege input). Run both
  at the same elevation level.
- **Multi-key/modifier shortcuts** aren't handled yet — only bare single-key
  presses. Extend `InputWatcher`'s key matching if you need combos.
- **Dragging the companion** isn't implemented — add `WM_NCHITTEST`-style
  handling or a modifier-drag gesture, remembering it needs to work despite
  `WS_EX_TRANSPARENT` being on most of the time (same trick as the
  right-click menu: toggle it off for the duration of the drag).
- Currently a single hardcoded corner position — persist position between
  runs once dragging exists.
