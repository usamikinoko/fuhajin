#pragma once
#include <windows.h>

// 几何：所有尺寸都从这里出，DPI 变化的唯一入口。
// 卡片行数 = CPU / CPUT / GPU / GPUT / SELF / DL / UL
constexpr int ROWS = 7;
constexpr int CORNER_DIP = 4;   // 圆角半径（96 DPI 下的值）

UINT LayoutQueryDpi();          // 从系统查当前 DPI
void LayoutSetDpi(UINT dpi);
UINT LayoutDpi();

int SC(int v);                  // 按 DPI 缩放
int FS();                       // 当前字号
int PAD();                      // 卡片内边距
int ROW_H();
int LBL_W();                    // 标签列宽
int CARD_W();
int CARD_H();
int CORNER();

POINT LayoutCardPos();          // 依 g_set.pos 算出挂在主屏工作区的哪个角
