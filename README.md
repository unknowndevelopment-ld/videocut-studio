# VideoCut Studio

A free, offline, native video editor for Windows. C++ / Qt 6, real-time
composited preview, no account, no watermark, no upload.

**Repository:** https://github.com/unknowndevelopment-ld/videocut-studio

**Install:** run `VideoCutStudio-Setup-1.4.2.exe` â€” a single branded installer
with no wizard and no MSI. It installs per-user (no UAC prompt) and adds the
Start Menu entry, desktop shortcut, `.vcs` association and an "Apps & features"
entry.

**Run without installing:** `dist-cpp\VideoCutStudio\VideoCutStudio.exe`
(copy the whole folder if you move it â€” Qt and ffmpeg live beside the exe)

---

## What it does

**Timeline** â€” multiple video/audio tracks, drag to move, drag edges to trim,
snapping guides, split at playhead, duplicate, ripple delete, copy/paste,
multi-select, rubber band, zoom (`+` / `-` / `F`, `Ctrl`+wheel) and pan
(middle mouse).

**Clips** â€” speed 0.1Ã—â€“8Ã— (pitch-correct audio on export), volume/mute,
position, scale, rotation, opacity, contain/cover/stretch, fade in/out.

**Looks** â€” brightness, contrast, saturation, hue, blur, sharpen, vignette.

**Transitions** â€” right-click a clip â†’ *Add transition* (or **Add â†’ Transition
after selected clip**). Six types: dissolve, fade through black, and slide
left/right/up/down. A badge sits on the junction â€” click it to change the type
or remove it. They play in the preview and render into the export.

**Text** â€” overlays with any installed font (plus *Load font fileâ€¦* for
`.ttf` / `.otf`), size, colour, outline, bold/italic, free positioning and
entrance animations: fade in, pop, slide left/right/up/down and typewriter
(character-by-character with a cursor). Animations render frame-for-frame in
the preview *and* in the export.

**Shapes & annotations** â€” arrows, circles, rectangles, lines, triangles,
stars, check marks and crosses. Pick one from **Shapes** in the toolbar (or
**A** for a quick arrow). Drag/scale/rotate them like any clip, then set the
colour, thickness, fill and a dark readable edge â€” great for pointing at
something in a screen recording. They render in the preview and the export.

**Looks** â€” one-click colour presets: Cinematic, Vivid, Black & white, Warm,
Cool, Vintage. They just set the grading sliders, so you can tweak afterwards.

**Media** â€” MP4, MOV, MKV, WebM, AVI, WMV, MP3, WAV, FLAC, PNG, JPG, GIFâ€¦
with filmstrip thumbnails and audio waveforms.

**Preview** â€” real-time, fully composited playback. Each media clip has its own
decoder; frames are blended live (video, images, text, transforms, opacity,
fades, colour, vignette). There is no proxy render and no "rendering preview"
wait â€” press play and it goes. Blur and sharpen stay export-only, like most
NLE previews.

**Direct manipulation** - click any video, image, text or shape right in the
preview to select it. Drag the body to move it, drag a corner handle to scale,
or drag the round handle above the box to rotate (`Shift` snaps to 15 degrees).
The properties panel follows along live, and every gesture is one undo step.

**Export** â€” MP4 (H.264), WebM (VP9), MKV, GIF; 480p â†’ 4K plus vertical and
square; quality slider with size estimate, live progress, cancel.

**Fast export startup** â€” each media file is opened once and fanned out to all
clips that use it, text intro frames are rendered only for the animation, and
ffmpeg is warmed in the background at launch, so the wait before the progress
bar moves is short even on a long timeline.

**Projects** â€” `.vcs` JSON, undo/redo on everything, remembered window layout.

## Accessibility

- Every control has an accessible name/role; the timeline announces selection
  and operation results through `QAccessible` alert events.
- **Accessible clip list (`Alt+L`)** â€” a plain, keyboard- and screen-reader
  friendly list of every clip. Select rows to select clips, Enter to jump to
  one. Use it instead of the canvas if dragging is difficult.
- **High contrast mode (`Alt+H`)** â€” pure black with red accent and white
  borders.
- **Interface scaling (`Alt+Plus` / `Alt+Minus`)** â€” 80 %â€“160 %, applied live.
- Full keyboard operation, visible focus rings, logical tab order, and
  shortcuts that do not fire while typing in a text field.

## Performance

The preview is built to stay at 30 fps rather than look pretty and stutter:

- The engine renders at exactly the on-screen size (1:1 blit, no rescaling).
- Colour grading runs at preview resolution, not source resolution.
- Playhead movement repaints two thin strips of the timeline, not the whole
  timeline (filmstrips and waveforms are not redrawn 30Ã—/second).
- Clip players are only re-seeked on large drift, never every half second.
- **View â†’ Preview quality** â€” *Balanced* (960Ã—540 cap, default), *Full
  resolution* (1280Ã—720), or *Fast* (640Ã—360) for older machines. Saved
  between sessions.

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| `Space` | Play / pause |
| `â†` / `â†’` | Step a frame (`Shift` = one second) |
| `Home` / `End` | Start / end |
| `S` | Split at playhead |
| `Del` / `Shift+Del` | Delete / ripple delete |
| `Ctrl+C` `Ctrl+V` `Ctrl+X` `Ctrl+D` | Copy / paste / cut / duplicate |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+I` / `Ctrl+E` | Import / export |
| `T` | Add text |
| `A` | Add arrow (all shapes are in the **Shapes** menu) |
| `N` | Snapping |
| `Alt+L` | Accessible clip list |
| `Alt+H` | High contrast |
| `Alt+Plus` / `Alt+Minus` | Interface size |
| `F1` | Shortcut help |

## Install

```
VideoCutStudio-Setup-<version>.exe     â† double-click, accept the terms, Install
```

A custom installer, not an MSI: one dark window with the app's branding, a
progress bar, and a "Launch" button when it's done. Details:

- **Per-user** install to `%LOCALAPPDATA%\Programs\VideoCut Studio` â€” **no UAC**,
  no admin rights.
- Installs like a normal Windows app: a **Start Menu folder** with the launcher
  and "Uninstall VideoCut Studio", an optional desktop shortcut, a `.vcs` file
  association, "Edit with VideoCut Studio" in the right-click menu for videos,
  `AppPaths`, Default-apps registration and an **Apps & features** entry.
- Silent/upgrade mode: `VideoCutStudio-Setup-1.4.2.exe --silent` (used by the 
  built-in updater).
- Uninstall from **Settings -> Apps**, the Start Menu entry, or
  `Uninstall.exe /S`. Shortcuts, registry entries and the install folder are
  removed; your projects and settings are left alone.
- The installer carries everything: Qt runtime, ffmpeg and the legal docs.

## Releasing & updates

The app updates itself from GitHub Releases:

1. Bump `project(VideoCutStudio VERSION x.y.z)` in `cpp/CMakeLists.txt`.
2. Build the installer with the repository baked in:
   ```powershell
   .\package.ps1 -Repo "unknowndevelopment-ld/videocut-studio"
   ```
   That produces `dist-cpp\VideoCutStudio-Setup-<version>.exe` and `update.json`
   (version, size, SHA-256).
3. Tag and push: `git tag v1.4.2; git push --tags`. The workflow in 
   `.github/workflows/release.yml` builds on `windows-latest` and attaches the
   installer + `update.json` to the release.
4. Installed copies check `https://api.github.com/repos/<repo>/releases/latest`
   nine seconds after launch (and from **Help â†’ Check for updates**). A newer
   version offers a one-click download, then the app closes and the Setup.exe
   runs in `--silent` mode to install it.

No feed is configured until you pass `-Repo`, so a plain build never phones
home. **Code signing:** the installer is unsigned by default â€” sign it with
`signtool sign /fd sha256 /tr <timestamp-url> /td sha256 <exe>` (the Setup.exe
and the app exe inside) with an EV/OV certificate before publishing, or
SmartScreen will warn on first run.

## Legal

- `legal/EULA.rtf` â€” shown in the installer before anything is copied
- `legal/TERMS.md` â€” Terms of Service
- `legal/PRIVACY.md` â€” no accounts, no telemetry, local-only processing
- `legal/THIRD-PARTY.md` â€” Qt (LGPL-3), ffmpeg (GPL-3), MinGW runtime

The same documents are installed in `%LOCALAPPDATA%\Programs\VideoCut Studio\legal` and
open from **Help â†’ Terms of Service / Privacy Policy / Third-party notices**.

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
- ffmpeg is distributed under the GPL/LGPL â€” see `vendor/ffmpeg`.
