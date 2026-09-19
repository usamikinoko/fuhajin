#include "rows.h"
#include "theme.h"
#include <wchar.h>

static const double TEMP_WARN = 75.0;
static const double TEMP_HOT = 88.0;

static COLORREF TempColor(double t) {
    if (t >= TEMP_HOT) return ThemeGet().hot;
    if (t >= TEMP_WARN) return ThemeGet().warn;
    return ThemeGet().ok;
}

static void FormatRate(double v, wchar_t* out) {
    if (v < 1024) swprintf(out, 32, L"%.0f B/s", v);
    else if (v < 1048576) swprintf(out, 32, L"%.1f KB/s", v / 1024.0);
    else swprintf(out, 32, L"%.2f MB/s", v / 1048576.0);
}

static void FormatMem(unsigned long long b, wchar_t* out) {
    if (b >= 1073741824ULL) swprintf(out, 32, L"%.2f GB", b / 1073741824.0);
    else if (b >= 1048576ULL) swprintf(out, 32, L"%llu MB", (b + 524288ULL) / 1048576ULL);
    else swprintf(out, 32, L"%llu KB", (b + 512ULL) / 1024ULL);
}

void RowsBuild(const Metrics& m, bool valid, RowText out[ROWS]) {
    const Theme& th = ThemeGet();

    wcscpy(out[0].label, L"CPU");
    wsprintfW(out[0].value, L"%d%% · Thd %d", valid ? (int)(m.cpu + 0.5) : 0, m.threads);
    out[0].color = th.value;

    wcscpy(out[1].label, L"CPUT");
    if (valid && m.cpuTemp > 0) {
        // 主数字是峰值保持值；正在保持时（当前瞬时值明显低于它）补一个 ▲，
        // 免得用户以为读数下不去。
        if (m.cpuTempNow > 0 && m.cpuTemp - m.cpuTempNow >= 3.0)
            swprintf(out[1].value, 32, L"%d℃ ▲", (int)(m.cpuTemp + 0.5));
        else
            swprintf(out[1].value, 32, L"%d℃", (int)(m.cpuTemp + 0.5));
        out[1].color = TempColor(m.cpuTemp);
    } else {
        wcscpy(out[1].value, L"--");
        out[1].color = th.label;
    }

    wcscpy(out[2].label, L"GPU");
    int gpuPct = valid ? (int)(m.gpu + 0.5) : 0;
    if (m.vramMB >= 1024)
        swprintf(out[2].value, 32, L"%d%% · %.1f GB", gpuPct, m.vramMB / 1024.0);
    else
        swprintf(out[2].value, 32, L"%d%% · %u MB", gpuPct, (unsigned)m.vramMB);
    out[2].color = th.value;

    wcscpy(out[3].label, L"GPUT");
    if (valid && m.gpuTemp > 0) {
        wsprintfW(out[3].value, L"%d℃", (int)(m.gpuTemp + 0.5));
        out[3].color = TempColor(m.gpuTemp);
    } else {
        wcscpy(out[3].value, L"--");
        out[3].color = th.label;
    }

    wcscpy(out[4].label, L"SELF");
    if (valid) FormatMem(m.selfMem, out[4].value);
    else wcscpy(out[4].value, L"--");
    out[4].color = th.value;

    wcscpy(out[5].label, L"DL");
    if (valid) FormatRate(m.down, out[5].value);
    else wcscpy(out[5].value, L"--");
    out[5].color = th.value;

    wcscpy(out[6].label, L"UL");
    if (valid) FormatRate(m.up, out[6].value);
    else wcscpy(out[6].value, L"--");
    out[6].color = th.value;
}
