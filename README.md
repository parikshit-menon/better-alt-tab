# Better Alt-Tab (prototype)

This is a small Windows-only prototype that replaces the system Alt+Tab flow with a per-monitor switcher. When Alt+Tab is pressed, it finds the monitor under the mouse cursor, lists eligible top-level windows on that monitor, cycles with Tab (or the arrow keys), and activates the selected window when Alt is released.

## Build

Requirements:

- Windows 10/11
- MinGW-w64 g++ or Visual Studio C++ tools
- CMake (optional)

With MinGW:

```powershell
g++ -std=c++17 -municode -mwindows src/main.cpp -o BetterAltTab.exe -luser32 -lgdi32 -ldwmapi
```

Or with CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Run `BetterAltTab.exe`. It stays resident in the background. Press Alt+Tab while the mouse is over a monitor; release Alt to activate the highlighted window. Escape cancels.

## Prototype limitations

- The low-level keyboard hook must run in the interactive desktop and may be restricted by security software.
- Windows that are minimized, hidden, owned, tool windows, or have no title are omitted.
- Some elevated applications may not accept activation from a non-elevated process.
- The app currently has no tray icon or installer; close it from Task Manager.
