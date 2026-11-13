# VideoCut Studio

A free, offline, native video editor for Windows. C++ / Qt 6, real-time
composited preview, no account, no watermark, no upload.

**Repository:** https://github.com/unknowndevelopment-ld/videocut-studio

**Install:** run `VideoCutStudio-1.2.0-x64.msi` from `dist-cpp\` — it adds a Start
Menu entry, a desktop shortcut, file association for `.vcs` projects, an
"Apps & features" entry, and installs to `C:\Program Files\VideoCut Studio`.

**Run without installing:** `dist-cpp\VideoCutStudio\VideoCutStudio.exe`
(copy the whole folder if you move it — Qt and ffmpeg live beside the exe)

---

## What it does

**Timeline** — multiple video/audio tracks, drag to move, drag edges to trim,
snapping guides, split at playhead, duplicate, ripple delete, copy/paste,
multi-select, rubber band, zoom (`+` / `-` / `F`, `Ctrl`+wheel) and pan
(middle mouse).

**Clips** — speed 0.1×–8× (pitch-correct audio on export), volume/mute,
position, scale, rotation, opacity, contain/cover/stretch, fade in/out.

**Looks** — brightness, contrast, saturation, hue, blur, sharpen, vignette.

**Transitions** — right-click a clip → *Add transition* (or **Add → Transition
after selected clip**). Six types: dissolve, fade through black, and slide
left/right/up/down. A badge sits on the junction — click it to change the type
or remove it. They play in the preview and render into the export.

**Text** — overlays with any installed font (plus *Load font file…* for
`.ttf` / `.otf`), size, colour, outline, bold/italic, free positioning and
entrance animations: fade in, pop, slide left/right/up/down and typewriter
(character-by-character with a cursor). Animations render frame-for-frame in
the preview *and* in the export.

**Looks** — one-click colour presets: Cinematic, Vivid, Black & white, Warm,
Cool, Vintage. They just set the grading sliders, so you can tweak afterwards.

**Media** — MP4, MOV, MKV, WebM, AVI, WMV, MP3, WAV, FLAC, PNG, JPG, GIF…
with filmstrip thumbnails and audio waveforms.

**Preview** — real-time, fully composited playback. Each media clip has its own
decoder; frames are blended live (video, images, text, transforms, opacity,
fades, colour, vignette). There is no proxy render and no "rendering preview"
wait — press play and it goes. Blur and sharpen stay export-only, like most
NLE previews.

**Export** — MP4 (H.264), WebM (VP9), MKV, GIF; 480p → 4K plus vertical and
square; quality slider with size estimate, live progress, cancel.

**Projects** — `.vcs` JSON, undo/redo on everything, remembered window layout.

## Accessibility

- Every control has an accessible name/role; the timeline announces selection
  and operation results through `QAccessible` alert events.
- **Accessible clip list (`Alt+L`)** — a plain, keyboard- and screen-reader
  friendly list of every clip. Select rows to select clips, Enter to jump to
  one. Use it instead of the canvas if dragging is difficult.
- **High contrast mode (`Alt+H`)** — pure black with red accent and white
  borders.
- **Interface scaling (`Alt+Plus` / `Alt+Minus`)** — 80 %–160 %, applied live.
- Full keyboard operation, visible focus rings, logical tab order, and
  shortcuts that do not fire while typing in a text field.

## Performance

The preview is built to stay at 30 fps rather than look pretty and stutter:

- The engine renders at exactly the on-screen size (1:1 blit, no rescaling).
- Colour grading runs at preview resolution, not source resolution.
- Playhead movement repaints two thin strips of the timeline, not the whole
  timeline (filmstrips and waveforms are not redrawn 30×/second).
- Clip players are only re-seeked on large drift, never every half second.
- **View → Preview quality** — *Balanced* (960×540 cap, default), *Full
  resolution* (1280×720), or *Fast* (640×360) for older machines. Saved
  between sessions.

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| `Space` | Play / pause |
| `←` / `→` | Step a frame (`Shift` = one second) |
| `Home` / `End` | Start / end |
| `S` | Split at playhead |
| `Del` / `Shift+Del` | Delete / ripple delete |
| `Ctrl+C` `Ctrl+V` `Ctrl+X` `Ctrl+D` | Copy / paste / cut / duplicate |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+I` / `Ctrl+E` | Import / export |
| `T` | Add text |
| `N` | Snapping |
| `Alt+L` | Accessible clip list |
| `Alt+H` | High contrast |
| `Alt+Plus` / `Alt+Minus` | Interface size |
| `F1` | Shortcut help |

## Install

```
VideoCutStudio-<version>-x64.msi      ← double-click, accept the licence, done
```

The package installs per machine (UAC prompt), and adds:

- `C:\Program Files\VideoCut Studio\` with the editor, Qt runtime, ffmpeg and `legal\`
- Start Menu → VideoCut Studio, plus a desktop shortcut
- an **Apps & features** entry (icon, version, support link)
- `.vcs` file association and an `App Paths` entry, so `VideoCutStudio` works
  from Win+R / the Run dialog

Uninstall from **Settings → Apps**; your projects and settings are left alone.
Silent install: `msiexec /i VideoCutStudio-1.2.0-x64.msi /qn`.

## Releasing & updates

The app updates itself from GitHub Releases:

1. Bump `project(VideoCutStudio VERSION x.y.z)` in `cpp/CMakeLists.txt`.
2. Build the installer with the repository baked in:
   ```powershell
   .\package.ps1 -Repo "unknowndevelopment-ld/videocut-studio"
   ```
   That produces `dist-cpp\VideoCutStudio-<version>-x64.msi` and `update.json`
   (version, size, SHA-256).
3. Tag and push: `git tag v1.2.0; git push --tags`. The workflow in
   `.github/workflows/release.yml` builds on `windows-latest` and attaches the
   MSI + `update.json` to the release.
4. Installed copies check `https://api.github.com/repos/<repo>/releases/latest`
   nine seconds after launch (and from **Help → Check for updates**). A newer
   version offers a one-click download; the app then hands the MSI to
   `msiexec` and closes so the upgrade can finish.

No feed is configured until you pass `-Repo`, so a plain build never phones
home. **Code signing:** the MSI is unsigned by default — sign it with
`signtool sign /fd sha256 /tr <timestamp-url> /td sha256 <msi>` (and the exe)
with an EV/OV certificate before publishing, or SmartScreen will warn on first
run.

## Legal

- `legal/EULA.rtf` — shown in the installer before anything is copied
- `legal/TERMS.md` — Terms of Service
- `legal/PRIVACY.md` — no accounts, no telemetry, local-only processing
- `legal/THIRD-PARTY.md` — Qt (LGPL-3), ffmpeg (GPL-3), MinGW runtime

The same documents are installed in `Program Files\VideoCut Studio\legal` and
open from **Help → Terms of Service / Privacy Policy / Third-party notices**.

## Build it yourself

Requires CMake, Ninja, MinGW 13.1 and Qt 6.8 (Widgets + Multimedia):

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;$env:PATH"
cmake -S cpp -B cpp\build -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\mingw_64
cmake --build cpp\build
```

Deploy:

```powershell
C:\Qt\6.8.3\mingw_64\bin\windeployqt.exe --release --compiler-runtime `
    --dir dist-cpp\VideoCutStudio dist-cpp\VideoCutStudio\VideoCutStudio.exe
```

Smoke test (builds a demo, screenshots the UI, checks playback fps, exports):

```powershell
dist-cpp\VideoCutStudio\VideoCutStudio.exe --selftest --out dev_cpp
```

`--console` attaches a console window if you want to watch diagnostics.

## Source map

```
cpp/src/
  Project.h          clips, tracks, project JSON, undo history
  Theme.cpp          palette, stylesheet, vector icons
  MediaUtils.cpp     ffprobe, thumbnails, waveforms, frame grabs
  Renderer.cpp       ffmpeg filter graph + export with progress
  PreviewEngine.cpp  real-time compositor (per-clip decoders, no proxy render)
  PreviewPanel.cpp   preview surface + transport controls
  TimelineWidget.cpp custom-painted timeline, snapping, a11y announcements
  MediaBin.cpp       media library with drag-out
  PropertiesPanel.cpp inspector
  ExportDialog.cpp   export UI
  MainWindow.cpp     window, commands, preview orchestration, clip list
```

## Notes

- Decoding/encoding uses the bundled ffmpeg; preview decoding uses Qt
  Multimedia (hardware accelerated where the driver allows).
- The Python/PySide6 prototype has been removed; this is the only build.
- ffmpeg is distributed under the GPL/LGPL — see `vendor/ffmpeg`.
