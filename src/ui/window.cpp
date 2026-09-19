#include "window.h"
#include "layout.h"
#include "menu.h"
#include "render.h"
#include "../app/settings.h"
#include "../metrics/metrics.h"
#include <string.h>

static const wchar_t* const CLASS_NAME = L"FuhajinWidget";

// 最近一帧读数。窗口是唯一的采样触发者，渲染只听它的。
static Metrics s_metrics;
static bool s_valid;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        MetricsSample(&s_metrics);   // 只是打点基线，这一帧没有值
        SetTimer(hwnd, TIMER_SAMPLE, INTERVAL_MS[g_set.interval], NULL);
        return 0;
    case WM_TIMER:
        if (MetricsSample(&s_metrics)) {
            s_valid = true;
            RenderCard(hwnd, s_metrics, s_valid);
        }
        return 0;
    case WM_RBUTTONUP:
        switch (MenuShow(hwnd)) {
        case MENU_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case MENU_REBUILD:
            RenderRebuild();
            break;
        case MENU_REPAINT:
            break;
        default:
            return 0;
        }
        RenderCard(hwnd, s_metrics, s_valid);
        SettingsSave();
        return 0;
    case WM_SETTINGCHANGE:
    case WM_DPICHANGED: {
        UINT d = LayoutQueryDpi();
        if (d != LayoutDpi()) {
            LayoutSetDpi(d);
            RenderRebuild();
        }
        RenderCard(hwnd, s_metrics, s_valid);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SAMPLE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool WindowRegister(HINSTANCE instance) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    return RegisterClassExW(&wc) != 0;
}

HWND WindowCreate(HINSTANCE instance) {
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        CLASS_NAME, L"", WS_POPUP, 0, 0, CARD_W(), CARD_H(), NULL, NULL, instance, NULL);
    if (!hwnd) return NULL;
    RenderCard(hwnd, s_metrics, s_valid);
    return hwnd;
}
