#include "render.h"
#include "layout.h"
#include "rows.h"
#include "theme.h"
#include "../app/settings.h"
#include <math.h>
#include <string.h>

// 三张等大离屏位图：
//   base  —— 背板（底色 + 圆角边框）
//   textA —— 文字层，黑底。合成后它就是提交给分层窗口的最终像素
//   textB —— 文字层，白底。只用于还原文字的抗锯齿覆盖率
// 拿黑底/白底两版相减求覆盖率，是为了不依赖 GDI 的任何 alpha 输出。
struct Canvas {
    HDC dc;
    HBITMAP bm;
    BYTE* px;
};

static HDC s_screen;
static HFONT s_font;
static int s_w, s_h;
static Canvas s_base, s_textA, s_textB;

static bool CanvasCreate(Canvas* c, int w, int h) {
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;          // 负高度 = 自上而下，与 GDI 绘制方向一致
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    c->bm = CreateDIBSection(s_screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    c->dc = CreateCompatibleDC(s_screen);
    SelectObject(c->dc, c->bm);
    c->px = (BYTE*)bits;
    return c->bm && c->dc && c->px;
}

static void CanvasDestroy(Canvas* c) {
    if (c->dc) { DeleteDC(c->dc); c->dc = NULL; }
    if (c->bm) { DeleteObject(c->bm); c->bm = NULL; }
    c->px = NULL;
}

// 背板不透明度：实体卡片全不透明；半透明则 1 - 透明度。
// 注意只有背板参与透明，文字始终不透明 —— 半透明卡片上文字依旧要看得清。
static int BaseAlpha() {
    if (g_set.effect != EFFECT_ALPHA) return 255;
    int t = g_set.alpha;
    if (t < 0) t = 0;
    if (t > 100) t = 100;
    return (255 * (100 - t) + 50) / 100;
}

void RenderFree() {
    CanvasDestroy(&s_base);
    CanvasDestroy(&s_textA);
    CanvasDestroy(&s_textB);
    if (s_font) { DeleteObject(s_font); s_font = NULL; }
}

void RenderRebuild() {
    RenderFree();
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -SC(FS());
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    s_font = CreateFontIndirectW(&lf);

    s_w = CARD_W();
    s_h = CARD_H();
    CanvasCreate(&s_base, s_w, s_h);
    CanvasCreate(&s_textA, s_w, s_h);
    CanvasCreate(&s_textB, s_w, s_h);
}

void RenderInit() {
    s_screen = GetDC(NULL);
    RenderRebuild();
}

static void DrawBase() {
    RECT full = { 0, 0, s_w, s_h };
    HBRUSH b = CreateSolidBrush(ThemeGet().bg);
    FillRect(s_base.dc, &full, b);
    DeleteObject(b);

    HPEN pen = CreatePen(PS_SOLID, 1, ThemeGet().border);
    HGDIOBJ op = SelectObject(s_base.dc, pen);
    HGDIOBJ ob = SelectObject(s_base.dc, GetStockObject(NULL_BRUSH));
    RoundRect(s_base.dc, 0, 0, s_w - 1, s_h - 1, CORNER() * 2, CORNER() * 2);
    SelectObject(s_base.dc, op);
    SelectObject(s_base.dc, ob);
    DeleteObject(pen);
}

// 同一份文字画两遍，只换底：黑底那版留着用，白底那版用来算覆盖率。
static void DrawTextLayer(HDC dc, COLORREF backdrop, const RowText rows[ROWS]) {
    RECT full = { 0, 0, s_w, s_h };
    HBRUSH b = CreateSolidBrush(backdrop);
    FillRect(dc, &full, b);
    DeleteObject(b);

    SetBkMode(dc, OPAQUE);
    SetBkColor(dc, backdrop);
    HGDIOBJ of = SelectObject(dc, s_font);
    int y = PAD();
    for (int i = 0; i < ROWS; i++) {
        RECT r;
        r.left = PAD();
        r.right = PAD() + LBL_W();
        r.top = y;
        r.bottom = y + ROW_H();
        SetTextColor(dc, ThemeGet().label);
        DrawTextW(dc, rows[i].label, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        r.left = r.right;
        r.right = s_w - PAD();
        SetTextColor(dc, rows[i].color);
        DrawTextW(dc, rows[i].value, -1, &r, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        y += ROW_H();
    }
    SelectObject(dc, of);
    SetBkMode(dc, TRANSPARENT);
}

static BYTE Over(BYTE src, BYTE dst, double k) {
    double v = src + dst * k;
    if (v > 255.0) v = 255.0;
    return (BYTE)(v + 0.5);
}

static void Composite() {
    const double aRow = BaseAlpha() / 255.0;
    const int R = CORNER();
    for (int y = 0; y < s_h; y++) {
        BYTE* pa = s_textA.px + (size_t)y * s_w * 4;
        BYTE* pb = s_textB.px + (size_t)y * s_w * 4;
        BYTE* pbase = s_base.px + (size_t)y * s_w * 4;
        for (int x = 0; x < s_w; x++) {
            // 圆角处按解析式算覆盖率，省掉一次遮罩位图
            double cc = 1.0;
            double cx = -1.0, cy = -1.0;
            if (x < R && y < R) { cx = R; cy = R; }
            else if (x >= s_w - R && y < R) { cx = s_w - R; cy = R; }
            else if (x < R && y >= s_h - R) { cx = R; cy = s_h - R; }
            else if (x >= s_w - R && y >= s_h - R) { cx = s_w - R; cy = s_h - R; }
            if (cx >= 0) {
                double dx = (double)x + 0.5 - cx;
                double dy = (double)y + 0.5 - cy;
                cc = R - sqrt(dx * dx + dy * dy) + 0.5;
                if (cc < 0) cc = 0;
                if (cc > 1) cc = 1;
            }
            BYTE* qa = pa + x * 4;
            BYTE* qb = pb + x * 4;
            BYTE* qbase = pbase + x * 4;
            // 黑底与白底两版同一像素的通道和之差 = 765 * (1 - 文字覆盖率)
            double sa = qa[0] + qa[1] + qa[2];
            double sb = qb[0] + qb[1] + qb[2];
            double cov = 1.0 - (sb - sa) / 765.0;
            if (cov < 0) cov = 0;
            if (cov > 1) cov = 1;
            double da = aRow * cc;
            double k = da * (1.0 - cov);
            qa[0] = Over(qa[0], qbase[0], k);
            qa[1] = Over(qa[1], qbase[1], k);
            qa[2] = Over(qa[2], qbase[2], k);
            double av = (cov + da * (1.0 - cov)) * 255.0;
            qa[3] = av > 255.0 ? 255 : (BYTE)(av + 0.5);
        }
    }
}

static void Commit(HWND hwnd) {
    POINT d = LayoutCardPos();
    POINT s = { 0, 0 };
    SIZE sz = { s_w, s_h };
    SIZE sz1 = { s_w + 1, s_h + 1 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    // 先按 +1 尺寸提交一次再回到实际尺寸：尺寸不变时 UpdateLayeredWindow 容易不刷新，
    // 这一步是原实现留下的规避手段，别删。
    UpdateLayeredWindow(hwnd, s_screen, &d, &sz1, s_textA.dc, &s, 0, &bf, ULW_ALPHA);
    UpdateLayeredWindow(hwnd, s_screen, &d, &sz, s_textA.dc, &s, 0, &bf, ULW_ALPHA);
}

void RenderCard(HWND hwnd, const Metrics& m, bool valid) {
    RowText rows[ROWS];
    RowsBuild(m, valid, rows);
    DrawBase();
    DrawTextLayer(s_textA.dc, RGB(0, 0, 0), rows);
    DrawTextLayer(s_textB.dc, RGB(255, 255, 255), rows);
    Composite();
    Commit(hwnd);
}
