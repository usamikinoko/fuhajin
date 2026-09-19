#pragma once
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>   // PDH_FMT_COUNTERVALUE_ITEM_W / PDH_MORE_DATA 在这里

// PDH 共用封装：cpu.cpp（热区）和 gpu.cpp（引擎占用 / 显存）都要挂计数器。
// 一律用英文计数器路径（PdhAddEnglishCounterW），不受系统显示语言影响。

// 依次尝试 n 个计数器路径，挂上第一个可用的；返回命中的下标，全部失败返回 -1。
// 首次采集已经在内部做过一次，差分型计数器下次调用即有值。
int PdhOpenFirst(PDH_HQUERY* q, PDH_HCOUNTER* c, const wchar_t* const* paths, int n);

bool PdhCollect(PDH_HQUERY q);

// 取计数器数组。返回的缓冲区由本模块复用，下一次调用即失效，调用方必须立刻消费完。
PDH_FMT_COUNTERVALUE_ITEM_W* PdhCollectArray(PDH_HCOUNTER c, DWORD* count);

// 采集一次并返回数组中的最大值（多实例计数器里"取其一"的口径，如显存占用）。
double PdhMax(PDH_HQUERY q, PDH_HCOUNTER c);
