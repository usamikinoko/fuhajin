#pragma once

// 网卡速率（GetIfTable2 累计字节差分）。

void NetInit();
void NetRates(double* down, double* up);   // 单位 B/s
