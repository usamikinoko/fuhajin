#include "pdh.h"
#include <stdlib.h>

static BYTE* s_buf;
static DWORD s_bufSize;

int PdhOpenFirst(PDH_HQUERY* q, PDH_HCOUNTER* c, const wchar_t* const* paths, int n) {
    *q = NULL;
    *c = NULL;
    PDH_HQUERY query;
    if (PdhOpenQueryW(NULL, 0, &query) != ERROR_SUCCESS) return -1;
    for (int i = 0; i < n; i++) {
        PDH_HCOUNTER counter;
        if (PdhAddEnglishCounterW(query, paths[i], 0, &counter) == ERROR_SUCCESS) {
            PdhCollectQueryData(query);
            *q = query;
            *c = counter;
            return i;
        }
    }
    PdhCloseQuery(query);
    return -1;
}

bool PdhCollect(PDH_HQUERY q) {
    return q && PdhCollectQueryData(q) == ERROR_SUCCESS;
}

PDH_FMT_COUNTERVALUE_ITEM_W* PdhCollectArray(PDH_HCOUNTER c, DWORD* count) {
    if (!c) return NULL;
    DWORD size = 0;
    PDH_STATUS st = PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &size, count, NULL);
    if ((DWORD)st != (DWORD)PDH_MORE_DATA) return NULL;
    if (size > s_bufSize) {
        free(s_buf);
        s_buf = (BYTE*)malloc(size);
        if (!s_buf) { s_bufSize = 0; return NULL; }
        s_bufSize = size;
    }
    st = PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &size, count, (PDH_FMT_COUNTERVALUE_ITEM_W*)s_buf);
    if (st != ERROR_SUCCESS) return NULL;
    return (PDH_FMT_COUNTERVALUE_ITEM_W*)s_buf;
}

double PdhMax(PDH_HQUERY q, PDH_HCOUNTER c) {
    if (!c || !PdhCollect(q)) return 0;
    DWORD n = 0;
    PDH_FMT_COUNTERVALUE_ITEM_W* items = PdhCollectArray(c, &n);
    if (!items) return 0;
    double best = 0;
    for (DWORD i = 0; i < n; i++)
        if (items[i].FmtValue.doubleValue > best) best = items[i].FmtValue.doubleValue;
    return best;
}
