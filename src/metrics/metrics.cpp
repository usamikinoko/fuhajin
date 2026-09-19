#include "metrics.h"
#include "cpu.h"
#include "gpu.h"
#include "net.h"
#include <psapi.h>
#include <string.h>

static int s_threads;
static bool s_ready;

// GetProcessMemoryInfo 的 PROCESS_MEMORY_COUNTERS_EX 在部分 SDK 里没有
// PrivateWorkingSetSize（PrivateUsage 口径偏大），这里手写完整结构体 —— cb 必须填对。
struct PMC2 {
    DWORD cb, PageFaultCount;
    SIZE_T PeakWorkingSetSize, WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage, PeakPagefileUsage, PrivateUsage;
    SIZE_T PrivateWorkingSetSize;
    ULONGLONG SharedCommitUsage;
};

static unsigned long long SelfMemBytes() {
    PMC2 pmc;
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)) && pmc.PrivateWorkingSetSize > 0)
        return (unsigned long long)pmc.PrivateWorkingSetSize;
    PROCESS_MEMORY_COUNTERS_EX ex;
    memset(&ex, 0, sizeof(ex));
    ex.cb = sizeof(ex);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&ex, sizeof(ex))) return 0;
    return (unsigned long long)ex.PrivateUsage;
}

void MetricsSetHoldMs(int ms) {
    CpuSetHoldMs(ms);
}

static void Init() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s_threads = (int)si.dwNumberOfProcessors;
    CpuInit();
    GpuInit();
    NetInit();
}

bool MetricsSample(Metrics* m) {
    if (!s_ready) {
        Init();
        s_ready = true;
        memset(m, 0, sizeof(*m));
        m->threads = s_threads;
        return false;
    }
    m->threads = s_threads;
    m->cpu = CpuUsagePercent();
    m->gpuTemp = GpuTempC();
    m->gpu = GpuUsagePercent();
    m->cpuTempNow = CpuDieC(m->gpuTemp);
    m->cpuTemp = CpuPeakHold(m->cpuTempNow);
    m->vramMB = GpuVramMB();
    NetRates(&m->down, &m->up);
    m->selfMem = SelfMemBytes();
    return true;
}
