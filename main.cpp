#define UNICODE
#define _UNICODE

#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cwchar>

constexpr UINT WM_HOOK_ALT_TAB = WM_APP + 1;
constexpr UINT WM_HOOK_TAB = WM_APP + 2;
constexpr UINT WM_HOOK_ALT_UP = WM_APP + 3;
constexpr UINT WM_HOOK_ESCAPE = WM_APP + 4;

struct WindowEntry {
    HWND hwnd;
    std::wstring title;
};

HINSTANCE g_instance = nullptr;
HWND g_controller = nullptr;
HWND g_switcher = nullptr;
HWND g_list = nullptr;
HHOOK g_keyboardHook = nullptr;

std::vector<WindowEntry> g_windows;
size_t g_selected = 0;

bool g_altDown = false;
bool g_switching = false;
bool g_altSession = false;

void ActivateWindow(HWND target);

bool IsCandidate(HWND hwnd, HMONITOR monitor) {
    if (!IsWindow(hwnd))
        return false;

    if (!IsWindowVisible(hwnd))
        return false;

    if (GetWindow(hwnd, GW_OWNER))
        return false;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    if (exStyle & WS_EX_TOOLWINDOW)
        return false;

    if (hwnd == g_switcher || hwnd == g_controller)
        return false;

    HMONITOR windowMonitor =
        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    return windowMonitor == monitor;
}

BOOL CALLBACK EnumerateWindows(HWND hwnd, LPARAM data) {
    HMONITOR monitor = reinterpret_cast<HMONITOR>(data);

    if (!IsCandidate(hwnd, monitor))
        return TRUE;

    int titleLength = GetWindowTextLength(hwnd);

    if (titleLength <= 0)
        return TRUE;

    std::wstring title(titleLength + 1, L'\0');

    GetWindowText(
        hwnd,
        &title[0],
        static_cast<int>(title.size()));

    title.resize(std::wcslen(title.c_str()));

    g_windows.push_back({ hwnd, title });

    return TRUE;
}

void RefreshList() {
    SendMessage(g_list, LB_RESETCONTENT, 0, 0);

    for (const auto& entry : g_windows) {
        SendMessage(
            g_list,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(entry.title.c_str()));
    }

    if (!g_windows.empty()) {
        if (g_selected >= g_windows.size())
            g_selected = 0;

        SendMessage(
            g_list,
            LB_SETCURSEL,
            static_cast<WPARAM>(g_selected),
            0);
    }
}

void PositionSwitcher(HMONITOR monitor) {
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);

    if (!GetMonitorInfo(monitor, &monitorInfo))
        return;

    const int width = 620;
    const int height = 230;

    int x = monitorInfo.rcWork.left +
            ((monitorInfo.rcWork.right -
              monitorInfo.rcWork.left -
              width) / 2);

    int y = monitorInfo.rcWork.top +
            ((monitorInfo.rcWork.bottom -
              monitorInfo.rcWork.top -
              height) / 2);

    SetWindowPos(
        g_switcher,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_SHOWWINDOW);
}

void StartSwitching() {
    POINT cursorPosition;
    GetCursorPos(&cursorPosition);

    HMONITOR monitor = MonitorFromPoint(
        cursorPosition,
        MONITOR_DEFAULTTONEAREST);

    g_windows.clear();
    g_selected = 0;

    EnumWindows(
        EnumerateWindows,
        reinterpret_cast<LPARAM>(monitor));

    if (g_windows.empty())
        return;

    HWND currentWindow = GetForegroundWindow();

    for (size_t i = 0; i < g_windows.size(); ++i) {
        if (g_windows[i].hwnd == currentWindow) {
            g_selected = (i + 1) % g_windows.size();
            break;
        }
    }

    HWND target = g_windows[g_selected].hwnd;

    ActivateWindow(target);

    g_altSession = true;
    g_switching = false;
}

void MoveSelection(int direction) {
    if (g_windows.empty())
        return;

    int count = static_cast<int>(g_windows.size());
    int next = static_cast<int>(g_selected) + direction;

    if (next < 0)
        next = count - 1;

    if (next >= count)
        next = 0;

    g_selected = static_cast<size_t>(next);

    SendMessage(
        g_list,
        LB_SETCURSEL,
        static_cast<WPARAM>(g_selected),
        0);
}
void ActivateWindow(HWND target) {
    if (!target || !IsWindow(target))
        return;

    DWORD currentThreadId =
        GetCurrentThreadId();

    DWORD targetThreadId =
        GetWindowThreadProcessId(target, nullptr);

    HWND foreground =
        GetForegroundWindow();

    DWORD foregroundThreadId =
        foreground
            ? GetWindowThreadProcessId(foreground, nullptr)
            : 0;

    if (IsIconic(target))
        ShowWindow(target, SW_RESTORE);

    if (foregroundThreadId &&
        foregroundThreadId != currentThreadId) {
        AttachThreadInput(
            currentThreadId,
            foregroundThreadId,
            TRUE);
    }

    if (targetThreadId &&
        targetThreadId != currentThreadId) {
        AttachThreadInput(
            currentThreadId,
            targetThreadId,
            TRUE);
    }

    BringWindowToTop(target);
    SetWindowPos(
        target,
        HWND_TOP,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_SHOWWINDOW);

    SetForegroundWindow(target);
    SetFocus(target);

    if (targetThreadId &&
        targetThreadId != currentThreadId) {
        AttachThreadInput(
            currentThreadId,
            targetThreadId,
            FALSE);
    }

    if (foregroundThreadId &&
        foregroundThreadId != currentThreadId) {
        AttachThreadInput(
            currentThreadId,
            foregroundThreadId,
            FALSE);
    }
}

void BeginExtendedSwitching() {
    POINT cursorPosition;
    GetCursorPos(&cursorPosition);

    HMONITOR monitor = MonitorFromPoint(
        cursorPosition,
        MONITOR_DEFAULTTONEAREST);

    g_windows.clear();
    g_selected = 0;

    EnumWindows(
        EnumerateWindows,
        reinterpret_cast<LPARAM>(monitor));

    if (g_windows.empty())
        return;

    HWND currentWindow = GetForegroundWindow();

    for (size_t i = 0; i < g_windows.size(); ++i) {
        if (g_windows[i].hwnd == currentWindow) {
            g_selected = (i + 1) % g_windows.size();
            break;
        }
    }

    PositionSwitcher(monitor);
    RefreshList();

    g_switching = true;
    SetForegroundWindow(g_switcher);
}

void FinishSwitching(bool activate) {
    if (!g_switching)
        return;

    HWND target = nullptr;

    if (activate && !g_windows.empty())
        target = g_windows[g_selected].hwnd;

    g_switching = false;

    ShowWindow(g_switcher, SW_HIDE);

    if (target && IsWindow(target)) {
    ActivateWindow(target);
}
}

LRESULT CALLBACK KeyboardProc(
    int code,
    WPARAM wParam,
    LPARAM lParam) {
    
    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT* key =
            reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        bool keyDown =
            wParam == WM_KEYDOWN ||
            wParam == WM_SYSKEYDOWN;

        bool keyUp =
            wParam == WM_KEYUP ||
            wParam == WM_SYSKEYUP;

        if (key->vkCode == VK_MENU ||
            key->vkCode == VK_LMENU ||
            key->vkCode == VK_RMENU) {
            
            if (keyDown && !g_altDown)
                g_altDown = true;

            if (keyUp && g_altDown) {
                g_altDown = false;
                PostMessage(
                    g_controller,
                    WM_HOOK_ALT_UP,
                    0,
                    0);
            }
        }
        else if (key->vkCode == VK_TAB &&
                 keyDown &&
                 g_altDown) {
            
            PostMessage(
                g_controller,
                (g_switching || g_altSession)
                    ? WM_HOOK_TAB
                    : WM_HOOK_ALT_TAB,
                0,
                0);

            return 1;
        }
        else if (key->vkCode == VK_ESCAPE &&
                 keyDown &&
                 g_switching) {
            
            PostMessage(
                g_controller,
                WM_HOOK_ESCAPE,
                0,
                0);

            return 1;
        }
        else if (g_switching &&
                 keyDown &&
                 (key->vkCode == VK_RIGHT ||
                  key->vkCode == VK_DOWN)) {
            
            PostMessage(
                g_controller,
                WM_HOOK_TAB,
                0,
                0);

            return 1;
        }
        else if (g_switching &&
                 keyDown &&
                 (key->vkCode == VK_LEFT ||
                  key->vkCode == VK_UP)) {
            
            PostMessage(
                g_controller,
                WM_HOOK_TAB,
                1,
                0);

            return 1;
        }
    }

    return CallNextHookEx(
        g_keyboardHook,
        code,
        wParam,
        lParam);
}

LRESULT CALLBACK SwitcherProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    
    if (message == WM_CREATE) {
        g_list = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            nullptr,
            WS_CHILD |
            WS_VISIBLE |
            LBS_NOINTEGRALHEIGHT,
            18,
            45,
            584,
            150,
            hwnd,
            nullptr,
            g_instance,
            nullptr);

        HFONT font = CreateFont(
            20,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH,
            L"Segoe UI");

        SendMessage(
            g_list,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(font),
            TRUE);
    }
    else if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC deviceContext = BeginPaint(hwnd, &paint);

        SetBkMode(deviceContext, TRANSPARENT);
        SetTextColor(deviceContext, RGB(240, 240, 240));

        const wchar_t* heading =
            L"Better Alt-Tab - this monitor";

        TextOut(
            deviceContext,
            18,
            15,
            heading,
            lstrlen(heading));

        EndPaint(hwnd, &paint);

        return 0;
    }

    return DefWindowProc(
        hwnd,
        message,
        wParam,
        lParam);
}

LRESULT CALLBACK ControllerProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    if (message == WM_HOOK_ALT_TAB) {
        StartSwitching();
    }
    else if (message == WM_HOOK_TAB) {
        if (g_altSession && !g_switching) {
            BeginExtendedSwitching();
        }
        else if (g_switching) {
            MoveSelection(wParam ? -1 : 1);
        }
    }
    else if (message == WM_HOOK_ALT_UP) {
        if (g_switching) {
            FinishSwitching(true);
        }

        g_altSession = false;
    }
    else if (message == WM_HOOK_ESCAPE) {
        FinishSwitching(false);
        g_altSession = false;
    }

    return DefWindowProc(
        hwnd,
        message,
        wParam,
        lParam);
}

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE,
    LPSTR,
    int) {
    
    g_instance = instance;

    WNDCLASS controllerClass = {};
    controllerClass.hInstance = instance;
    controllerClass.lpfnWndProc = ControllerProc;
    controllerClass.lpszClassName =
        L"BetterAltTabController";

    RegisterClass(&controllerClass);

    g_controller = CreateWindow(
        controllerClass.lpszClassName,
        L"",
        0,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        nullptr);

    WNDCLASS switcherClass = {};
    switcherClass.hInstance = instance;
    switcherClass.lpfnWndProc = SwitcherProc;
    switcherClass.lpszClassName =
        L"BetterAltTabSwitcher";
    switcherClass.hbrBackground =
        CreateSolidBrush(RGB(35, 35, 38));
    switcherClass.hCursor =
        LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&switcherClass);

    g_switcher = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        switcherClass.lpszClassName,
        L"Better Alt-Tab",
        WS_POPUP | WS_BORDER,
        0,
        0,
        620,
        230,
        nullptr,
        nullptr,
        instance,
        nullptr);

    g_keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        KeyboardProc,
        instance,
        0);

    if (!g_keyboardHook) {
        MessageBox(
            nullptr,
            L"Could not install the keyboard hook.",
            L"Better Alt-Tab",
            MB_ICONERROR);

        return 1;
    }

    MSG message;

    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    UnhookWindowsHookEx(g_keyboardHook);

    return 0;
}