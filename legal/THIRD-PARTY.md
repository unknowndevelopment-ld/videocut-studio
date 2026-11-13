# Third-party notices

VideoCut Studio is built with open-source software. This file lists the
components that are redistributed with the installer, their licences, and where
to obtain the corresponding source code.

## Qt 6 (Qt Widgets, Qt Multimedia, Qt Network)

- Copyright © The Qt Company Ltd. and contributors.
- Licence: **LGPL v3** (dynamic linking).
- The Qt libraries ship as separate DLLs in the install folder, so you may
  replace them with compatible builds.
- Source: <https://download.qt.io/official_releases/qt/6.8/>
- Licence text: <https://www.gnu.org/licenses/lgpl-3.0.html>

Qt is used unmodified. No Qt source is statically linked into
`VideoCutStudio.exe`.

## ffmpeg (ffmpeg.exe, ffprobe.exe)

- Copyright © the FFmpeg developers.
- Build: `ffmpeg 9.x essentials` from <https://www.gyan.dev/ffmpeg/builds/>,
  licence **GPL v3** (the build includes GPL components such as libx264).
- ffmpeg is executed as a separate program; it is not linked into the editor.
- Source for this build: <https://www.gyan.dev/ffmpeg/builds/#sources> and
  <https://git.ffmpeg.org/ffmpeg.git>
- Licence text: <https://www.gnu.org/licenses/gpl-3.0.html>
- Build configuration: <https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip>

## MinGW-w64 runtime

- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- Copyright © the GCC / MinGW-w64 projects.
- Licence: GPL v3 **with the GCC runtime library exception**, which permits
  redistribution in binary form without affecting the licence of the editor.

## Icons and fonts

All icons in the application are drawn at runtime with Qt's painting API — no
third-party icon set is bundled. Text is rendered with fonts already installed
on your system; no font files are redistributed.

## Julia / other

Nothing else is redistributed. The editor's own source is not covered by these
licences; see `TERMS.md`.

---

If you believe a component is missing from this list, please open an issue so it
can be corrected in the next release.
