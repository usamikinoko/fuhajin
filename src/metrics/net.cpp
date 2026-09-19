#include "net.h"

// 下面这组 include 的顺序不是多余的，调换会编译失败：
// netioapi.h 里的 MIB_IF_TABLE2 被包在 #ifdef _WS2IPDEF_ 里，若 iphlpapi.h 先被包含，
// 会走掉 __IPHLPAPI_H__ 分支导致 ws2ipdef.h 根本没被引入。
// （GetIfTable2 需要 _WIN32_WINNT >= 0x0601，mingw 默认 0xa00，无需自己定义。）
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>

static ULONGLONG s_rx, s_tx, s_tick;

void NetRates(double* down, double* up) {
    *down = 0;
    *up = 0;
    MIB_IF_TABLE2* table = NULL;
    if (GetIfTable2(&table) != NO_ERROR || !table) return;
    ULONGLONG rx = 0, tx = 0;
    for (ULONG i = 0; i < table->NumEntries; i++) {
        MIB_IF_ROW2& r = table->Table[i];
        if (r.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (r.OperStatus != IfOperStatusUp) continue;
        rx += r.InOctets;
        tx += r.OutOctets;
    }
    FreeMibTable(table);
    ULONGLONG now = GetTickCount64();
    if (s_tick && now > s_tick) {
        double dt = (double)(now - s_tick) / 1000.0;
        *down = (double)(rx - s_rx) / dt;
        *up = (double)(tx - s_tx) / dt;
    }
    s_rx = rx;
    s_tx = tx;
    s_tick = now;
}

void NetInit() {
    double d = 0, u = 0;
    NetRates(&d, &u);
}
