#include "settings.h"
#include "../metrics/metrics.h"
#include <wchar.h>
#include <stdlib.h>

Settings g_set = { 1, 1, 0, EFFECT_ALPHA, 50, 1, 5 };

const wchar_t* const POS_NAMES[POS_N] = {
    L"顶部左侧", L"顶部居中", L"顶部右侧",
    L"底部左侧", L"底部居中", L"底部右侧",
    L"左侧居中", L"右侧居中"
};

const int INTERVAL_MS[INTERVAL_N] = { 500, 1000, 2000, 5000 };
const wchar_t* const INTERVAL_NAMES[INTERVAL_N] = { L"0.5 秒", L"1 秒", L"2 秒", L"5 秒" };

const wchar_t* const THEME_NAMES[THEME_N] = { L"暗色", L"明色" };

const wchar_t* const EFFECT_NAMES[EFFECT_N] = { L"实体卡片", L"半透明" };

const int ALPHA_PCT[ALPHA_N] = { 20, 30, 40, 50, 60, 70, 80 };

const int FONT_SIZES[FONT_N] = { 9, 10, 11, 12, 13 };

const int HOLD_VALS[HOLD_N] = { 0, 5, 15, 30 };
const wchar_t* const HOLD_NAMES[HOLD_N] = { L"关闭", L"5 秒", L"15 秒", L"30 秒" };

static wchar_t s_ini[MAX_PATH];

static int LoadInt(const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntW(L"widget", key, def, s_ini);
}

// 越界（含用户手改 ini 写坏）一律退回默认值，而不是悄悄钳到第 0 项。
static int LoadIdx(const wchar_t* key, int def, int n) {
    int v = LoadInt(key, def);
    return (v >= 0 && v < n) ? v : def;
}

static void SaveInt(const wchar_t* key, int v) {
    wchar_t b[16];
    wsprintfW(b, L"%d", v);
    WritePrivateProfileStringW(L"widget", key, b, s_ini);
}

void SettingsApplyHold() {
    if (g_set.hold <= 0) {
        MetricsSetHoldMs(0);
        return;
    }
    int ms = g_set.hold * 1000;
    int floorMs = INTERVAL_MS[g_set.interval] * 2;
    MetricsSetHoldMs(ms < floorMs ? floorMs : ms);
}

void SettingsInit() {
    GetModuleFileNameW(NULL, s_ini, MAX_PATH);
    wchar_t* slash = wcsrchr(s_ini, L'\\');
    if (slash) wcscpy(slash + 1, L"widget.ini");

    // 旧版只有 glass（0 实体卡片 / 1 高斯模糊）。模糊已被半透明取代，
    // 这里把老值读来当 effect 的默认值，老用户的选择不会被重置。
    int legacy = LoadIdx(L"glass", EFFECT_ALPHA, EFFECT_N);

    g_set.pos = LoadIdx(L"pos", g_set.pos, POS_N);
    g_set.interval = LoadIdx(L"interval", g_set.interval, INTERVAL_N);
    g_set.theme = LoadIdx(L"theme", g_set.theme, THEME_N);
    g_set.effect = LoadIdx(L"effect", legacy, EFFECT_N);

    // 透明度吸附到最近的档位，保证菜单里总有一项打勾。
    int a = LoadInt(L"alpha", g_set.alpha);
    int best = 0;
    for (int i = 1; i < ALPHA_N; i++)
        if (abs(ALPHA_PCT[i] - a) < abs(ALPHA_PCT[best] - a)) best = i;
    g_set.alpha = ALPHA_PCT[best];

    int fs = LoadInt(L"font", FONT_SIZES[g_set.fontIdx]);
    for (int i = 0; i < FONT_N; i++)
        if (FONT_SIZES[i] == fs) { g_set.fontIdx = i; break; }

    g_set.hold = LoadInt(L"hold", g_set.hold);
    if (g_set.hold < 0 || g_set.hold > 60) g_set.hold = 5;

    SettingsApplyHold();
}

void SettingsSave() {
    SaveInt(L"pos", g_set.pos);
    SaveInt(L"interval", g_set.interval);
    SaveInt(L"theme", g_set.theme);
    SaveInt(L"effect", g_set.effect);
    SaveInt(L"alpha", g_set.alpha);
    SaveInt(L"font", FONT_SIZES[g_set.fontIdx]);
    SaveInt(L"hold", g_set.hold);
}
