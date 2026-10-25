#define UNICODE
#define _UNICODE

#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cwchar>
#include <windows.h>
#include <psapi.h>
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

extern "C" BOOL WINAPI PrintWindow(
    HWND hwnd,
    HDC hdc,
    UINT flags);

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
HWND g_previewWindow = nullptr;

struct PreviewCard {
    HICON icon;
    HWND hwnd;
    HBITMAP bitmap;
    std::wstring title;
};

std::vector<PreviewCard> g_previewCards;
std::vector<WindowEntry> g_windows;
size_t g_selected = 0;
HMONITOR g_activeMonitor = nullptr;
size_t g_pageStart = 0;

constexpr int MAX_COLUMNS = 4;
constexpr int MAX_ROWS = 3;
constexpr int MAX_VISIBLE_CARDS = MAX_COLUMNS * MAX_ROWS;
constexpr int PREVIEW_WIDTH = 260;
constexpr int PREVIEW_HEIGHT = 165;
constexpr int CARD_TITLE_HEIGHT = 42;
constexpr int CARD_GAP = 18;
constexpr int SWITCHER_MARGIN = 28;

bool g_altDown = false;
bool g_switching = false;
bool g_altSession = false;

void ActivateWindow(HWND target);
void BuildPreviewCards();


HICON GetApplicationIcon(HWND hwnd) {
    HICON icon = nullptr;

    SendMessageTimeout(
        hwnd,
        WM_GETICON,
        ICON_BIG,
        0,
        SMTO_ABORTIFHUNG,
        100,
        reinterpret_cast<PDWORD_PTR>(&icon));

    if (!icon) {
        SendMessageTimeout(
            hwnd,
            WM_GETICON,
            ICON_SMALL,
            0,
            SMTO_ABORTIFHUNG,
            100,
            reinterpret_cast<PDWORD_PTR>(&icon));
    }

    if (!icon) {
        icon = reinterpret_cast<HICON>(
            GetClassLongPtr(hwnd, GCLP_HICON));
    }

    if (!icon) {
        icon = LoadIcon(
            nullptr,
            IDI_APPLICATION);
    }

    return icon;
}

LRESULT CALLBACK KeyboardProc(
    int code,
    WPARAM wParam,
    LPARAM lParam);

bool InstallKeyboardHook() {
    if (g_keyboardHook)
        return true;

    g_keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        KeyboardProc,
        nullptr,
        0);

    return g_keyboardHook != nullptr;
}

bool IsCandidate(HWND hwnd, HMONITOR monitor) {
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd))
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
std::wstring GetApplicationName(HWND hwnd) {
    DWORD processId = 0;

    GetWindowThreadProcessId(
        hwnd,
        &processId);

    HANDLE process =
        OpenProcess(
            PROCESS_QUERY_INFORMATION |
            PROCESS_VM_READ,
            FALSE,
            processId);

    if (!process)
        return L"Application";

    wchar_t path[MAX_PATH] = {};

    DWORD length =
        GetModuleFileNameEx(
            process,
            nullptr,
            path,
            MAX_PATH);

    CloseHandle(process);

    if (length == 0)
        return L"Application";

    const wchar_t* filename =
        wcsrchr(path, L'\\');

    if (filename)
        filename++;
    else
        filename = path;

    std::wstring name(filename);

    size_t extension =
        name.find_last_of(L'.');

    if (extension != std::wstring::npos)
        name.erase(extension);

    if (name == L"chrome")
        return L"Google Chrome";

    if (name == L"msedge")
        return L"Microsoft Edge";

    if (name == L"Code")
        return L"Visual Studio Code";

    if (name == L"explorer")
        return L"File Explorer";

    if (name == L"POWERShell" ||
        name == L"powershell")
        return L"Windows PowerShell";

    return name;
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

    std::wstring applicationName =
        GetApplicationName(hwnd);

    g_windows.push_back({
        hwnd,
        applicationName
    });

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

void PositionSwitcher(HMONITOR monitor, size_t visibleCount) {
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);

    if (!GetMonitorInfo(monitor, &monitorInfo))
        return;

    int columns = static_cast<int>(
        std::min(
            visibleCount,
            static_cast<size_t>(MAX_COLUMNS)));

    if (columns <= 0)
        columns = 1;

    int rows = static_cast<int>(
        (visibleCount + columns - 1) / columns);

    if (rows > MAX_ROWS)
        rows = MAX_ROWS;

    int width =
        SWITCHER_MARGIN * 2 +
        columns * PREVIEW_WIDTH +
        (columns - 1) * CARD_GAP;

    int height =
        SWITCHER_MARGIN * 2 +
        CARD_TITLE_HEIGHT +
        rows * PREVIEW_HEIGHT +
        (rows - 1) * CARD_GAP;

    int monitorWidth =
        monitorInfo.rcWork.right -
        monitorInfo.rcWork.left;

    int monitorHeight =
        monitorInfo.rcWork.bottom -
        monitorInfo.rcWork.top;

    int x =
        monitorInfo.rcWork.left +
        (monitorWidth - width) / 2;

    int y =
        monitorInfo.rcWork.top +
        (monitorHeight - height) / 2;

    SetWindowPos(
        g_switcher,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_SHOWWINDOW);

    HRGN roundedRegion =
        CreateRoundRectRgn(
            0,
            0,
            width + 1,
            height + 1,
            28,
            28);

    SetWindowRgn(
        g_switcher,
        roundedRegion,
        TRUE);
}

void StartSwitching() {
    POINT cursorPosition;
    GetCursorPos(&cursorPosition);

    HMONITOR monitor = MonitorFromPoint(
        cursorPosition,
        MONITOR_DEFAULTTONEAREST);

    g_activeMonitor = monitor;

    g_windows.clear();
    g_selected = 0;
    g_pageStart = 0;

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

    int total =
        static_cast<int>(g_windows.size());

    int next =
        static_cast<int>(g_selected) + direction;

    if (next < 0)
        next = total - 1;

    if (next >= total)
        next = 0;

    g_selected =
        static_cast<size_t>(next);

    g_pageStart =
        (g_selected / MAX_VISIBLE_CARDS) *
        MAX_VISIBLE_CARDS;

    if (g_switching && g_activeMonitor) {
        size_t pageEnd = std::min(
            g_pageStart +
            static_cast<size_t>(MAX_VISIBLE_CARDS),
            g_previewCards.size());

        PositionSwitcher(
            g_activeMonitor,
            pageEnd - g_pageStart);
    }

    InvalidateRect(
        g_switcher,
        nullptr,
        TRUE);
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
    g_pageStart = 0;

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

    BuildPreviewCards();

    size_t visibleCount = std::min(
        g_previewCards.size(),
        static_cast<size_t>(MAX_VISIBLE_CARDS));

    PositionSwitcher(
        monitor,
        visibleCount);
    InvalidateRect(g_switcher, nullptr, TRUE);

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

void ClearPreviewCards() {
    for (auto& card : g_previewCards) {
        if (card.bitmap)
            DeleteObject(card.bitmap);
    }

    g_previewCards.clear();
}

HBITMAP CaptureWindowPreview(HWND hwnd) {
    RECT rect{};
    GetWindowRect(hwnd, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0)
        return nullptr;

    HDC screenDC = GetDC(nullptr);
    HDC memoryDC = CreateCompatibleDC(screenDC);

    HBITMAP bitmap = CreateCompatibleBitmap(
        screenDC,
        width,
        height);

    HGDIOBJ oldBitmap =
        SelectObject(memoryDC, bitmap);

    BOOL captured = PrintWindow(
        hwnd,
        memoryDC,
        PW_RENDERFULLCONTENT);

    SelectObject(memoryDC, oldBitmap);
    DeleteDC(memoryDC);
    ReleaseDC(nullptr, screenDC);

    if (!captured) {
        DeleteObject(bitmap);
        return nullptr;
    }

    return bitmap;
}

void BuildPreviewCards() {
    ClearPreviewCards();

    for (const auto& entry : g_windows) {
        PreviewCard card{};
        card.icon = GetApplicationIcon(entry.hwnd);
        card.hwnd = entry.hwnd;
        card.title = entry.title;
        card.bitmap = CaptureWindowPreview(entry.hwnd);

        g_previewCards.push_back(card);
    }
}

void DrawPreviewCards(HWND hwnd, HDC dc) {
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    HBRUSH background =
        CreateSolidBrush(RGB(24, 24, 28));

    FillRect(
        dc,
        &clientRect,
        background);

    DeleteObject(background);

    if (g_previewCards.empty())
        return;

    const int margin = 28;
    const int gap = 18;
    const int titleHeight = 42;

    size_t visibleEnd =
        std::min(
            g_pageStart +
            static_cast<size_t>(MAX_VISIBLE_CARDS),
            g_previewCards.size());

    int visibleCount =
        static_cast<int>(
            visibleEnd - g_pageStart);

    if (visibleCount <= 0)
        return;

    int columns =
        std::min(
            MAX_COLUMNS,
            visibleCount);

    int rows =
        (visibleCount + columns - 1) /
        columns;

    int availableWidth =
        clientRect.right -
        clientRect.left -
        margin * 2;

    int availableHeight =
        clientRect.bottom -
        clientRect.top -
        margin * 2 -
        titleHeight;

    int cardWidth = PREVIEW_WIDTH;
    int cardHeight = PREVIEW_HEIGHT;

    HFONT titleFont = CreateFont(
        18,
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

    HFONT oldFont =
        (HFONT)SelectObject(
            dc,
            titleFont);

    SetBkMode(
        dc,
        TRANSPARENT);

    for (int visibleIndex = 0;
         visibleIndex < visibleCount;
         ++visibleIndex) {

        size_t index =
            g_pageStart +
            static_cast<size_t>(
                visibleIndex);

        int row =
            visibleIndex / columns;

        int column =
            visibleIndex % columns;

        int x =
            margin +
            column * (cardWidth + gap);

        int y =
            margin +
            titleHeight +
            row * (cardHeight + gap);

        RECT cardRect{
            x,
            y,
            x + cardWidth,
            y + cardHeight
        };

        bool selected =
            index == g_selected;

        COLORREF cardColor =
            selected
                ? RGB(35, 125, 235)
                : RGB(48, 48, 56);

        COLORREF borderColor =
            selected
                ? RGB(90, 180, 255)
                : RGB(70, 70, 78);

        HPEN cardPen =
            CreatePen(
                PS_SOLID,
                selected ? 4 : 1,
                borderColor);

        HBRUSH cardBrush =
            CreateSolidBrush(cardColor);

        HPEN oldPen =
            (HPEN)SelectObject(
                dc,
                cardPen);

        HBRUSH oldBrush =
            (HBRUSH)SelectObject(
                dc,
                cardBrush);

        RoundRect(
            dc,
            x,
            y,
            x + cardWidth,
            y + cardHeight,
            24,
            24);

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);

        DeleteObject(cardBrush);
        DeleteObject(cardPen);

        const int imagePadding = 12;
        const int titleAreaHeight = 34;

        int imageX =
            x + imagePadding;

        int imageY =
            y + imagePadding;

        int imageWidth =
            cardWidth -
            imagePadding * 2;

        int imageHeight =
            cardHeight -
            titleAreaHeight -
            imagePadding * 2;

        if (g_previewCards[index].bitmap) {
            HDC previewDC =
                CreateCompatibleDC(dc);

            HGDIOBJ oldBitmap =
                SelectObject(
                    previewDC,
                    g_previewCards[index].bitmap);

            SetStretchBltMode(
                dc,
                HALFTONE);

            StretchBlt(
                dc,
                imageX,
                imageY,
                imageWidth,
                imageHeight,
                previewDC,
                0,
                0,
                250,
                155,
                SRCCOPY);

            SelectObject(
                previewDC,
                oldBitmap);

            DeleteDC(
                previewDC);
        }
        else {
            RECT emptyPreview{
                imageX,
                imageY,
                imageX + imageWidth,
                imageY + imageHeight
            };

            HBRUSH emptyBrush =
                CreateSolidBrush(
                    RGB(18, 18, 22));

            FillRect(
                dc,
                &emptyPreview,
                emptyBrush);

            DeleteObject(
                emptyBrush);
        }

        RECT titleRect{
            x + imagePadding,
            y + cardHeight - titleAreaHeight,
            x + cardWidth - imagePadding,
            y + cardHeight - 6
        };

        if (g_previewCards[index].icon) {
            DrawIconEx(
                dc,
                titleRect.left,
                titleRect.top + 2,
                g_previewCards[index].icon,
                22,
                22,
                0,
                nullptr,
                DI_NORMAL);

            titleRect.left += 28;
        }

        SetTextColor(
            dc,
            RGB(245, 245, 245));

        DrawText(
            dc,
            g_previewCards[index].title.c_str(),
            -1,
            &titleRect,
            DT_SINGLELINE |
            DT_END_ELLIPSIS |
            DT_LEFT |
            DT_VCENTER);
    }

    SelectObject(
        dc,
        oldFont);

    DeleteObject(
        titleFont);

    size_t totalPages =
        (g_previewCards.size() +
         MAX_VISIBLE_CARDS - 1) /
        MAX_VISIBLE_CARDS;

    size_t currentPage =
        (g_pageStart /
         MAX_VISIBLE_CARDS) + 1;

    wchar_t pageText[64];

    wsprintf(
        pageText,
        L"%lu / %lu",
        static_cast<unsigned long>(
            currentPage),
        static_cast<unsigned long>(
            totalPages));

    SetTextColor(
        dc,
        RGB(180, 180, 190));

    RECT pageRect{
        clientRect.right - 100,
        12,
        clientRect.right - 28,
        36
    };

    DrawText(
        dc,
        pageText,
        -1,
        &pageRect,
        DT_RIGHT |
        DT_SINGLELINE |
        DT_VCENTER);
}

LRESULT CALLBACK SwitcherProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {

    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);

        DrawPreviewCards(hwnd, dc);

        EndPaint(hwnd, &paint);
        return 0;
    }

    if (message == WM_ERASEBKGND)
        return 1;

    if (message == WM_DESTROY) {
        ClearPreviewCards();
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

    if (message == WM_POWERBROADCAST &&
        (wParam == PBT_APMRESUMEAUTOMATIC ||
         wParam == PBT_APMRESUMESUSPEND)) {

        g_altDown = false;
        g_altSession = false;
        g_switching = false;

        ShowWindow(g_switcher, SW_HIDE);

        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }

        InstallKeyboardHook();
        return TRUE;
    }

    if (message == WM_TIMER && wParam == 1) {
        if (!g_keyboardHook)
            InstallKeyboardHook();

        return 0;
    }

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
        WS_POPUP,
        0,
        0,
        900,
        600,
        nullptr,
        nullptr,
        instance,
        nullptr);

    SetTimer(
        g_controller,
        1,
        2000,
        nullptr);

    if (!InstallKeyboardHook()) {
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