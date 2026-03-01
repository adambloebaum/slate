# Slate

A blank slate for web browsing.

Slate is a minimal, privacy-focused web browser built with Qt and Chromium. No history. No bookmarks. No clutter. Just browsing.

![Browser Screenshot](/docs/screenshot.png)

## Features

- **Privacy First** - No persistent history, cookies, or cache. Permissions denied by default with visible notifications.
- **HTTPS-Only** - All connections automatically upgraded to HTTPS.
- **Native Ad Blocking** - Built-in blocker for 100+ ad networks and trackers, with support for external blocklists.
- **Minimal UI** - Clean interface with just tabs and an address bar.
- **Dark Mode** - Toggle between light and dark themes (default is dark).
- **Fast Start** - UI appears immediately while the engine spins up.
- **Smart Address Bar** - Session-only URL suggestions.
- **Find in Page** - Search within pages with Ctrl+F.
- **Zoom Controls** - Ctrl+/- to zoom, Ctrl+0 to reset.
- **Favicon Tabs** - Site icons displayed in tab bar.
- **Fullscreen** - F11 for fullscreen, with HTML5 fullscreen video support.
- **Download Manager** - Progress bar and notifications for file downloads.
- **Print Support** - Print pages via Ctrl+P.
- **Developer Tools** - F12 to inspect pages.
- **Certificate Warnings** - Blocks untrusted connections with clear error pages.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab |
| `Ctrl+L` | Focus address bar |
| `Ctrl+R` | Reload page |
| `Ctrl+F` | Find in page |
| `Ctrl+P` | Print page |
| `Ctrl++` / `Ctrl+-` | Zoom in / out |
| `Ctrl+0` | Reset zoom |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `F11` | Toggle fullscreen |
| `F12` | Developer tools |
| `Esc` | Exit fullscreen / close find bar |

## Build (Windows)

1. Set `QT_PATH` to your Qt install, for example:
   - PowerShell: `$env:QT_PATH = "C:\Qt\6.10.1\msvc2022_64"`
2. Configure and build:
   - `cmake -S . -B build -DCMAKE_PREFIX_PATH="$env:QT_PATH"`
   - `cmake --build build --config Release`
3. Run `build\Release\Slate.exe`

## Custom Blocklists

Place a `blocklist.txt` file next to `Slate.exe` to load additional blocked domains. The file supports:
- One domain per line: `ads.example.com`
- Hosts file format: `0.0.0.0 ads.example.com`
- Comments with `#`

## Download

Download the latest release from the [Releases](../../releases) page.

(*Windows-only*)

1. Download `Slate-vX.X.X-windows.zip`
2. Extract the zip file
3. Run `Slate.exe`
