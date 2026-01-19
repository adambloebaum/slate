# Slate

A blank slate for web browsing.

Slate is a minimal, privacy-focused web browser built with Qt and Chromium. No history. No bookmarks. No clutter. Just browsing.

![Browser Screenshot](/docs/screenshot.png)

## Features

- **Privacy First** - No persistent history, cookies, or cache. Permissions are denied by default.
- **Native Ad Blocking** - Built-in blocker for 100+ ad networks and trackers.
- **Minimal UI** - Clean interface with just tabs and an address bar.
- **Dark Mode** - Toggle between light and dark themes (default is dark).
- **Fast Start** - UI appears immediately while the engine spins up.
- **Smart Address Bar** - Session-only URL suggestions.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab |
| `Ctrl+L` | Focus address bar |
| `Ctrl+R` | Reload page |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |

## Build (Windows)

1. Set `QT_PATH` to your Qt install, for example:
   - PowerShell: `$env:QT_PATH = "C:\Qt\6.10.1\msvc2022_64"`
2. Configure and build:
   - `cmake -S . -B build -DCMAKE_PREFIX_PATH="$env:QT_PATH"`
   - `cmake --build build --config Release`
3. Run `build\Release\Slate.exe`

## Download

Download the latest release from the [Releases](../../releases) page.

(*Windows-only*)

1. Download `Slate-vX.X.X-windows.zip`
2. Extract the zip file
3. Run `Slate.exe`
