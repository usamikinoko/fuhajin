#include "layout.h"
#include "../app/settings.h"
#include <string.h>

typedef UINT(WINAPI* P_GetDpiForMon)(HMONITOR, int, UINT*, UINT*);

static UINT s_dpi = 96;

int SC(int v) { return MulDiv(v, (int)s_dpi, 96); }
int FS() { return FONT_SIZES[g_set.fontIdx]; }
int PAD() { return SC(FS() + 2); }
int ROW_H() { return SC(FS() + 3); }
int LBL_W() { return SC(FS() * 4); }
int CARD_W() { return SC(FS() * 74 / 5 + 4); }
int CARD_H() { return PAD() * 2 + ROW_H() * ROWS; }
int CORNER() { return SC(CORNER_DIP); }

void LayoutSetDpi(UINT dpi) { s_dpi = dpi < 96 ? 96 : dpi; }
UINT LayoutDpi() { return s_dpi; }

UINT LayoutQueryDpi() {
    HMODULE sh = LoadLibraryW(L"shcore.dll");
    if (sh) {
        P_GetDpiForMon gdm = (P_GetDpiForMon)(void*)GetProcAddress(sh, "GetDpiForMonitor");
        if (gdm) {
            HMONITOR mon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
            UINT x = 96, y = 96;
            if (gdm(mon, 0, &x, &y) == 0 && y >= 96) return y;
        }
    }
    HDC dc = GetDC(NULL);
    int v = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return v < 96 ? 96 : (UINT)v;
}

POINT LayoutCardPos() {
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT wa = mi.rcWork;
    int w = CARD_W(), h = CARD_H(), m = SC(6);
    int x = wa.left + m;
    int y = wa.top + m;
    switch (g_set.pos) {
    case 1: x = (wa.left + wa.right - w) / 2; break;
    case 2: x = wa.right - m - w; break;
    case 3: y = wa.bottom - m - h; break;
    case 4: x = (wa.left + wa.right - w) / 2; y = wa.bottom - m - h; break;
    case 5: x = wa.right - m - w; y = wa.bottom - m - h; break;
    case 6: y = (wa.top + wa.bottom - h) / 2; break;
    case 7: x = wa.right - m - w; y = (wa.top + wa.bottom - h) / 2; break;
    }
    POINT p = { x, y };
    return p;
}
