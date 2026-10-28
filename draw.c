// draw.c — the GBA's screen: 240x160, one byte (a palette colour) per pixel, two pages
// (one shown while the other is drawn). Video memory only takes 16-bit writes, so a single
// pixel is read, changed and written back as a pair.
#include "qu.h"

// ── maths (the GBA has no floating-point hardware: keep these light) ──────
float fsqrt(float v) {
    if (v <= 0) return 0;
    union { float f; u32 i; } u = { v }; u.i = 0x1FBD1DF5 + (u.i >> 1);
    float x = u.f; x = 0.5f * (x + v / x); x = 0.5f * (x + v / x); return 0.5f * (x + v / x);
}
float fabsf_(float v) { return v < 0 ? -v : v; }
static float wrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
float fsin(float a) { a = wrapPi(a); float y = 1.2732395f * a - 0.4052847f * a * fabsf_(a); return 0.225f * (y * fabsf_(y) - y) + y; }
float fcos(float a) { return fsin(a + 1.5707963f); }
float frand(void) { return (rand() % 1000) / 1000.0f; }
float fatan2r(float y, float x) {
    float ax = fabsf_(x), ay = fabsf_(y);
    if (ax < 1e-6f && ay < 1e-6f) return 0;
    float a = (ax < ay ? ax / ay : ay / ax), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}

// ── palettes ──────────────────────────────────────────────────────────────
static u16 mix15(u16 a, u16 b, int k, int n) {
    int ar = a & 31, ag = (a >> 5) & 31, ab = (a >> 10) & 31, br = b & 31, bg = (b >> 5) & 31, bb = (b >> 10) & 31;
    return RGB15(ar + (br - ar) * k / n, ag + (bg - ag) * k / n, ab + (bb - ab) * k / n);
}
void uiPalette(void) {
    static const u16 P[] = {
        [C_BLACK] = RGB15(0, 0, 0), [C_WHITE] = RGB15(31, 31, 31), [C_DARK] = RGB15(2, 2, 3), [C_YELLOW] = RGB15(31, 27, 4),
        [C_GOLD] = RGB15(31, 24, 2), [C_GREY] = RGB15(18, 18, 18), [C_RED] = RGB15(31, 6, 6), [C_LIME] = RGB15(8, 31, 8),
        [C_SEL] = RGB15(31, 26, 4), [C_DIMEDGE] = RGB15(15, 15, 15), [C_SILVER] = RGB15(24, 24, 26), [C_BRONZE] = RGB15(26, 15, 7),
        [C_PANEL] = RGB15(2, 2, 4), [C_PANELLINE] = RGB15(9, 9, 12), [C_TOAST] = RGB15(3, 3, 1), [C_SHINE] = RGB15(15, 15, 15),
        [C_FR] = RGB15(26, 3, 5), [C_FB] = RGB15(2, 6, 18), [C_FG] = RGB15(2, 16, 8), [C_FY] = RGB15(31, 26, 0),
        [C_FO] = RGB15(31, 16, 2), [C_FL] = RGB15(4, 12, 28), [C_MGREEN] = RGB15(6, 28, 8), [C_MRED] = RGB15(31, 6, 6),
        [C_MBLUE] = RGB15(8, 12, 31), [C_POWER] = RGB15(31, 31, 31), [C_NIB] = RGB15(31, 28, 6), [C_PWDARK] = RGB15(1, 1, 1),
        [C_PWOFF] = RGB15(10, 10, 10), [C_CYAN] = RGB15(17, 31, 29), [C_LBLUE] = RGB15(17, 26, 31), [C_DIMWHITE] = RGB15(23, 23, 23),
    };
    for (int i = 0; i < 64; i++) bgPal[i] = i < (int)(sizeof P / sizeof P[0]) ? P[i] : 0;
    // the buttons' body: the website's dark gradient, top to bottom
    u16 top = RGB15(7, 7, 7), mid = RGB15(2, 2, 2), end = RGB15(0, 0, 0);
    for (int i = 0; i < 32; i++) bgPal[C_GRAD0 + i] = i < 15 ? mix15(top, mid, i, 15) : mix15(mid, end, i - 15, 17);
}
void scenePalette(int which) {
    static int cur = -1;
    if (cur == which) return;
    cur = which;
    memcpy(&bgPal[64], which >= PAL_BALL ? bb_pal[which - PAL_BALL] : which == PAL_FIELD ? field_pal : which == PAL_SLOT ? slot_pal : which == PAL_MINI ? minit_pal : which == PAL_FIDGET ? fd_pal : which == PAL_BOWL ? bwl_pal : which == PAL_STACK ? stw_pal : which == PAL_FLIP ? fl_pal : which == PAL_DOZER ? dz_pal8 : which == PAL_PIN ? pb_pal8 : which == PAL_JUMP ? jump_pal : logo_pal, 192 * 2);
}

// ── pixels ────────────────────────────────────────────────────────────────
int clipX0 = 0, clipX1 = SW, clipY0 = 0, clipY1 = SH;
void clipAll(void) { clipX0 = 0; clipX1 = SW; clipY0 = 0; clipY1 = SH; }
// (a macro, not a function: the fast-RAM drawing code below must not call out to the cartridge)
#define put(x, y, c) do { int x_ = (x); u16 *p_ = &page[((y) * SW + x_) >> 1]; int c_ = (c); \
    *p_ = (x_ & 1) ? PXR(*p_, c_) : PXL(*p_, c_); } while (0)
static void pset_(int x, int y, int c) { if (x >= clipX0 && x < clipX1 && y >= clipY0 && y < clipY1) put(x, y, c); }
IWRAM_CODE static void hfill(int y, int x0, int x1, int c) {          // [x0, x1), already clipped (fast RAM)
    if (x0 >= x1) return;
    if (x0 & 1) { put(x0, y, c); x0++; }
    if (x0 >= x1) return;
    if (x1 & 1) { x1--; put(x1, y, c); }
    u16 cc = (u16)(c | (c << 8)), *d = &page[(y * SW + x0) >> 1];
    for (int n = (x1 - x0) >> 1; n > 0; n--) *d++ = cc;
}
// one row from x0 to x1 (exclusive), clipped; drawn at once, never listed: only for screens
// that draw straight into the page (after drawDirect) (fast RAM)
IWRAM_CODE void hspan(int y, int x0, int x1, int c) {
    if (y < clipY0 || y >= clipY1) return;
    if (x0 < clipX0) x0 = clipX0;
    if (x1 > clipX1) x1 = clipX1;
    hfill(y, x0, x1, c);
}
// a row of diagonal stripes, w wide, alternating c0/c1, shifted by the row (fast RAM)
IWRAM_CODE void hstripe(int y, int x0, int x1, int w, int c0, int c1) {
    if (y < clipY0 || y >= clipY1 || w < 1) return;
    int k = 0, ph = x0 + y; while (ph >= w) { ph -= w; k++; }       // (x0 + y) / w, without a divide
    int x = x0, e = x0 + (w - ph);
    while (x < x1) { int x2 = e < x1 ? e : x1, a = x < clipX0 ? clipX0 : x, b = x2 > clipX1 ? clipX1 : x2; if (a < b) hfill(y, a, b, k & 1 ? c1 : c0); x = x2; k++; e += w; }
}
static void rect_(int x, int y, int w, int h, int c) {
    int x0 = x < clipX0 ? clipX0 : x, x1 = x + w > clipX1 ? clipX1 : x + w;
    int y0 = y < clipY0 ? clipY0 : y, y1 = y + h > clipY1 ? clipY1 : y + h;
    for (int yy = y0; yy < y1; yy++) hfill(yy, x0, x1, c);
}
static void fillScreen_(int c) { u32 cc = c * 0x01010101u; fill32(page, cc, SW * SH / 4); }
void box(int x, int y, int w, int h, int fill, int edge) { rect(x, y, w, h, edge); rect(x + 2, y + 2, w - 4, h - 4, fill); }

static void shape(int x, int y, int w, int h, int rl, int rr, int grad, int c);
// ── the website's marker shape (body + cap), used by buttons and the power marker ──
static int inset(int j, int h, int r) {
    int d = j < r ? r - j : (j >= h - r ? j - (h - 1 - r) : 0);
    if (d <= 0) return 0;
    int i = 0; while (i < r && (r - i) * (r - i) + d * d > r * r + r) i++;
    return i;
}
// rounded left (rl) / right (rr) ends; each row's colour from the gradient (grad) or c
static void shape_(int x, int y, int w, int h, int rl, int rr, int grad, int c) {
    for (int j = 0; j < h; j++) { int yy = y + j; if (yy < clipY0 || yy >= clipY1) continue;
        int x0 = x + inset(j, h, rl), x1 = x + w - inset(j, h, rr);
        if (x0 < clipX0) x0 = clipX0;
        if (x1 > clipX1) x1 = clipX1;
        hfill(yy, x0, x1, grad ? C_GRAD0 + j * 32 / h : c); }
}
static void markerShapeCap(int x, int y, int w, int h, int cap, int edge) {
    int rc = h / 2 - 1, rb = h / 4; if (rc > cap) rc = cap;
    shape(x, y, w, h, rc, rb, 0, edge);                                        // outline
    shape(x + 2, y + 2, cap - 3, h - 4, rc - 2 > 1 ? rc - 2 : 1, 1, 1, 0);       // the cap
    shape(x + cap + 1, y + 2, w - cap - 3, h - 4, 1, rb - 2 > 1 ? rb - 2 : 1, 1, 0);   // the body
    if (cap > 8) rect(x + 3 + rc / 2, y + 2, cap - 5 - rc / 2, 1, C_SHINE);    // the shine along the top
    rect(x + cap + 2, y + 2, w - cap - 3 - rb, 1, C_SHINE);
}

// ── text (DejaVu Sans Bold glyphs, as the DS), dark outline for reading on photos ──
static int squeeze;                          // pixels taken off each letter's spacing (long labels)
int textW(const char *t, int sc) { int w = 0; for (; *t; t++) { int ch = *t; if (ch < 32 || ch > 126) ch = '?'; w += (font_w[ch - 32] - squeeze) * sc; } return w; }
// One row of a letter into the page two pixels (one 16-bit word) at a time. Bit b of the masks
// is the pixel at base + b; "all" is every pixel drawn, "body" the ones in colour col (the rest
// are its dark outline). Clipped to [cx0, cx1).
static inline __attribute__((always_inline)) void rowPairs(int py, int base, u32 body, u32 all, int col, int cx0, int cx1) {
    if (base < cx0) { int k = cx0 - base; if (k >= 32) return; all &= ~((1u << k) - 1); }
    if (cx1 - base < 32) { int k = cx1 - base; if (k <= 0) return; all &= (1u << k) - 1; }
    if (!all) return;
    if (base & 1) { base--; all <<= 1; body <<= 1; }
    u16 *d = &page[(py * SW + base) >> 1];
    u32 cb = (u32)col, ck = C_BLACK;
    for (; all; all >>= 2, body >>= 2, d++) {
        u32 two = all & 3; if (!two) continue;
        u32 lo = body & 1 ? cb : ck, hi = body & 2 ? cb : ck;
        if (two == 3) *d = PX2(lo, hi);
        else if (two == 1) *d = PXL(*d, lo);
        else *d = PXR(*d, hi);
    }
}
IWRAM_CODE static void glyphs(int x, int y, const char *t, int col, int sc) {   // (fast RAM: text is drawn a lot)
    int cx0 = clipX0, cx1 = clipX1, cy0 = clipY0, cy1 = clipY1, sq = squeeze;
    for (; *t; t++) {
        int ch = *t; if (ch < 32 || ch > 126) ch = '?';
        const u16 *rows = &font_rows[(ch - 32) * FONT_H];
        for (int j = -1; j <= FONT_H; j++) {
            u32 r = (j >= 0 && j < FONT_H) ? rows[j] : 0, up = j > 0 ? rows[j - 1] : 0, dn = j < FONT_H - 1 ? rows[j + 1] : 0;
            u32 r2 = r << 1, out = ((r2 << 1) | (r2 >> 1) | (up << 1) | (dn << 1)) & ~r2, m = out | r2;   // bit i+1 = column i
            if (!m) continue;
            if (sc == 1) { int py = y + j; if (py >= cy0 && py < cy1) rowPairs(py, x - 1, r2, m, col, cx0, cx1); continue; }
            for (int b2 = 0; b2 < sc; b2++) {
                int py = y + j * sc + b2;
                if (py < cy0 || py >= cy1) continue;
                for (int i = -1; m >> (i + 1); i++) {
                    if (!((m >> (i + 1)) & 1)) continue;
                    int c = (r2 >> (i + 1)) & 1 ? col : C_BLACK;
                    for (int a2 = 0; a2 < sc; a2++) { int px = x + i * sc + a2; if (px >= cx0 && px < cx1) put(px, py, c); }
                }
            }
        }
        x += (font_w[ch - 32] - sq) * sc;
    }
}

// centred text that always fits: too wide at double size drops to normal size (kept vertically
// centred where the big text would have been), then tightens its letters
void textCW(int x0, int w, int y, const char *t, int col, int sc) {
    if (sc > 1 && textW(t, sc) > w - 4) { y += (FONT_H * (sc - 1)) / 2; sc = 1; }
    int old = squeeze;
    while (textW(t, sc) > w - 2 && squeeze < 3) squeeze++;
    text(x0 + (w - textW(t, sc)) / 2, y, t, col, sc);
    squeeze = old;
}
void textC(int y, const char *t, int col, int sc) { textCW(0, SW, y, t, col, sc); }
int textSW(const char *t) { int w = 0; for (; *t; t++) { int ch = *t; if (ch < 32 || ch > 126) ch = '?'; w += fonts_w[ch - 32]; } return w; }
IWRAM_CODE static void textS_(int x, int y, const char *t, int col) {   // (fast RAM)
    int cx0 = clipX0, cx1 = clipX1, cy0 = clipY0, cy1 = clipY1;
    for (; *t; t++) {
        int ch = *t; if (ch < 32 || ch > 126) ch = '?';
        const u16 *rows = &fonts_rows[(ch - 32) * FONTS_H];
        for (int j = 0; j < FONTS_H; j++) { int py = y + j; if (py < cy0 || py >= cy1) continue;
            u32 m = rows[j]; if (m) rowPairs(py, x, m, m, col, cx0, cx1); }
        x += fonts_w[ch - 32];
    }
}
void scoreStr(char *o, int doubled) {
    if (doubled < 0 && (doubled & 1)) sprintf(o, "-%d.5", (-doubled) / 2);
    else if (doubled & 1) sprintf(o, "%d.5", doubled / 2);
    else sprintf(o, "%d", doubled / 2);
}
// a picture, colour 0 see-through: two pixels at a time where both show (fast RAM)
IWRAM_CODE void drawImg_(const u8 *img, int w, int h, int x, int y) {
    int cx0 = clipX0, cx1 = clipX1, cy0 = clipY0, cy1 = clipY1;
    int i0 = x < cx0 ? cx0 - x : 0, i1 = x + w > cx1 ? cx1 - x : w;
    if (i0 >= i1) return;
    for (int j = 0; j < h; j++) { int yy = y + j; if (yy < cy0 || yy >= cy1) continue;
        const u8 *s = &img[j * w];
        int i = i0;
        if ((x + i) & 1) { if (s[i]) put(x + i, yy, s[i]); i++; }
        u16 *d = &page[(yy * SW + x + i) >> 1];
        for (; i + 1 < i1; i += 2, d++) {
            int a = s[i], b = s[i + 1];
            if (a && b) *d = PX2(a, b);
            else if (a) *d = PXL(*d, a);
            else if (b) *d = PXR(*d, b);
        }
        if (i < i1 && s[i]) put(x + i, yy, s[i]);
    }
}
// darkens a box by painting every other pixel (a checkerboard) in colour c (fast RAM)
IWRAM_CODE static void stipple_(int x, int y, int w, int h, int c) {
    for (int j = 0; j < h; j++) { int yy = y + j; if (yy < clipY0 || yy >= clipY1) continue;
        for (int i = (j + x) & 1; i < w; i += 2) { int xx = x + i; if (xx >= clipX0 && xx < clipX1) put(xx, yy, c); } }
}
void drawLogo(int x, int y) { drawImg(logo8, LOGO_W, LOGO_H, x, y); }
void coinCount(int x, int y, int slotPal) {
    char s[16]; drawImg(slotPal ? scoin8 : coin8, COIN_W, COIN_H, x, y); sprintf(s, "%d", sv.coins);
    text(x + COIN_W + 3, y + 2, s, GOLD, 1);
}

// ── buttons ───────────────────────────────────────────────────────────────
// The website's marker buttons: a dark gradient body with a white outline and a rounded cap
// on the left end, like a marker pen. The highlighted one gets a gold outline and sinks 2 px
// while A is held.
void drawBtns(Btn *b, int n, int sel) {
    for (int i = 0; i < n; i++) {
        int on = (i == sel), dn = on && (kHeld & KEY_A) ? 2 : 0;
        int edge = b[i].dim ? C_DIMEDGE : (on ? C_SEL : C_WHITE);
        int c = b[i].col ? b[i].col : (b[i].dim ? GREY : (on ? YELLOW : WHITE));
        if (b[i].w < 40) {                                // little keys (the name keyboard): plain rounded keys
            shape(b[i].x, b[i].y + dn, b[i].w, b[i].h, 3, 3, 0, edge);
            shape(b[i].x + 1, b[i].y + dn + 1, b[i].w - 2, b[i].h - 2, 2, 2, 1, 0);
            text(b[i].x + (b[i].w - textW(b[i].label, 1)) / 2, b[i].y + dn + (b[i].h - FONT_H) / 2, b[i].label, c, 1);
            continue;
        }
        int cap = b[i].h * 45 / 100; if (cap < 10) cap = 10;
        // the label must sit inside the body: tighten the letters, then shrink the cap, until it does
        squeeze = 0; int tw = textW(b[i].label, 1);
        while (tw > b[i].w - cap - 8 && squeeze < 2) { squeeze++; tw = textW(b[i].label, 1); }
        while (tw > b[i].w - cap - 8 && cap > 7) cap--;
        markerShapeCap(b[i].x, b[i].y + dn, b[i].w, b[i].h, cap, edge);
        int bx = b[i].x + cap + 1, bw = b[i].w - cap - 3;
        text(bx + (bw - tw) / 2, b[i].y + dn + (b[i].h - FONT_H) / 2, b[i].label, c, 1);
        squeeze = 0;
    }
}
// D-pad moves the highlight (cols = buttons per row), A presses it
int btnInput(Btn *b, int n, int *sel, int cols) {
    (void)b;
    if (n <= 0) return -1;
    if (*sel < 0 || *sel >= n) *sel = 0;
    if (kDown & KEY_RIGHT) *sel = (*sel + 1) % n;
    if (kDown & KEY_LEFT) *sel = (*sel + n - 1) % n;
    if (kDown & KEY_DOWN) *sel = (*sel + cols) % n;
    if (kDown & KEY_UP) *sel = (*sel + n - cols % n) % n;
    if (kDown & KEY_A) return *sel;
    return -1;
}

// ── the power marker: a marker pointing where the throw goes, growing with power and filling
// from white through gold to red. (x0,y0) is the back of the cap, (ux,uy) the direction.
// Drawn into the page by a rotation in whole-number maths (16.16), only over each row's
// stretch that lands inside the picture.
static void blitRot8(const u8 *spr, int w, int h, int cx, int cy, float ang) {
    int ci = (int)(fcos(ang) * 65536.0f), si = (int)(fsin(ang) * 65536.0f);
    int r = (int)(fsqrt((float)(w * w + h * h)) / 2) + 1;
    int W = w << 16, H = h << 16;
    for (int dy = -r; dy <= r; dy++) {
        int gy = cy + dy; if (gy < clipY0 || gy >= clipY1) continue;
        int sxf = ci * (-r) + si * dy + (w << 15), syf = -si * (-r) + ci * dy + (h << 15);
        int a = -r, b = r;
        #define NARROW(v0, dv, LIM) if (dv == 0) { if (v0 < 0 || v0 >= LIM) continue; } \
            else { int lo, hi; if (dv > 0) { lo = (-(v0)) / dv - 1; hi = (LIM - 1 - (v0)) / dv + 1; } \
                   else { lo = (LIM - 1 - (v0)) / dv - 1; hi = (-(v0)) / dv + 1; } \
                   if (lo - r > a) { a = lo - r; } if (hi - r < b) { b = hi - r; } }
        NARROW(sxf, ci, W) NARROW(syf, -si, H)
        #undef NARROW
        if (cx + a < clipX0) a = clipX0 - cx;
        if (cx + b > clipX1 - 1) b = clipX1 - 1 - cx;
        if (a > b) continue;
        int k = a + r; sxf += ci * k; syf -= si * k;
        for (int dx = a; dx <= b; dx++, sxf += ci, syf -= si) {
            int ix = sxf >> 16, iy = syf >> 16;
            if ((unsigned)ix >= (unsigned)w || (unsigned)iy >= (unsigned)h) continue;
            u8 p = spr[iy * w + ix];
            if (p) put(cx + dx, gy, p);
        }
    }
}
// The website's power marker: a black marker with a thick white outline and a narrower cap
// block on the front end, pointing where the throw goes. It grows with power, and the body
// fills from the back with the power's colour (white through gold to red).
// a rounded rectangle's row j: the columns [*a, *b) it covers (or a >= b if none)
static void rrSpan(int j, int x, int y, int w, int h, int r, int *a, int *b) {
    *a = 0; *b = 0;
    if (j < y || j >= y + h) return;
    int dy = j < y + r ? y + r - j : j >= y + h - r ? j - (y + h - r - 1) : 0, in = 0;
    while (in < r && (r - in) * (r - in) + dy * dy > r * r) in++;
    if (!dy) in = 0;
    *a = x + in; *b = x + w - in;
}
static void powerMarker_(int x0, int y0, float ux, float uy, float len, float power) {
    static u8 spr[180 * 14] EWRAM_BSS;
    int L = (int)len; if (L < 22) L = 22; if (L > 180) L = 180;
    const int H = 14, ol = 2, capL = 10, capW = 11, r = 3, cy = (H - capW) / 2;
    int bodyL = L - capL + ol, fillL = power > 0 ? (int)((bodyL - 2 * ol) * power + 1) : 0;
    int pr = (int)(power * 31);
    bgPal[C_POWER] = RGB15(31, 31 - pr * 2 / 3, pr < 16 ? 31 - pr * 2 : 0);
    for (int j = 0; j < H; j++) {
        u8 *row = &spr[j * L];
        int a, b, ia, ib, ca, cb, cia, cib;
        memset(row, 0, L);
        rrSpan(j, 0, 0, bodyL, H, r, &a, &b);                              // the body: outline, then inside
        if (a < b) {
            memset(row + a, C_WHITE, b - a);
            rrSpan(j, ol, ol, bodyL - 2 * ol, H - 2 * ol, r - 1, &ia, &ib);
            if (ia < ib) { int f = ol + fillL; if (f > ib) f = ib; if (f < ia) f = ia;
                memset(row + ia, C_POWER, f - ia); memset(row + f, C_PWDARK, ib - f); }
        }
        rrSpan(j, L - capL, cy, capL, capW, 1, &ca, &cb);                  // the cap, over the body's end
        if (ca < cb) {
            memset(row + ca, C_WHITE, cb - ca);
            rrSpan(j, L - capL + ol, cy + ol, capL - 2 * ol, capW - 2 * ol, 0, &cia, &cib);
            if (cia < cib) memset(row + cia, C_PWDARK, cib - cia);
        }
    }
    float cx = x0 + ux * L / 2, cy2 = y0 + uy * L / 2;
    blitRot8(spr, L, H, (int)cx, (int)cy2, fatan2r(uy, ux));
}

// ── nation flags (simplified, drawn from shapes) ──────────────────────────
static void hstripes(int x, int y, int w, int h, const int *c, int n) { for (int i = 0; i < n; i++) rect(x, y + h * i / n, w, h * (i + 1) / n - h * i / n, c[i]); }
static void vstripes(int x, int y, int w, int h, const int *c, int n) { for (int i = 0; i < n; i++) rect(x + w * i / n, y, w * (i + 1) / n - w * i / n, h, c[i]); }
static void disc(int cx, int cy, int r, int c) { for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r) pset(cx + i, cy + j, c); }
static void nordic(int x, int y, int w, int h, int bg, int outer, int inner) {
    rect(x, y, w, h, bg);
    int cx = x + w * 3 / 8, t = h / 4;
    rect(x, y + h / 2 - t / 2 - 1, w, t + 2, outer); rect(cx - t / 2 - 1, y, t + 2, h, outer);
    if (inner != outer) { rect(x, y + h / 2 - t / 4, w, t / 2 > 0 ? t / 2 : 1, inner); rect(cx - t / 4, y, t / 2 > 0 ? t / 2 : 1, h, inner); }
}
void drawFlag(int x, int y, int w, int h, int n) {
    const int R = C_FR, W = C_WHITE, B = C_FB, G = C_FG, Y = C_FY, K = C_BLACK, O = C_FO, L = C_FL;
    switch (n) {
        case 0: { int c[3] = { G, W, O }; vstripes(x, y, w, h, c, 3); } break;                              // Ireland
        case 1: rect(x, y, w, h, W); rect(x, y + h / 6, w, h / 8 > 0 ? h / 8 : 1, L); rect(x, y + h * 5 / 6 - h / 8, w, h / 8 > 0 ? h / 8 : 1, L);   // Israel
                disc(x + w / 2, y + h / 2, h / 5, L); disc(x + w / 2, y + h / 2, h / 5 - 1, W); break;
        case 2: for (int i = 0; i < 13; i++) rect(x, y + h * i / 13, w, h / 13 + 1, i % 2 ? W : R);              // USA
                rect(x, y, w * 2 / 5, h * 7 / 13, B); break;
        case 3: { int c[3] = { R, W, B }; hstripes(x, y, w, h, c, 3); }                                          // South Africa
                rect(x, y + h * 2 / 5, w, h / 5, G);
                for (int j = 0; j < h; j++) { int e = (j < h / 2 ? j : h - 1 - j) * w / h; rect(x, y + j, e, 1, j > h / 5 && j < h * 4 / 5 ? K : Y); } break;
        case 4: rect(x, y, w, h, W); rect(x, y + h / 2 - h / 8, w, h / 4, R); rect(x + w / 2 - h / 8, y, h / 4, h, R); break;   // England
        case 5: rect(x, y, w, h, L); for (int i = 0; i < w; i++) { int yy = i * h / w; rect(x + i, y + yy - 1, 1, 2, W); rect(x + i, y + h - 1 - yy - 1, 1, 2, W); } break;   // Scotland
        case 6: nordic(x, y, w, h, R, W, B); break;                                                             // Norway
        case 7: rect(x, y, w, h, W); disc(x + w / 2, y + h / 2, h * 3 / 10, R); break;                          // Japan
        case 8: { int c[3] = { K, R, Y }; hstripes(x, y, w, h, c, 3); } break;                                  // Germany
        case 9: rect(x, y, w, h, G);                                                                            // Brazil
                for (int j = 0; j < h; j++) { int e = (j < h / 2 ? j : h - 1 - j) * w / h; rect(x + w / 2 - e, y + j, 2 * e, 1, Y); }
                disc(x + w / 2, y + h / 2, h / 4, B); break;
        case 10: rect(x, y, w, h, B); rect(x + w / 4 - 1, y, 2, h / 2, R); rect(x, y + h / 4 - 1, w / 2, 2, R);   // Australia
                 disc(x + w / 4, y + h * 3 / 4, 1, W); pset(x + w * 3 / 4, y + h / 3, W); pset(x + w * 3 / 4, y + h * 3 / 4, W); break;
        case 11: { int c[4] = { R, W, W, R }; vstripes(x, y, w, h, c, 4); } disc(x + w / 2, y + h / 2, h / 4, R); break;   // Canada
        case 12: { int c[3] = { B, W, R }; vstripes(x, y, w, h, c, 3); } break;                                // France
        case 13: { int c[3] = { R, W, B }; hstripes(x, y, w, h, c, 3); } break;                                // Netherlands
        case 14: nordic(x, y, w, h, L, Y, Y); break;                                                            // Sweden
        case 15: { int c[3] = { W, L, R }; hstripes(x, y, w, h, c, 3); } break;                                // Russia
        case 16: { int c[3] = { G, W, R }; hstripes(x, y, w, h, c, 3); } disc(x + w / 2, y + h / 2, h / 8, R); break;   // Iran
        case 17: rect(x, y, w, h, R); disc(x + w / 5, y + h / 3, h / 7, Y); break;                             // China
    }
    // thin border so light flags don't vanish on dark screens
    rect(x, y, w, 1, GREY); rect(x, y + h - 1, w, 1, GREY); rect(x, y, 1, h, GREY); rect(x + w - 1, y, 1, h, GREY);
}

// ── toasts: "NEW HIGH SCORE!" ─────────────────────────────────────────────
static char toastA[28], toastB[28]; static int toastT;
void toast(const char *a, const char *b) { strncpy(toastA, a, 27); toastA[27] = 0; strncpy(toastB, b ? b : "", 27); toastB[27] = 0; toastT = 150; }
int toastOn(void) { return toastT > 0; }
void drawToast(void) {
    if (toastT <= 0) return;
    toastT--;
    clipAll();
    box(20, 112, 200, 42, C_TOAST, GOLD);
    textC(116, toastA, GOLD, 1);
    if (toastB[0]) textC(134, toastB, WHITE, 1);
}

// ── the frame's drawing list ─────────────────────────────────────────────
// Menus look the same frame after frame, and drawing them pixel by pixel took up to three
// frames' time. So while a screen draws, each drawing call is only written down (with the
// clipping it had), and the list's fingerprint compared with what each of the two pages
// already shows: the same as the page on screen -> nothing is drawn at all; the same as the
// hidden page -> just show it; otherwise the list is played into the hidden page. A screen
// that copies straight into the page (a game's playfield) plays what's listed so far at that
// moment and draws directly from then on (drawDirect), so the order never changes.
enum { D_PSET, D_RECT, D_FILL, D_SHAPE, D_TEXT, D_TEXTS, D_IMG, D_STIP, D_POWER };
typedef struct { u8 op, c, sq, n; s16 cx0, cx1, cy0, cy1; int a[6]; const void *p; } DCmd;
#define DL_MAX 1400
static DCmd dl[DL_MAX] EWRAM_BSS;
static char dpool[8192] EWRAM_BSS;
static int dN, dPool, listing, backPage;
static u32 pageSig[2];                       // what each page holds (0 = unknown); [backPage] is being drawn
static void playList(void) {
    int s0 = clipX0, s1 = clipX1, s2 = clipY0, s3 = clipY1, sq = squeeze;
    listing = 0;
    for (int i = 0; i < dN; i++) {
        const DCmd *d = &dl[i]; const int *a = d->a;
        clipX0 = d->cx0; clipX1 = d->cx1; clipY0 = d->cy0; clipY1 = d->cy1; squeeze = d->sq;
        switch (d->op) {
            case D_PSET: pset_(a[0], a[1], d->c); break;
            case D_RECT: rect_(a[0], a[1], a[2], a[3], d->c); break;
            case D_FILL: fillScreen_(d->c); break;
            case D_SHAPE: shape_(a[0], a[1], a[2], a[3], a[4], a[5], d->n, d->c); break;
            case D_TEXT: glyphs(a[0], a[1], d->p, d->c, d->n); break;
            case D_TEXTS: textS_(a[0], a[1], d->p, d->c); break;
            case D_IMG: drawImg_(d->p, a[0], a[1], a[2], a[3]); break;
            case D_STIP: stipple_(a[0], a[1], a[2], a[3], d->c); break;
            case D_POWER: { float f[4]; memcpy(f, &a[2], 16); powerMarker_(a[0], a[1], f[0], f[1], f[2], f[3]); } break;
        }
    }
    clipX0 = s0; clipX1 = s1; clipY0 = s2; clipY1 = s3; squeeze = sq; dN = 0;
}
// write one call down (0: not listing, so draw it now)
static DCmd *note(int op, int c) {
    if (!listing) return 0;
    if (dN == DL_MAX) { drawDirect(); return 0; }              // (too much to list: draw the rest directly)
    DCmd *d = &dl[dN++]; d->op = (u8)op; d->c = (u8)c; d->sq = (u8)squeeze; d->n = 0; d->p = 0;
    d->cx0 = (s16)clipX0; d->cx1 = (s16)clipX1; d->cy0 = (s16)clipY0; d->cy1 = (s16)clipY1;
    return d;
}
static const char *keepStr(const char *t) {
    int n = strlen(t) + 1;
    if (dPool + n > (int)sizeof dpool) { drawDirect(); return 0; }
    char *k = &dpool[dPool]; memcpy(k, t, n); dPool += n; return k;
}
void drawBegin(void) { dN = 0; dPool = 0; listing = 1; }
// A game's side panel changes a few times a second, not every frame: it's redrawn only when its
// fingerprint (sig) differs from what this page's panel last showed. A frame that didn't draw a
// panel (a menu, a toast's end) forgets this page's.
static u32 panelSig[2], lineSig[2][12]; static int panelClaimed;
u32 sigMix(u32 h, int v) { return (h ^ (u32)v) * 16777619u + 0x9E37u; }
int panelNeeds(u32 sig) {
    panelClaimed = 1;
    sig = (sigMix(sigMix(sig, screen), toastOn()) | 1);
    if (panelSig[backPage] == sig) return 0;
    panelSig[backPage] = sig; memset(lineSig[backPage], 0, sizeof lineSig[0]); return 1;
}
// one line of the panel: 1 = it changed on this page, so clear its box (x, y, w, h) to colour bg
// and draw it. Call panelNeeds first (with what never changes mid-game), each frame.
int panelLine(int id, u32 sig, int x, int y, int w, int h, int bg) {
    sig |= 1;
    if (lineSig[backPage][id] == sig) return 0;
    lineSig[backPage][id] = sig; rect(x, y, w, h, bg); return 1;
}
void drawDirect(void) { if (listing) playList(); pageSig[backPage] = 0; }
void drawEnd(void) {
    if (!panelClaimed) panelSig[backPage] = 0;
    panelClaimed = 0;
    if (!listing) { pageSig[backPage] = 0; backPage ^= 1; platShow(1); return; }
    u32 h = 2166136261u;                                       // fingerprint: the list, then the words
    const u8 *b = (const u8 *)dl; int nb = dN * (int)sizeof(DCmd);
    for (int i = 0; i < nb; i += 4) h = (h ^ *(const u32 *)(b + i)) * 16777619u;
    for (int i = 0; i < dPool; i++) h = (h ^ (u8)dpool[i]) * 16777619u;
    h |= 1;
    if (h == pageSig[backPage ^ 1]) { listing = 0; dN = 0; platShow(0); return; }   // already on screen
    if (h != pageSig[backPage]) { playList(); pageSig[backPage] = h; }
    listing = 0; backPage ^= 1; platShow(1);
}
void pset(int x, int y, int c) { DCmd *d = note(D_PSET, c); if (!d) { pset_(x, y, c); return; } d->a[0] = x; d->a[1] = y; memset(&d->a[2], 0, 16); }
void rect(int x, int y, int w, int h, int c) { DCmd *d = note(D_RECT, c); if (!d) { rect_(x, y, w, h, c); return; } d->a[0] = x; d->a[1] = y; d->a[2] = w; d->a[3] = h; d->a[4] = d->a[5] = 0; }
void fillScreen(int c) { DCmd *d = note(D_FILL, c); if (!d) { fillScreen_(c); return; } dN = 0; dPool = 0; d = note(D_FILL, c); memset(d->a, 0, 24); }   // (covers all before it)
static void shape(int x, int y, int w, int h, int rl, int rr, int grad, int c) {
    DCmd *d = note(D_SHAPE, c); if (!d) { shape_(x, y, w, h, rl, rr, grad, c); return; }
    d->a[0] = x; d->a[1] = y; d->a[2] = w; d->a[3] = h; d->a[4] = rl; d->a[5] = rr; d->n = (u8)grad;
}
void text(int x, int y, const char *t, int col, int sc) {
    const char *k = listing ? keepStr(t) : 0;
    DCmd *d = note(D_TEXT, col); if (!d) { glyphs(x, y, t, col, sc); return; }
    d->a[0] = x; d->a[1] = y; memset(&d->a[2], 0, 16); d->n = (u8)sc; d->p = k;
}
void textS(int x, int y, const char *t, int col) {
    const char *k = listing ? keepStr(t) : 0;
    DCmd *d = note(D_TEXTS, col); if (!d) { textS_(x, y, t, col); return; }
    d->a[0] = x; d->a[1] = y; memset(&d->a[2], 0, 16); d->p = k;
}
void drawImg(const u8 *img, int w, int h, int x, int y) { DCmd *d = note(D_IMG, 0); if (!d) { drawImg_(img, w, h, x, y); return; } d->a[0] = w; d->a[1] = h; d->a[2] = x; d->a[3] = y; d->a[4] = d->a[5] = 0; d->p = img; }
void stipple(int x, int y, int w, int h, int c) { DCmd *d = note(D_STIP, c); if (!d) { stipple_(x, y, w, h, c); return; } d->a[0] = x; d->a[1] = y; d->a[2] = w; d->a[3] = h; d->a[4] = d->a[5] = 0; }
void powerMarker(int x0, int y0, float ux, float uy, float len, float power) {
    DCmd *d = note(D_POWER, 0); if (!d) { powerMarker_(x0, y0, ux, uy, len, power); return; }
    float f[4] = { ux, uy, len, power }; d->a[0] = x0; d->a[1] = y0; memcpy(&d->a[2], f, 16);
}
