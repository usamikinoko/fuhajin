#include "menu.h"
#include "input.h"
#include "window.h"
#include "../app/settings.h"
#include <wchar.h>

enum {
    ID_POS = 1000,
    ID_INTERVAL = 1100,
    ID_THEME = 1200,
    ID_EFFECT = 1300,
    ID_ALPHA = 1320,
    ID_FONT = 1500,
    ID_HOLD = 1600,
    ID_EXIT = 1700
};

static void AppendByIndex(HMENU m, int idBase, const wchar_t* const* names, int n, int cur) {
    for (int i = 0; i < n; i++)
        AppendMenuW(m, MF_STRING | (i == cur ? MF_CHECKED : 0), idBase + i, names[i]);
}

static void AppendByValue(HMENU m, int idBase, const int* vals, const wchar_t* const* names, int n, int cur) {
    for (int i = 0; i < n; i++)
        AppendMenuW(m, MF_STRING | (vals[i] == cur ? MF_CHECKED : 0), idBase + i, names[i]);
}

MenuAction MenuShow(HWND hwnd) {
    HMENU root = CreatePopupMenu();

    HMENU pos = CreatePopupMenu();
    AppendByIndex(pos, ID_POS, POS_NAMES, POS_N, g_set.pos);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)pos, L"挂靠位置");

    HMENU it = CreatePopupMenu();
    AppendByIndex(it, ID_INTERVAL, INTERVAL_NAMES, INTERVAL_N, g_set.interval);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)it, L"刷新间隔");

    HMENU th = CreatePopupMenu();
    AppendByIndex(th, ID_THEME, THEME_NAMES, THEME_N, g_set.theme);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)th, L"主题");

    HMENU alpha = CreatePopupMenu();
    for (int i = 0; i < ALPHA_N; i++) {
        wchar_t name[8];
        wsprintfW(name, L"%d%%", ALPHA_PCT[i]);
        AppendMenuW(alpha, MF_STRING | (g_set.effect == EFFECT_ALPHA && ALPHA_PCT[i] == g_set.alpha ? MF_CHECKED : 0),
                    ID_ALPHA + i, name);
    }
    HMENU eff = CreatePopupMenu();
    AppendMenuW(eff, MF_STRING | (g_set.effect == EFFECT_SOLID ? MF_CHECKED : 0),
                ID_EFFECT + EFFECT_SOLID, EFFECT_NAMES[EFFECT_SOLID]);
    wchar_t alphaLabel[32];
    if (g_set.effect == EFFECT_ALPHA)
        wsprintfW(alphaLabel, L"%s（%d%%）", EFFECT_NAMES[EFFECT_ALPHA], g_set.alpha);
    else
        wcscpy(alphaLabel, EFFECT_NAMES[EFFECT_ALPHA]);
    AppendMenuW(eff, MF_POPUP, (UINT_PTR)alpha, alphaLabel);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)eff, L"卡片效果");

    HMENU fo = CreatePopupMenu();
    for (int i = 0; i < FONT_N; i++) {
        wchar_t name[8];
        wsprintfW(name, L"%d", FONT_SIZES[i]);
        AppendMenuW(fo, MF_STRING | (i == g_set.fontIdx ? MF_CHECKED : 0), ID_FONT + i, name);
    }
    AppendMenuW(root, MF_POPUP, (UINT_PTR)fo, L"字体大小");

    HMENU hd = CreatePopupMenu();
    AppendByValue(hd, ID_HOLD, HOLD_VALS, HOLD_NAMES, HOLD_N, g_set.hold);
    AppendMenuW(root, MF_POPUP, (UINT_PTR)hd, L"峰值保持");

    AppendMenuW(root, MF_SEPARATOR, 0, NULL);
    AppendMenuW(root, MF_STRING, ID_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    InputMenuOpen(true);
    int cmd = TrackPopupMenu(root, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    InputMenuOpen(false);
    DestroyMenu(root);

    MenuAction act = MENU_REPAINT;
    if (cmd >= ID_POS && cmd < ID_POS + POS_N) {
        g_set.pos = cmd - ID_POS;
    } else if (cmd >= ID_INTERVAL && cmd < ID_INTERVAL + INTERVAL_N) {
        g_set.interval = cmd - ID_INTERVAL;
        KillTimer(hwnd, TIMER_SAMPLE);
        SetTimer(hwnd, TIMER_SAMPLE, INTERVAL_MS[g_set.interval], NULL);
        SettingsApplyHold();
    } else if (cmd >= ID_THEME && cmd < ID_THEME + THEME_N) {
        g_set.theme = cmd - ID_THEME;
    } else if (cmd >= ID_EFFECT && cmd < ID_EFFECT + EFFECT_N) {
        g_set.effect = cmd - ID_EFFECT;
    } else if (cmd >= ID_ALPHA && cmd < ID_ALPHA + ALPHA_N) {
        g_set.alpha = ALPHA_PCT[cmd - ID_ALPHA];
        g_set.effect = EFFECT_ALPHA;
    } else if (cmd >= ID_FONT && cmd < ID_FONT + FONT_N) {
        g_set.fontIdx = cmd - ID_FONT;
        act = MENU_REBUILD;
    } else if (cmd >= ID_HOLD && cmd < ID_HOLD + HOLD_N) {
        g_set.hold = HOLD_VALS[cmd - ID_HOLD];
        SettingsApplyHold();
    } else if (cmd == ID_EXIT) {
        return MENU_EXIT;
    } else {
        return MENU_NONE;
    }

    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    return act;
}
