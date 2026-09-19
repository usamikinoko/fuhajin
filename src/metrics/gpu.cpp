#include "gpu.h"
#include "pdh.h"
#include <string.h>

static PDH_HQUERY s_qGpu, s_qVram;
static PDH_HCOUNTER s_cGpu, s_cVram;

// ---- D3DKMT：拿 iGPU 片上温度的唯免提权通道 ----
// mingw 没有 d3dkmthk.h，结构体与函数指针全部手写，运行时从 gdi32.dll 取。
// 两个坑（都踩过，改动时别推翻）：
//   1) ADAPTER_PERFDATA 手写后的 sizeof 必须 = 64，多一个字段就返回错误；
//   2) D3DKMTOpenAdapterFromGdiDisplayName 的 DeviceName 是 WCHAR[32] 数组，
//      指针版叫 D3DKMTOpenAdapterFromDeviceName —— 用错返回 STATUS_INVALID_PARAMETER。
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
static_assert(sizeof(ADAPTER_PERFDATA) == 64, "D3DKMT 要求该结构体恰为 64 字节");

typedef LONG(NTAPI* PFN_OPEN_GDINAME)(OPEN_GDINAME*);
typedef LONG(NTAPI* PFN_QUERY_ADAPTER)(QUERY_INFO*);

static const UINT32 KMTQAITYPE_ADAPTERPERFDATA = 62;

static HAD s_hAdapter;
static PFN_QUERY_ADAPTER s_queryAdapter;

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

double GpuTempC() {
    if (!s_hAdapter || !s_queryAdapter) return 0;
    ADAPTER_PERFDATA pd;
    memset(&pd, 0, sizeof(pd));
    QUERY_INFO q;
    q.hAdapter = s_hAdapter;
    q.Type = KMTQAITYPE_ADAPTERPERFDATA;
    q.pData = &pd;
    q.Size = sizeof(pd);
    // Temperature 单位是 0.1℃，且无传感器时常回 0 或哨兵大值。
    if (s_queryAdapter(&q) != 0 || pd.Temperature == 0 || pd.Temperature >= 1500) return 0;
    return pd.Temperature / 10.0;
}

// 实例名形如 pid_N_luid_..._eng_N_engtype_X，一个 (进程, 引擎) 一个实例。
//
// 已知口径问题（待改，本轮保持原行为不动）：这里把同一 LUID 下的所有实例直接求和，
// 等于"跨进程跨引擎全加"，会让 VideoEncode 之类的引擎叠进来、读数系统性虚高。
// 与任务管理器一致的口径应当是：先按 engtype 分组、跨进程求和，再取各 engtype 的最大值。
double GpuUsagePercent() {
    if (!PdhCollect(s_qGpu)) return 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W* items = PdhCollectArray(s_cGpu, &n);
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

unsigned long long GpuVramMB() {
    return (unsigned long long)(PdhMax(s_qVram, s_cVram) / 1048576.0);
}

void GpuInit() {
    const wchar_t* engine[1] = { L"\\GPU Engine(*)\\Utilization Percentage" };
    const wchar_t* vram[1] = { L"\\GPU Adapter Memory(*)\\Dedicated Usage" };
    PdhOpenFirst(&s_qGpu, &s_cGpu, engine, 1);
    PdhOpenFirst(&s_qVram, &s_cVram, vram, 1);
    D3dInit();
}
