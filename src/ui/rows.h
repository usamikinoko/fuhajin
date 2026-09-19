#pragma once
#include <windows.h>
#include "../metrics/metrics.h"
#include "layout.h"

// 一行的文案与颜色。渲染层只认这个，不认识 Metrics 的字段含义。
struct RowText {
    wchar_t label[12];
    wchar_t value[32];
    COLORREF color;
};

// 把一帧读数翻成 ROWS 行文案。valid=false 时温度等不可用项显示 "--"。
void RowsBuild(const Metrics& m, bool valid, RowText out[ROWS]);
