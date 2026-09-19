#pragma once
#include <windows.h>

// 一帧完整读数。采集层的唯一对外契约 —— UI 只认这个结构，不碰任何系统 API。
struct Metrics {
    double cpu;
    int threads;
    double cpuTemp;      // 片上最高温度，已做峰值保持（℃）
    double cpuTempNow;   // 本次采样的瞬时片上温度（℃），0 = 不可用
    double gpu;
    double gpuTemp;
    unsigned long long vramMB;
    double down;
    double up;
    unsigned long long selfMem;
};

// 峰值保持窗口（毫秒）。0 = 关闭，只报瞬时值。
void MetricsSetHoldMs(int ms);

// 首次调用只做初始化并返回 false，之后每次返回一帧完整读数。
// 差分型计数器（CPU 占用、网速）必须有两次采样才有值，所以第一次必然返回 false。
bool MetricsSample(Metrics* m);
