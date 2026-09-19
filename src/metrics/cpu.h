#pragma once

// CPU 侧数据源：占用率（GetSystemTimes 差分）+ 片上温度（ACPI 热区）+ 峰值保持。

void CpuInit();
double CpuUsagePercent();
double CpuDieC(double gpuTempC);   // 片上最高温度（℃），0 = 不可用
double CpuPeakHold(double nowC);   // 峰值保持后的温度（℃）
void CpuSetHoldMs(int ms);
