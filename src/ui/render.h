#pragma once
#include <windows.h>
#include "../metrics/metrics.h"

// 分层窗口的绘图资源与绘制管线。
// 卡片是 WS_EX_LAYERED，一帧的像素在离屏 DIB 里合成好后一次性提交。

void RenderInit();     // 屏幕 DC + 字体 + 离屏位图（进程内一次）
void RenderRebuild();  // 字号 / DPI 变化后重建（尺寸随之变化）
void RenderFree();

void RenderCard(HWND hwnd, const Metrics& m, bool valid);
