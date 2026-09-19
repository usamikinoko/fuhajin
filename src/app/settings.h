#pragma once
#include <windows.h>

// 全部可持久化配置。取值表就是配置 schema —— 加一档只改这里 + menu.cpp 的菜单项。
enum { EFFECT_SOLID = 0, EFFECT_ALPHA = 1, EFFECT_N = 2 };

struct Settings {
    int pos;      // 挂靠位置
    int interval; // 刷新间隔
    int theme;    // 主题
    int effect;   // 卡片效果：EFFECT_SOLID / EFFECT_ALPHA
    int alpha;    // 半透明的"透明度"（%），越大越透；仅 effect == EFFECT_ALPHA 时生效
    int fontIdx;  // 字号
    int hold;     // 峰值保持秒数，0 = 关闭
};

extern Settings g_set;

constexpr int POS_N = 8;
extern const wchar_t* const POS_NAMES[POS_N];

constexpr int INTERVAL_N = 4;
extern const int INTERVAL_MS[INTERVAL_N];
extern const wchar_t* const INTERVAL_NAMES[INTERVAL_N];

constexpr int THEME_N = 2;
extern const wchar_t* const THEME_NAMES[THEME_N];

extern const wchar_t* const EFFECT_NAMES[EFFECT_N];

// 半透明档位（% 透明度）
constexpr int ALPHA_N = 7;
extern const int ALPHA_PCT[ALPHA_N];

constexpr int FONT_N = 5;
extern const int FONT_SIZES[FONT_N];

constexpr int HOLD_N = 4;
extern const int HOLD_VALS[HOLD_N];
extern const wchar_t* const HOLD_NAMES[HOLD_N];

// 定位 widget.ini、载入并规范化，然后下发一次衍生设置。
void SettingsInit();
void SettingsSave();

// 把 hold 下发到采样层：保持窗口不短于两个采样间隔，否则每个采样点都会被立刻取代。
void SettingsApplyHold();
