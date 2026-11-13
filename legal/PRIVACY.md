# VideoCut Studio — Privacy Policy

**Version 1.0 · Effective 12 November 2026**

Short version: **your footage never leaves your computer.** The editor has no
account, no analytics, no telemetry and no cloud processing.

## What we collect

**Nothing.** There is no sign-up, no user ID, no usage statistics, no crash
upload, no advertising identifier.

## Where your files go

Everything you import, edit and export is read and written on your own machine.
Video frames are decoded locally, and export is performed by the bundled ffmpeg
process running on your computer.

The editor writes these files under `%LOCALAPPDATA%\VideoCutStudio`:

| Folder | Contents |
| --- | --- |
| `cache` | thumbnails, waveforms, temporary text frames and preview proxies |
| `VideoCutStudio.ini` (registry) | window layout, recent folders, UI scale, preferences |

Delete that folder at any time; the app recreates what it needs. Uninstalling
removes the program files but deliberately leaves your projects and settings.

## Network access

The only network request the app can make is an **update check**:

- It asks `api.github.com` for the latest release of the project repository.
- The request contains no personal data — just a standard HTTP request and a
  `VideoCutStudio/<version>` user agent.
- GitHub will see your IP address, exactly as it would if you visited the site.
  See GitHub's privacy statement for how they handle that.
- The check is skipped entirely if no update feed is configured, and you can
  avoid it by not using *Help → Check for updates*.

Nothing is uploaded during the check: no footage, no project data, no
identifiers, no machine information.

## Fonts

If you load a font file (`.ttf` / `.otf`), the path is remembered in the app's
settings so it can be re-registered at startup. The file itself is not copied.

## Children

The app is not directed at children and collects no data from anyone.

## Changes

If a future version ever collects anything, this document will say so before
that version ships, and the change will be called out in the release notes.

## Contact

Open an issue on the project's GitHub repository.
