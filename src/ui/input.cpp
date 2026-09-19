#include "input.h"
#include <wchar.h>

static HHOOK s_hook;
static HWND s_hwnd;
static bool s_pass;
static bool s_catchRight;
static bool s_quietRight;
static bool s_menu;

static bool HitCard(POINT pt) {
    RECT r;
    return s_hwnd && GetWindowRect(s_hwnd, &r) && PtInRect(&r, pt);
}

static bool OverOwnMenu(POINT pt) {
    HWND h = WindowFromPoint(pt);
    if (!h) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return false;
    wchar_t cls[16];
    GetClassNameW(h, cls, 16);
    return wcscmp(cls, L"#32768") == 0;
}

static void PostRight(UINT msg, POINT pt) {
    ScreenToClient(s_hwnd, &pt);
    PostMessageW(s_hwnd, msg, 0, MAKELPARAM(pt.x, pt.y));
}

static LRESULT CALLBACK HookProc(int code, WPARAM wp, LPARAM lp) {
    if (code != HC_ACTION) return CallNextHookEx(NULL, code, wp, lp);
    POINT pt = ((MSLLHOOKSTRUCT*)lp)->pt;

    if (wp == WM_LBUTTONDOWN || wp == WM_LBUTTONUP) {
        if (s_menu && wp == WM_LBUTTONDOWN && !OverOwnMenu(pt))
            PostMessageW(s_hwnd, WM_CANCELMODE, 0, 0);
        return CallNextHookEx(NULL, code, wp, lp);
    }

    if (wp == WM_RBUTTONUP) {
        if (!s_catchRight) return CallNextHookEx(NULL, code, wp, lp);
        s_catchRight = false;
        if (!s_quietRight) PostRight(WM_RBUTTONUP, pt);
        s_quietRight = false;
        return 1;
    }

    if (wp == WM_RBUTTONDOWN || wp == WM_RBUTTONDBLCLK) {
        s_catchRight = false;
        if (s_menu) {
            if (OverOwnMenu(pt)) return CallNextHookEx(NULL, code, wp, lp);
            PostMessageW(s_hwnd, WM_CANCELMODE, 0, 0);
            if (!HitCard(pt)) return CallNextHookEx(NULL, code, wp, lp);
            s_catchRight = true;
            s_quietRight = true;
            return 1;
        }
        if (!s_pass || !HitCard(pt)) return CallNextHookEx(NULL, code, wp, lp);
        s_catchRight = true;
        if (wp == WM_RBUTTONDOWN) PostRight(WM_RBUTTONDOWN, pt);
        return 1;
    }

    if (s_menu && wp == WM_MBUTTONDOWN && !OverOwnMenu(pt))
        PostMessageW(s_hwnd, WM_CANCELMODE, 0, 0);

    if (!s_pass) return CallNextHookEx(NULL, code, wp, lp);

    switch (wp) {
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (HitCard(pt)) return 1;
        break;
    }
    return CallNextHookEx(NULL, code, wp, lp);
}

bool InputInit(HWND hwnd) {
    s_hwnd = hwnd;
    s_pass = false;
    s_hook = SetWindowsHookExW(WH_MOUSE_LL, HookProc, GetModuleHandleW(NULL), 0);
    return s_hook != NULL;
}

void InputDone() {
    if (s_hook) UnhookWindowsHookEx(s_hook);
    s_hook = NULL;
    s_hwnd = NULL;
    s_pass = false;
    s_catchRight = false;
    s_quietRight = false;
    s_menu = false;
}

void InputMenuOpen(bool open) { s_menu = open; }

void InputSetPass(bool on) {
    if (!s_hwnd || on == s_pass) return;
    s_pass = on;
    LONG ex = GetWindowLongW(s_hwnd, GWL_EXSTYLE);
    if (on) ex |= WS_EX_TRANSPARENT;
    else ex &= ~(LONG)WS_EX_TRANSPARENT;
    SetWindowLongW(s_hwnd, GWL_EXSTYLE, ex);
    if (!IsWindowVisible(s_hwnd)) return;
    ShowWindow(s_hwnd, SW_HIDE);
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
}
