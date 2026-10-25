# Better Alt-Tab

A Windows utility that provides monitor-specific Alt+Tab switching.

It detects the monitor under the mouse cursor, shows windows belonging to that monitor, includes minimized applications, and lets you switch using a custom visual preview grid.

## Features

- Per-monitor Alt+Tab window filtering
- Immediate first Alt+Tab switching
- Hold Alt and press Tab to open the visual switcher
- Preview cards with application names and icons
- Four cards per row and up to three rows per page
- Minimized windows are included and restored when selected
- Rounded switcher and preview cards
- Automatic keyboard-hook recovery after sleep or wake
- Optional automatic startup when you sign in to Windows

## Download

### Source download

1. Open the repository on GitHub:
   https://github.com/parikshit-menon/better-alt-tab
2. Select **Code**.
3. Select **Download ZIP**.
4. Extract the ZIP to a permanent folder, such as:

```text
Documents\Projects\better-alt-tab
```

Keep the folder in the same location after configuring startup.

> A packaged `.exe` release is not published yet. Until then, the program must be compiled locally.

## Requirements

- Windows 10 or Windows 11
- MinGW-w64 with `g++`
- PowerShell

Check whether the compiler is installed:

```powershell
g++ --version
```

If PowerShell says that `g++` is not recognized, install MinGW-w64 and add its `bin` folder to your PATH.

## Build

Open PowerShell in the folder containing `main.cpp):

```powershell
g++ -std=c++17 -mwindows main.cpp -o BetterAltTab.exe -luser32 -lgdi32 -lpsapi
```

If an older copy is running, stop it first:

```powershell
taskkill /IM BetterAltTab.exe /F
```

Then run the program:

```powershell
.\BetterAltTab.exe
```

The program runs in the background and does not open a normal application window.

## Usage

- Move the mouse to the monitor you want to use.
- Press and release **Alt+Tab** to switch immediately to the next window on that monitor.
- Hold **Alt** and press **Tab** repeatedly to open and cycle through the visual switcher.
- Use the arrow keys while holding Alt to move through the cards.
- Release **Alt** to activate the selected window.
- Press **Escape** to cancel extended switching.

## Start automatically with Windows

After building the program, run this once from the project folder:

```powershell
$startup = [Environment]::GetFolderPath("Startup")
$target = (Resolve-Path ".\BetterAltTab.exe").Path
$shortcut = Join-Path $startup "Better Alt-Tab.lnk"

$wsh = New-Object -ComObject WScript.Shell
$link = $wsh.CreateShortcut($shortcut)
$link.TargetPath = $target
$link.WorkingDirectory = Split-Path $target
$link.Save()
```

Verify that the shortcut exists:

```powershell
Test-Path "$([Environment]::GetFolderPath('Startup'))\Better Alt-Tab.lnk"
```

A result of `True` confirms that it was created. Better Alt-Tab will then start automatically when you sign in.

## Troubleshooting

### The executable cannot be overwritten

The program is still running. Stop it and rebuild:

```powershell
taskkill /IM BetterAltTab.exe /F
g++ -std=c++17 -mwindows main.cpp -o BetterAltTab.exe -luser32 -lgdi32 -lpsapi
```

### `g++` is not recognized

MinGW-w64 is not installed or its `bin` folder is not on PATH.

### The program does not respond to Alt+Tab

Confirm that it is running:

```powershell
Get-Process BetterAltTab
```

If necessary, start it again:

```powershell
.\BetterAltTab.exe
```

### Preview images are black

Some applications, especially browsers and GPU-rendered applications, restrict screen capture. Window activation and switching can still work even when a preview cannot be captured.

## Stop the program

To stop the running utility:

```powershell
taskkill /IM BetterAltTab.exe /F
```

To disable automatic startup, delete the shortcut named **Better Alt-Tab** from the Windows Startup folder.

## Current limitations

- A packaged installer and downloadable release executable are not available yet.
- Some elevated applications may require Better Alt-Tab to run with matching privileges.
- Some protected applications do not provide capturable preview images.
- The utility currently has no tray icon or settings window.

## License

No license has been selected yet. Add a license before distributing the project publicly.
