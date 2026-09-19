#include <windows.h>
#include <imm.h>

#include "app/settings.h"
#include "metrics/metrics.h"
#include "ui/layout.h"
#include "ui/render.h"
#include "ui/window.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    // 单实例：手滑双击第 N 次不会挂出第 N 个卡片
    if (CreateMutexW(NULL, TRUE, L"FuhajinWidget_Instance") && GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    ImmDisableIME(-1);   // 挂件不接受输入法，省掉一个 IME 线程

    // 每显示器 DPI 感知（Win10 1703+），老系统回退到系统级 DPI 感知
    typedef BOOL(WINAPI* P_SetCtx)(HANDLE);
    HMODULE u = LoadLibraryW(L"user32.dll");
    P_SetCtx setCtx = (P_SetCtx)(void*)GetProcAddress(u, "SetProcessDpiAwarenessContext");
    if (!setCtx || !setCtx((HANDLE)(LONG_PTR)-4)) SetProcessDPIAware();

    LayoutSetDpi(LayoutQueryDpi());   // 必须在建绘图资源之前
    SettingsInit();
    RenderInit();

    if (!WindowRegister(instance)) return 1;
    HWND hwnd = WindowCreate(instance);
    if (!hwnd) return 1;

    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    RenderFree();
    return 0;
}
