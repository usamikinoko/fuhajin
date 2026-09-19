#pragma once

// GPU 侧数据源：PDH 引擎占用 / 专用显存，D3DKMT 直读 iGPU 温度。

void GpuInit();
double GpuUsagePercent();
double GpuTempC();                 // iGPU 片上温度（℃），0 = 不可用
unsigned long long GpuVramMB();
