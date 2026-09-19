#include "cpu.h"
#include "pdh.h"
#include <string.h>

static ULONGLONG s_idle, s_kernel, s_user;
static PDH_HQUERY s_qThermal;
static PDH_HCOUNTER s_cThermal;
static bool s_thermalHighRes;
static double s_peak;
static ULONGLONG s_peakAt;
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

double CpuUsagePercent() {
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

// ACPI 热区（perfthermalzone）。这是免提权进程唯一能拿到的片上热读数。
// 优先用 High Precision Temperature（0.1 K），不存在时回退到 Temperature（1 K），
// 统一到 ℃。实测同一热区两者恒差 10 倍，没有单位歧义。
static double AcpiZoneC() {
    if (!PdhCollect(s_qThermal)) return 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W* items = PdhCollectArray(s_cThermal, &n);
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
//   2) iGPU 温度传感器 —— 同一颗芯片上的真实二极管，D3DKMT 直读（已在 gpu.cpp 取到，零额外开销）
// Tctl/Tdie 的定义本来就是"片上各传感器里最高的那个"，而 APU 的 CPU 与 iGPU 共用一颗裸片，
// 所以两者取大是当前不加载内核驱动前提下最接近真实片上峰值的口径。
// （真正的 AMD Tctl 寄存器 SMN 0x00059800 只能 ring0 读；本机 AMD Ryzen Master 驱动的
//   设备 ACL 限制为管理员，普通用户打不开，因此退而使用 PDH + D3DKMT 这两条免提权通道。）
double CpuDieC(double gpuTempC) {
    double t = AcpiZoneC();
    if (gpuTempC > t) t = gpuTempC;
    return (t > 0 && t < 130) ? t : 0;
}

// 峰值保持：报最近 s_holdMs 窗口内的最大瞬时值，而不是碰巧采到的那一个点。
// 采样间隔通常 1s，瞬时点采很容易刚好落在两次热量尖峰之间。
double CpuPeakHold(double now) {
    ULONGLONG t = GetTickCount64();
    if (s_holdMs <= 0) {
        s_peak = now;
        s_peakAt = t;
        return now;
    }
    if (now > s_peak || (t - s_peakAt) > (ULONGLONG)s_holdMs) {
        s_peak = now;
        s_peakAt = t;
    }
    return s_peak;
}

void CpuSetHoldMs(int ms) {
    if (ms < 0) ms = 0;
    if (ms > 60000) ms = 60000;
    s_holdMs = ms;
}

void CpuInit() {
    const wchar_t* paths[2] = {
        L"\\Thermal Zone Information(*)\\High Precision Temperature",   // 0.1 K
        L"\\Thermal Zone Information(*)\\Temperature"                    // 1 K（回退）
    };
    s_thermalHighRes = (PdhOpenFirst(&s_qThermal, &s_cThermal, paths, 2) == 0);
    PrimeCpu();
}
