#define _WIN32_WINNT 0x0601
#define WINVER 0x0601
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include "metrics.h"
#include <wchar.h>
#include <string.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

typedef UINT32 HAD;

struct OPEN_GDINAME { WCHAR DeviceName[32]; HAD hAdapter; };
struct QUERY_INFO { HAD hAdapter; UINT32 Type; void* pData; UINT32 Size; };
struct ADAPTER_PERFDATA {
    UINT32 PhysicalAdapterIndex;
    UINT64 MemoryFrequency;
    UINT64 MaxMemoryFrequency;
    UINT64 MaxMemoryFrequencyOC;
    UINT64 MemoryBandwidth;
    UINT64 PCIEBandwidth;
    UINT32 FanRPM;
    UINT32 Power;
    UINT32 Temperature;
    UINT8 PowerStateOverride;
};

typedef LONG(NTAPI* PFN_OPEN_GDINAME)(OPEN_GDINAME*);
typedef LONG(NTAPI* PFN_QUERY_ADAPTER)(QUERY_INFO*);

static const UINT32 KMTQAITYPE_ADAPTERPERFDATA = 62;

static PDH_HQUERY s_qThermal, s_qGpu, s_qVram;
static PDH_HCOUNTER s_cThermal, s_cGpu, s_cVram;
static BYTE* s_buf;
static DWORD s_bufSize;
static ULONGLONG s_idle, s_kernel, s_user;
static ULONGLONG s_rx, s_tx, s_tick;
static HAD s_hAdapter;
static PFN_QUERY_ADAPTER s_queryAdapter;
static int s_threads;
static bool s_ready;
static bool s_thermalHighRes;
static double s_cpuPeak;
static ULONGLONG s_cpuPeakAt;
static int s_holdMs = 5000;

static ULONGLONG FT(const FILETIME& f) {
    return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime;
}

static void PrimeCpu() {
    FILETIME a, b, c;
    if (!GetSystemTimes(&a, &b, &c)) return;
    s_idle = FT(a);
    s_kernel = FT(b);
    s_user = FT(c);
}

static double CpuPercent() {
    FILETIME a, b, c;
    if (!GetSystemTimes(&a, &b, &c)) return 0;
    ULONGLONG i = FT(a), k = FT(b), u = FT(c);
    ULONGLONG di = i - s_idle, dk = k - s_kernel, du = u - s_user;
    s_idle = i;
    s_kernel = k;
    s_user = u;
    ULONGLONG total = dk + du;
    if (!total) return 0;
    double v = (double)(total - di) * 100.0 / (double)total;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return v;
}

static PDH_FMT_COUNTERVALUE_ITEM_W* CollectArray(PDH_HCOUNTER counter, DWORD* count) {
    DWORD size = 0;
    PDH_STATUS st = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, count, NULL);
    if ((DWORD)st != (DWORD)PDH_MORE_DATA) return NULL;
    if (size > s_bufSize) {
        free(s_buf);
        s_buf = (BYTE*)malloc(size);
        if (!s_buf) { s_bufSize = 0; return NULL; }
        s_bufSize = size;
    }
    st = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, count, (PDH_FMT_COUNTERVALUE_ITEM_W*)s_buf);
    if (st != ERROR_SUCCESS) return NULL;
    return (PDH_FMT_COUNTERVALUE_ITEM_W*)s_buf;
}

// ACPI 热区（perfthermalzone）。这是免提权进程唯一能拿到的片上热读数。
// 优先用 High Precision Temperature（0.1 K），不存在时回退到 Temperature（1 K），
// 统一到 ℃。实测同一热区两者恒差 10 倍，没有单位歧义。
static double AcpiZoneC() {
    if (!s_cThermal) return 0;
    if (PdhCollectQueryData(s_qThermal) != ERROR_SUCCESS) return 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W* items = CollectArray(s_cThermal, &n);
    if (!items) return 0;
    double best = 0;
    for (DWORD i = 0; i < n; i++) {
        double v = items[i].FmtValue.doubleValue;
        double c;
        if (s_thermalHighRes) {
            c = v / 10.0 - 273.15;                       // 0.1 K → ℃
        } else if (v >= 200.0 && v <= 400.0) {
            c = v - 273.15;                              // 1 K → ℃
        } else if (v >= 0.0 && v <= 150.0) {
            c = v;                                       // 摄氏
        } else {
            continue;                                    // 无意义的哨兵值
        }
        if (c > best && c < 130) best = c;
    }
    return best;
}

// CPU 片上最高温度。
// 取两个同片传感器的较大值：
//   1) ACPI 热区 —— EC/PMF 从 SoC 转出来的温度，免提权可得
//   2) iGPU 温度传感器 —— 同一颗芯片上的真实二极管，D3DKMT 直读（已在 GpuTempC 取到，零额外开销）
// Tctl/Tdie 的定义本来就是"片上各传感器里最高的那个"，而 APU 的 CPU 与 iGPU 共用一颗裸片，
// 所以两者取大是当前不加载内核驱动前提下最接近真实片上峰值的口径。
// （真正的 AMD Tctl 寄存器 SMN 0x00059800 只能 ring0 读；本机 AMD Ryzen Master 驱动的
//   设备 ACL 限制为管理员，普通用户打不开，因此退而使用 PDH + D3DKMT 这两条免提权通道。）
static double CpuDieC(double gpuTemp) {
    double t = AcpiZoneC();
    if (gpuTemp > t) t = gpuTemp;
    return (t > 0 && t < 130) ? t : 0;
}

// 峰值保持：报最近 s_holdMs 窗口内的最大瞬时值，而不是碰巧采到的那一个点。
// 采样间隔通常 1s，瞬时点采很容易刚好落在两次热量尖峰之间。
static double PeakHold(double now) {
    ULONGLONG t = GetTickCount64();
    if (s_holdMs <= 0) {
        s_cpuPeak = now;
        s_cpuPeakAt = t;
        return now;
    }
    if (now > s_cpuPeak || (t - s_cpuPeakAt) > (ULONGLONG)s_holdMs) {
        s_cpuPeak = now;
        s_cpuPeakAt = t;
    }
    return s_cpuPeak;
}

void MetricsSetHoldMs(int ms) {
    if (ms < 0) ms = 0;
    if (ms > 60000) ms = 60000;
    s_holdMs = ms;
}

static double GpuUsage() {
    if (!s_cGpu) return 0;
    if (PdhCollectQueryData(s_qGpu) != ERROR_SUCCESS) return 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W* items = CollectArray(s_cGpu, &n);
    if (!items) return 0;
    double best = 0;
    for (DWORD i = 0; i < n; i++) {
        const wchar_t* key = wcsstr(items[i].szName, L"_luid_");
        if (!key) continue;
        double sum = 0;
        for (DWORD j = 0; j < n; j++) {
            WCHAR* other = wcsstr(items[j].szName, L"_luid_");
            if (other && wcscmp(other, key) == 0) sum += items[j].FmtValue.doubleValue;
        }
        if (sum > best) best = sum;
    }
    if (best > 100) best = 100;
    return best;
}

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

static double MaxCounter(PDH_HQUERY query, PDH_HCOUNTER counter) {
    if (!counter) return 0;
    if (PdhCollectQueryData(query) != ERROR_SUCCESS) return 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W* items = CollectArray(counter, &n);
    if (!items) return 0;
    double best = 0;
    for (DWORD i = 0; i < n; i++)
        if (items[i].FmtValue.doubleValue > best) best = items[i].FmtValue.doubleValue;
    return best;
}

static void D3dInit() {
    HMODULE g = GetModuleHandleW(L"gdi32.dll");
    if (!g) return;
    PFN_OPEN_GDINAME open = (PFN_OPEN_GDINAME)(void*)GetProcAddress(g, "D3DKMTOpenAdapterFromGdiDisplayName");
    s_queryAdapter = (PFN_QUERY_ADAPTER)(void*)GetProcAddress(g, "D3DKMTQueryAdapterInfo");
    if (!open || !s_queryAdapter) return;
    DISPLAY_DEVICEW dd;
    memset(&dd, 0, sizeof(dd));
    dd.cb = sizeof(dd);
    bool found = false;
    for (DWORD i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) { found = true; break; }
    }
    if (!found) return;
    OPEN_GDINAME o;
    memset(&o, 0, sizeof(o));
    wcsncpy(o.DeviceName, dd.DeviceName, 31);
    if (open(&o) == 0) s_hAdapter = o.hAdapter;
}

static double GpuTempC() {
    if (!s_hAdapter || !s_queryAdapter) return 0;
    ADAPTER_PERFDATA pd;
    memset(&pd, 0, sizeof(pd));
    QUERY_INFO q;
    q.hAdapter = s_hAdapter;
    q.Type = KMTQAITYPE_ADAPTERPERFDATA;
    q.pData = &pd;
    q.Size = sizeof(pd);
    if (s_queryAdapter(&q) != 0 || pd.Temperature == 0 || pd.Temperature >= 1500) return 0;
    return pd.Temperature / 10.0;
}

static void NetRates(double* down, double* up) {
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

static void Init() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s_threads = (int)si.dwNumberOfProcessors;
    if (PdhOpenQueryW(NULL, 0, &s_qThermal) == ERROR_SUCCESS) {
        // 优先用 0.1 K 精度的计数器，没有就回退到 1 K 版本。
        if (PdhAddEnglishCounterW(s_qThermal, L"\\Thermal Zone Information(*)\\High Precision Temperature", 0, &s_cThermal) == ERROR_SUCCESS) {
            s_thermalHighRes = true;
        } else if (PdhAddEnglishCounterW(s_qThermal, L"\\Thermal Zone Information(*)\\Temperature", 0, &s_cThermal) != ERROR_SUCCESS) {
            s_cThermal = NULL;
        }
        PdhCollectQueryData(s_qThermal);
    }
    if (PdhOpenQueryW(NULL, 0, &s_qGpu) == ERROR_SUCCESS) {
        if (PdhAddEnglishCounterW(s_qGpu, L"\\GPU Engine(*)\\Utilization Percentage", 0, &s_cGpu) != ERROR_SUCCESS)
            s_cGpu = NULL;
        PdhCollectQueryData(s_qGpu);
    }
    if (PdhOpenQueryW(NULL, 0, &s_qVram) == ERROR_SUCCESS) {
        if (PdhAddEnglishCounterW(s_qVram, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &s_cVram) != ERROR_SUCCESS)
            s_cVram = NULL;
        PdhCollectQueryData(s_qVram);
    }
    PrimeCpu();
    double d0 = 0, u0 = 0;
    NetRates(&d0, &u0);
    D3dInit();
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
    m->cpu = CpuPercent();
    m->gpu = GpuUsage();
    m->gpuTemp = GpuTempC();
    m->cpuTempNow = CpuDieC(m->gpuTemp);
    m->cpuTemp = PeakHold(m->cpuTempNow);
    m->vramMB = (unsigned long long)(MaxCounter(s_qVram, s_cVram) / 1048576.0);
    NetRates(&m->down, &m->up);
    m->selfMem = SelfMemBytes();
    return true;
}
