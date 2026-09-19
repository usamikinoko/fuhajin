#pragma once
#include <windows.h>

// 配色。加主题 = 在 theme.cpp 的表里加一行 + settings.h 的 THEME_N。
struct Theme {
    COLORREF bg;
    COLORREF border;
    COLORREF label;
    COLORREF value;
    COLORREF ok;
    COLORREF warn;
    COLORREF hot;
};

const Theme& ThemeGet();
