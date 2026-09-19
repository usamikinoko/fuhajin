#include <windows.h>
#include <imm.h>
#include <math.h>
#include <wchar.h>
#include <string.h>
#include "metrics.h"

struct Theme {
    COLORREF bg;
    COLORREF border;
    COLORREF label;
    COLORREF value;
    COLORREF ok;
    COLORREF warn;
    COLORREF hot;
    DWORD tint;
};

static const Theme THEMES[2] = {
    { RGB(16, 17, 20), RGB(104, 110, 126), RGB(150, 156, 170), RGB(238, 241, 246),
      RGB(104, 214, 132), RGB(246, 199, 72), RGB(248, 104, 96), 0x38141110u },
    { RGB(246, 247, 250), RGB(196, 201, 212), RGB(118, 126, 140), RGB(24, 28, 36),
      RGB(22, 142, 74), RGB(190, 116, 0), RGB(206, 48, 42), 0x38FAF7F6u }
};

static const int ROWS = 7;
static const int CORNER = 4;

static const int FONT_N = 5;
static const int FONT_SIZES[FONT_N] = { 9, 10, 11, 12, 13 };

static const int A_BG[2] = { 255, 165 };
static const DWORD ACCENT_STATE[2] = { 0, 3 };
static const int INTERVALS[4] = { 500, 1000, 2000, 5000 };
static const wchar_t* INTERVAL_NAMES[4] = { L"0.5 秒", L"1 秒", L"2 秒", L"5 秒" };
static const wchar_t* POS_NAMES[8] = {
    L"顶部左侧", L"顶部居中", L"顶部右侧",
    L"底部左侧", L"底部居中", L"底部右侧",
    L"左侧居中", L"右侧居中"
};
static const wchar_t* THEME_NAMES[2] = { L"暗色", L"明色" };
static const wchar_t* GLASS_NAMES[2] = { L"实体卡片", L"高斯模糊" };
static const int HOLD_N = 4;
static const int HOLD_VALS[HOLD_N] = { 0, 5, 15, 30 };
static const wchar_t* HOLD_NAMES[HOLD_N] = { L"关闭", L"5 秒", L"15 秒", L"30 秒" };

enum { ID_POS = 1000, ID_INT = 1100, ID_THEME = 1200, ID_GLASS = 1300, ID_FONT = 1500, ID_HOLD = 1600, ID_EXIT = 1700 };

struct ACCENT_POLICY { DWORD state; DWORD flags; DWORD grad; DWORD anim; };
struct WCAD { int attr; void* data; SIZE_T size; };
typedef BOOL(WINAPI* P_SetWCA)(HWND, WCAD*);
typedef BOOL(WINAPI* P_SetCtx)(HANDLE);
typedef UINT(WINAPI* P_GetDpiForMon)(HMONITOR, int, UINT*, UINT*);

static int g_pos = 1;
static int g_interval = 1;
static int g_theme = 0;
static int g_glass = 1;
static int g_fontIdx = 1;
static int g_hold = 5;
static int g_dpi = 96;
static int g_w = 0;
static int g_h = 0;
static Metrics g_metrics;
static bool g_valid = false;
static HFONT g_font = NULL;
static HDC g_screen = NULL;
static HDC g_dcBase = NULL;
static HDC g_dcA = NULL;
static HDC g_dcB = NULL;
static HBITMAP g_bmBase = NULL;
static HBITMAP g_bmA = NULL;
static HBITMAP g_bmB = NULL;
static BYTE* g_pxBase = NULL;
static BYTE* g_pxA = NULL;
static BYTE* g_pxB = NULL;
static wchar_t g_iniPath[MAX_PATH];
static P_SetWCA g_setWca = NULL;

static int SC(int v) { return MulDiv(v, g_dpi, 96); }
static const Theme& TH() { return THEMES[g_theme]; }

static int FS() { return FONT_SIZES[g_fontIdx]; }
static int PAD() { return SC(FS() + 2); }
static int ROW_H() { return SC(FS() + 3); }
static int LBL_W() { return SC(FS() * 4); }
static int CARD_W() { return SC(FS() * 74 / 5 + 4); }

// 峰值保持窗口：显示"最近这段时间里出现过的最高片上温度"。
// 窗口不短于两个采样间隔 —— 否则每个采样点都会被立刻取代，保持就没意义了。
static void ApplyHold() {
    if (g_hold <= 0) { MetricsSetHoldMs(0); return; }
    int ms = g_hold * 1000;
    int floorMs = INTERVALS[g_interval] * 2;
    MetricsSetHoldMs(ms < floorMs ? floorMs : ms);
}

static void LoadConfig() {
    g_pos = (int)GetPrivateProfileIntW(L"widget", L"pos", 1, g_iniPath);
    if (g_pos < 0 || g_pos > 7) g_pos = 1;
    g_interval = (int)GetPrivateProfileIntW(L"widget", L"interval", 1, g_iniPath);
    if (g_interval < 0 || g_interval > 3) g_interval = 1;
    g_theme = (int)GetPrivateProfileIntW(L"widget", L"theme", 0, g_iniPath);
    if (g_theme < 0 || g_theme > 1) g_theme = 0;
    g_glass = (int)GetPrivateProfileIntW(L"widget", L"glass", 1, g_iniPath);
    if (g_glass < 0 || g_glass > 1) g_glass = 1;
    g_hold = (int)GetPrivateProfileIntW(L"widget", L"hold", 5, g_iniPath);
    if (g_hold < 0 || g_hold > 60) g_hold = 5;
    int fs = (int)GetPrivateProfileIntW(L"widget", L"font", FS(), g_iniPath);
    for (int i = 0; i < FONT_N; i++)
        if (FONT_SIZES[i] == fs) { g_fontIdx = i; break; }
    ApplyHold();
}

static void SaveConfig() {
    wchar_t b[16];
    wsprintfW(b, L"%d", g_pos);
    WritePrivateProfileStringW(L"widget", L"pos", b, g_iniPath);
    wsprintfW(b, L"%d", g_interval);
    WritePrivateProfileStringW(L"widget", L"interval", b, g_iniPath);
    wsprintfW(b, L"%d", g_theme);
    WritePrivateProfileStringW(L"widget", L"theme", b, g_iniPath);
    wsprintfW(b, L"%d", g_glass);
    WritePrivateProfileStringW(L"widget", L"glass", b, g_iniPath);
    wsprintfW(b, L"%d", FS());
    WritePrivateProfileStringW(L"widget", L"font", b, g_iniPath);
    wsprintfW(b, L"%d", g_hold);
    WritePrivateProfileStringW(L"widget", L"hold", b, g_iniPath);
}

static UINT QueryDpi() {
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

static HBITMAP MakeDib(HDC* dc, BYTE** px) {
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_w;
    bi.bmiHeader.biHeight = -g_h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP hb = CreateDIBSection(g_screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    *dc = CreateCompatibleDC(g_screen);
    SelectObject(*dc, hb);
    *px = (BYTE*)bits;
    return hb;
}

static void FreeGfx() {
    if (g_font) { DeleteObject(g_font); g_font = NULL; }
    if (g_dcBase) { DeleteDC(g_dcBase); g_dcBase = NULL; }
    if (g_dcA) { DeleteDC(g_dcA); g_dcA = NULL; }
    if (g_dcB) { DeleteDC(g_dcB); g_dcB = NULL; }
    if (g_bmBase) { DeleteObject(g_bmBase); g_bmBase = NULL; }
    if (g_bmA) { DeleteObject(g_bmA); g_bmA = NULL; }
    if (g_bmB) { DeleteObject(g_bmB); g_bmB = NULL; }
    g_pxBase = g_pxA = g_pxB = NULL;
}

static void MakeGfx() {
    FreeGfx();
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -SC(FS());
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    g_font = CreateFontIndirectW(&lf);

    g_w = CARD_W();
    g_h = PAD() * 2 + ROW_H() * ROWS;

    g_bmBase = MakeDib(&g_dcBase, &g_pxBase);
    g_bmA = MakeDib(&g_dcA, &g_pxA);
    g_bmB = MakeDib(&g_dcB, &g_pxB);
}

static COLORREF TempColor(double t) {
    if (t >= 88) return TH().hot;
    if (t >= 75) return TH().warn;
    return TH().ok;
}

static void FormatRate(double v, wchar_t* out) {
    if (v < 1024) swprintf(out, 32, L"%.0f B/s", v);
    else if (v < 1048576) swprintf(out, 32, L"%.1f KB/s", v / 1024.0);
    else swprintf(out, 32, L"%.2f MB/s", v / 1048576.0);
}

static void FormatMem(unsigned long long b, wchar_t* out) {
    if (b >= 1073741824ULL) swprintf(out, 32, L"%.2f GB", b / 1073741824.0);
    else if (b >= 1048576ULL) swprintf(out, 32, L"%llu MB", (b + 524288ULL) / 1048576ULL);
    else swprintf(out, 32, L"%llu KB", (b + 512ULL) / 1024ULL);
}

static void BuildRows(wchar_t labels[ROWS][12], wchar_t values[ROWS][32], COLORREF* colors) {
    wcscpy(labels[0], L"CPU");
    wsprintfW(values[0], L"%d%% · Thd %d", g_valid ? (int)(g_metrics.cpu + 0.5) : 0, g_metrics.threads);
    colors[0] = TH().value;

    wcscpy(labels[1], L"CPUT");
    if (g_valid && g_metrics.cpuTemp > 0) {
        // 主数字是峰值保持值；正在保持时（当前瞬时值明显低于它）补一个 ▲，
        // 免得用户以为读数下不去。
        if (g_metrics.cpuTempNow > 0 && g_metrics.cpuTemp - g_metrics.cpuTempNow >= 3.0)
            swprintf(values[1], 32, L"%d℃ ▲", (int)(g_metrics.cpuTemp + 0.5));
        else
            swprintf(values[1], 32, L"%d℃", (int)(g_metrics.cpuTemp + 0.5));
        colors[1] = TempColor(g_metrics.cpuTemp);
    } else {
        wcscpy(values[1], L"--");
        colors[1] = TH().label;
    }

    wcscpy(labels[2], L"GPU");
    int gpuPct = g_valid ? (int)(g_metrics.gpu + 0.5) : 0;
    if (g_metrics.vramMB >= 1024)
        swprintf(values[2], 32, L"%d%% · %.1f GB", gpuPct, g_metrics.vramMB / 1024.0);
    else
        swprintf(values[2], 32, L"%d%% · %u MB", gpuPct, (unsigned)g_metrics.vramMB);
    colors[2] = TH().value;

    wcscpy(labels[3], L"GPUT");
    if (g_valid && g_metrics.gpuTemp > 0) {
        wsprintfW(values[3], L"%d℃", (int)(g_metrics.gpuTemp + 0.5));
        colors[3] = TempColor(g_metrics.gpuTemp);
    } else {
        wcscpy(values[3], L"--");
        colors[3] = TH().label;
    }

    wcscpy(labels[4], L"SELF");
    if (g_valid) FormatMem(g_metrics.selfMem, values[4]);
    else wcscpy(values[4], L"--");
    colors[4] = TH().value;

    wcscpy(labels[5], L"DL");
    if (g_valid) FormatRate(g_metrics.down, values[5]);
    else wcscpy(values[5], L"--");
    colors[5] = TH().value;

    wcscpy(labels[6], L"UL");
    if (g_valid) FormatRate(g_metrics.up, values[6]);
    else wcscpy(values[6], L"--");
    colors[6] = TH().value;
}

static void DrawBase() {
    RECT full = { 0, 0, g_w, g_h };
    HBRUSH b = CreateSolidBrush(TH().bg);
    FillRect(g_dcBase, &full, b);
    DeleteObject(b);

    HPEN pen = CreatePen(PS_SOLID, 1, TH().border);
    HGDIOBJ op = SelectObject(g_dcBase, pen);
    HGDIOBJ ob = SelectObject(g_dcBase, GetStockObject(NULL_BRUSH));
    RoundRect(g_dcBase, 0, 0, g_w - 1, g_h - 1, SC(CORNER) * 2, SC(CORNER) * 2);
    SelectObject(g_dcBase, op);
    SelectObject(g_dcBase, ob);
    DeleteObject(pen);
}

static void DrawTextLayer(HDC dc, COLORREF backdrop, wchar_t labels[ROWS][12],
                          wchar_t values[ROWS][32], COLORREF* colors) {
    RECT full = { 0, 0, g_w, g_h };
    HBRUSH b = CreateSolidBrush(backdrop);
    FillRect(dc, &full, b);
    DeleteObject(b);

    SetBkMode(dc, OPAQUE);
    SetBkColor(dc, backdrop);
    HGDIOBJ of = SelectObject(dc, g_font);
    int y = PAD();
    for (int i = 0; i < ROWS; i++) {
        RECT r;
        r.left = PAD();
        r.right = PAD() + LBL_W();
        r.top = y;
        r.bottom = y + ROW_H();
        SetTextColor(dc, TH().label);
        DrawTextW(dc, labels[i], -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        r.left = r.right;
        r.right = g_w - PAD();
        SetTextColor(dc, colors[i]);
        DrawTextW(dc, values[i], -1, &r, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        y += ROW_H();
    }
    SelectObject(dc, of);
    SetBkMode(dc, TRANSPARENT);
}

static BYTE Over(BYTE src, BYTE dst, double k) {
    double v = src + dst * k;
    if (v > 255.0) v = 255.0;
    return (BYTE)(v + 0.5);
}

static void Composite() {
    const double aRow = A_BG[g_glass] / 255.0;
    const int R = SC(CORNER);
    for (int y = 0; y < g_h; y++) {
        BYTE* pa = g_pxA + (size_t)y * g_w * 4;
        BYTE* pb = g_pxB + (size_t)y * g_w * 4;
        BYTE* pbase = g_pxBase + (size_t)y * g_w * 4;
        for (int x = 0; x < g_w; x++) {
            double cc = 1.0;
            double cx = -1.0, cy = -1.0;
            if (x < R && y < R) { cx = R; cy = R; }
            else if (x >= g_w - R && y < R) { cx = g_w - R; cy = R; }
            else if (x < R && y >= g_h - R) { cx = R; cy = g_h - R; }
            else if (x >= g_w - R && y >= g_h - R) { cx = g_w - R; cy = g_h - R; }
            if (cx >= 0) {
                double dx = (double)x + 0.5 - cx;
                double dy = (double)y + 0.5 - cy;
                cc = R - sqrt(dx * dx + dy * dy) + 0.5;
                if (cc < 0) cc = 0;
                if (cc > 1) cc = 1;
            }
            BYTE* qa = pa + x * 4;
            BYTE* qb = pb + x * 4;
            BYTE* qbase = pbase + x * 4;
            double sa = qa[0] + qa[1] + qa[2];
            double sb = qb[0] + qb[1] + qb[2];
            double cov = 1.0 - (sb - sa) / 765.0;
            if (cov < 0) cov = 0;
            if (cov > 1) cov = 1;
            double da = aRow * cc;
            double k = da * (1.0 - cov);
            qa[0] = Over(qa[0], qbase[0], k);
            qa[1] = Over(qa[1], qbase[1], k);
            qa[2] = Over(qa[2], qbase[2], k);
            double av = (cov + da * (1.0 - cov)) * 255.0;
            qa[3] = av > 255.0 ? 255 : (BYTE)(av + 0.5);
        }
    }
}

static void ApplyAccent(HWND hwnd) {
    if (!g_setWca) return;
    ACCENT_POLICY ap;
    memset(&ap, 0, sizeof(ap));
    ap.state = ACCENT_STATE[g_glass];
    ap.grad = TH().tint;
    WCAD d;
    d.attr = 19;
    d.data = &ap;
    d.size = sizeof(ap);
    g_setWca(hwnd, &d);
}

static void CardPos(POINT* p) {
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT wa = mi.rcWork;
    int m = SC(6);
    int x = wa.left + m;
    int y = wa.top + m;
    switch (g_pos) {
    case 1: x = (wa.left + wa.right - g_w) / 2; break;
    case 2: x = wa.right - m - g_w; break;
    case 3: y = wa.bottom - m - g_h; break;
    case 4: x = (wa.left + wa.right - g_w) / 2; y = wa.bottom - m - g_h; break;
    case 5: x = wa.right - m - g_w; y = wa.bottom - m - g_h; break;
    case 6: y = (wa.top + wa.bottom - g_h) / 2; break;
    case 7: x = wa.right - m - g_w; y = (wa.top + wa.bottom - g_h) / 2; break;
    }
    p->x = x;
    p->y = y;
}

static void Commit(HWND hwnd) {
    POINT d;
    CardPos(&d);
    HDC sdc = GetDC(NULL);
    POINT s = { 0, 0 };
    SIZE sz = { g_w, g_h };
    SIZE sz1 = { g_w + 1, g_h + 1 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, sdc, &d, &sz1, g_dcA, &s, 0, &bf, ULW_ALPHA);
    UpdateLayeredWindow(hwnd, sdc, &d, &sz, g_dcA, &s, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, sdc);
}

static void Render(HWND hwnd) {
    wchar_t labels[ROWS][12];
    wchar_t values[ROWS][32];
    COLORREF colors[ROWS];
    BuildRows(labels, values, colors);
    DrawBase();
    DrawTextLayer(g_dcA, RGB(0, 0, 0), labels, values, colors);
    DrawTextLayer(g_dcB, RGB(255, 255, 255), labels, values, colors);
    Composite();
    Commit(hwnd);
}

static void ShowMenu(HWND hwnd) {
    HMENU root = CreatePopupMenu();
    HMENU pos = CreatePopupMenu();
    for (int i = 0; i < 8; i++)
        AppendMenuW(pos, MF_STRING | (i == g_pos ? MF_CHECKED : 0), ID_POS + i, POS_NAMES[i]);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)pos, L"挂靠位置");

    HMENU it = CreatePopupMenu();
    for (int i = 0; i < 4; i++)
        AppendMenuW(it, MF_STRING | (i == g_interval ? MF_CHECKED : 0), ID_INT + i, INTERVAL_NAMES[i]);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)it, L"刷新间隔");

    HMENU th = CreatePopupMenu();
    for (int i = 0; i < 2; i++)
        AppendMenuW(th, MF_STRING | (i == g_theme ? MF_CHECKED : 0), ID_THEME + i, THEME_NAMES[i]);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)th, L"主题");

    HMENU gl = CreatePopupMenu();
    for (int i = 0; i < 2; i++)
        AppendMenuW(gl, MF_STRING | (i == g_glass ? MF_CHECKED : 0), ID_GLASS + i, GLASS_NAMES[i]);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)gl, L"卡片效果");

    HMENU fo = CreatePopupMenu();
    for (int i = 0; i < FONT_N; i++) {
        wchar_t name[8];
        wsprintfW(name, L"%d", FONT_SIZES[i]);
        AppendMenuW(fo, MF_STRING | (i == g_fontIdx ? MF_CHECKED : 0), ID_FONT + i, name);
    }
    AppendMenuW(root, MF_POPUP, (UINT_PTR)fo, L"字体大小");

    HMENU hd = CreatePopupMenu();
    for (int i = 0; i < HOLD_N; i++)
        AppendMenuW(hd, MF_STRING | (HOLD_VALS[i] == g_hold ? MF_CHECKED : 0), ID_HOLD + i, HOLD_NAMES[i]);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)hd, L"峰值保持");

    AppendMenuW(root, MF_SEPARATOR, 0, NULL);
    AppendMenuW(root, MF_STRING, ID_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex & ~(LONG)WS_EX_NOACTIVATE);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(root, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex);
    DestroyMenu(root);

    bool touched = true;
    if (cmd >= ID_POS && cmd < ID_POS + 8) {
        g_pos = cmd - ID_POS;
    } else if (cmd >= ID_INT && cmd < ID_INT + 4) {
        g_interval = cmd - ID_INT;
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, INTERVALS[g_interval], NULL);
        ApplyHold();
    } else if (cmd >= ID_THEME && cmd < ID_THEME + 2) {
        g_theme = cmd - ID_THEME;
        ApplyAccent(hwnd);
    } else if (cmd >= ID_GLASS && cmd < ID_GLASS + 2) {
        g_glass = cmd - ID_GLASS;
        ApplyAccent(hwnd);
    } else if (cmd >= ID_FONT && cmd < ID_FONT + FONT_N) {
        g_fontIdx = cmd - ID_FONT;
        MakeGfx();
    } else if (cmd >= ID_HOLD && cmd < ID_HOLD + HOLD_N) {
        g_hold = HOLD_VALS[cmd - ID_HOLD];
        ApplyHold();
    } else if (cmd == ID_EXIT) {
        DestroyWindow(hwnd);
        return;
    } else {
        touched = false;
    }
    if (touched) {
        Render(hwnd);
        SaveConfig();
    }
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        MetricsSample(&g_metrics);
        SetTimer(hwnd, 1, INTERVALS[g_interval], NULL);
        SetTimer(hwnd, 2, 250, NULL);
        return 0;
    case WM_TIMER:
        if (wp == 2) {
            if (g_glass) Commit(hwnd);
            return 0;
        }
        if (MetricsSample(&g_metrics)) {
            g_valid = true;
            Render(hwnd);
        }
        return 0;
    case WM_RBUTTONUP:
        ShowMenu(hwnd);
        return 0;
    case WM_SETTINGCHANGE:
    case WM_DPICHANGED: {
        UINT d = QueryDpi();
        if ((int)d != g_dpi) {
            g_dpi = (int)d;
            MakeGfx();
        }
        Render(hwnd);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    if (CreateMutexW(NULL, TRUE, L"FuhajinWidget_Instance") && GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    ImmDisableIME(-1);

    HMODULE u = LoadLibraryW(L"user32.dll");
    g_setWca = (P_SetWCA)(void*)GetProcAddress(u, "SetWindowCompositionAttribute");
    P_SetCtx ctx = (P_SetCtx)(void*)GetProcAddress(u, "SetProcessDpiAwarenessContext");
    if (!ctx || !ctx((HANDLE)(LONG_PTR)-4)) SetProcessDPIAware();

    g_screen = GetDC(NULL);
    g_dpi = (int)QueryDpi();

    GetModuleFileNameW(NULL, g_iniPath, MAX_PATH);
    wchar_t* slash = wcsrchr(g_iniPath, L'\\');
    if (slash) wcscpy(slash + 1, L"widget.ini");
    LoadConfig();

    MakeGfx();

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"FuhajinWidget";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"FuhajinWidget", L"", WS_POPUP, 0, 0, g_w, g_h, NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    ApplyAccent(hwnd);
    Render(hwnd);
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    FreeGfx();
    if (g_screen) ReleaseDC(NULL, g_screen);
    return 0;
}


