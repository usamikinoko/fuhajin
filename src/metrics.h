#pragma once
#include <windows.h>

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

bool MetricsSample(Metrics* m);
