// dozer.c — QU33PH DOZER (the website's dozer.html): a coin pusher.
//
// The DS version's cabinet (its slot, the pusher bed and the ledge; the marquee is cropped) at
// 4/7 of the DS's size in a column, with the time, your haul and your markers in the panel
// beside it. The website's push, shelf, pile, chair and refill rules and the DS's whole-number
// (fixed-point) pile physics are all here. The coins (there can be over a hundred) are small
// pictures drawn straight into the screen; the markers, the chair and the falling pieces are
// hardware sprites.
//
// CONTROLS: D-pad left/right aims along the slot, A drops. L/R turns the marker (along the
// push / across). Out of markers: B buys 3 for 10 coins. START pauses (SELECT quits).
#include "qu.h"
#ifdef PROF
extern volatile u32 profT[8];
static inline u32 tnw(void) { return *(volatile u16 *)0x04000108 | (*(volatile u16 *)0x0400010C << 16); }
#endif
typedef int64_t s64; typedef uint64_t u64;
#define SLOT_X0_ 0

#define FX 65536                                     // 1.0 in fixed point (16.16)
#define F(x) ((int)((x) * FX))
static int fmul(int a, int b) { return (int)(((s64)a * b) >> 16); }
static inline int fm(int a, int b) { return ((a >> 4) * (b >> 4)) >> 8; }   // fmul for numbers under 8.0, in 32 bits (no 64-bit maths)
static int fdiv(int a, int b) { return (int)(((s64)a << 16) / b); }
static int isqrt64(s64 v) {                          // integer square root (for 32.32 -> 16.16)
    if (v <= 0) return 0;
    u64 x = (u64)v, r = 0, bit = (u64)1 << 62;
    while (bit > x) bit >>= 2;
    while (bit) { if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return (int)r;
}
// the website's bed, slot and sizes (fractions of the cabinet art and of the bed)
#define BED_BACKY 0.455f
#define BED_FRONTY 0.862f
#define BED_BACKL 0.350f
#define BED_BACKR 0.650f
#define BED_FRONTL 0.205f
#define BED_FRONTR 0.775f
#define SLOT_Y0 0.176f
#define SLOT_Y1 0.211f
#define SLOT_X0 0.140f
#define SLOT_X1 0.860f
#define CHAIR_AT 30
#define PZ_MIN F(0.02)
#define PZ_MAX F(0.43)
#define R_COIN F(0.050)
#define MK_RAD F(0.048)
#define MK_H F(0.062)
#define R_CHAIR F(0.115)
#define MAXI 120
typedef struct { int u, v, du, dv, rad, h, drop, rot, spin; s16 ex, ey; u8 shelf, coin, col, awake; } Item;   // awake: frames left to test its contacts (a still pile sleeps)   // ex,ey: half-length along rot
static Item it[MAXI]; static int nI;
// the pieces in order of their left edge across the bed, kept up to date as pieces come and go
// (re-sorting from scratch each time the count changed cost more than all the contacts)
static int sweepOrd[MAXI], sweepN;                    // (5 KB, in the fast RAM: the contact loops read it all the time)
static int queueC[64], nQ, cycleN, coinsWon, state, pz, pzPrev, chairOn, refillFr, dropping, paidOut, upright = 1, menuSel, overSel;
static float tLeft, tTotal = 60, pzPhase, chairT, dragU = 0.5f, dropT; static int dropCol; static float dropRot;
typedef struct { float x, y, vy, vx, rot, spin, sz; int coin, col; } Fall;
static Fall fall[24] EWRAM_BSS; static int nFall;
typedef struct { char txt[16]; int life; int col; float u; int big; } Pop; static Pop pops[6];
enum { ST_PLAY, ST_OVER };

static inline int damp9(int x) { return x >= 0 ? (x * 58982) >> 16 : -((-x * 58982) >> 16); }   // x * 0.9, no divide
static int rndi(int a, int b) { return a + (int)(((s64)(rand() & 0xFFFF) * (b - a)) >> 16); }

// Turning is kept in whole numbers too: rot in 65536ths of a turn, sines from a table.
static s16 sinT[256] EWRAM_BSS; static u32 recipT[600] EWRAM_BSS; static int tablesOk, segK;
#define ROT2RAD (6.2831853f / 65536)
static void tables(void) {
    if (tablesOk) return; tablesOk = 1;
    for (int i = 0; i < 256; i++) sinT[i] = (s16)(fsin(i * 6.2831853f / 256) * 16384);
    for (int i = 1; i < 600; i++) recipT[i] = (1u << 24) / i;
    int h4 = MK_H >> 4; segK = (1 << 28) / (4 * h4 * h4);
}
static inline __attribute__((always_inline)) void setEnds(Item *a) {
    if (!a->h) { a->ex = a->ey = 0; return; }
    int k = (a->rot >> 8) & 255;
    a->ex = (sinT[(k + 64) & 255] * a->h) >> 14; a->ey = (sinT[k] * a->h) >> 14;
}
static void addCoin(int u, int v) { if (nI >= MAXI) return; sweepOrd[nI] = nI; sweepN = nI + 1; Item *a = &it[nI++]; memset(a, 0, sizeof *a); a->u = u; a->v = v; a->rad = R_COIN; a->coin = 1; a->awake = 6; }
static Item *addMarker(int u, int v, int col, float rotF) {
    int rot = (int)(rotF * (65536 / 6.2831853f));
    if (nI >= MAXI) return 0; sweepOrd[nI] = nI; sweepN = nI + 1; Item *a = &it[nI++]; memset(a, 0, sizeof *a);
    a->u = u; a->v = v; a->rad = MK_RAD; a->h = MK_H; a->col = col; a->rot = rot; a->awake = 6; setEnds(a); return a;
}
static void removeItem(int i) {                    // (the last piece moves into i's place)
    int last = --nI, w = 0; it[i] = it[last];
    for (int k = 0; k <= last; k++) { int o = sweepOrd[k]; if (o == i) continue; sweepOrd[w++] = o == last ? i : o; }
    sweepN = nI;
}
static void grant(int n) { for (int i = 0; i < n && nQ < 64; i++) queueC[nQ++] = cycleN++ % 3; }
static void popAdd(const char *t, int c, float u, int big) {
    for (int i = 0; i < 6; i++) if (pops[i].life <= 0) { strncpy(pops[i].txt, t, 15); pops[i].txt[15] = 0; pops[i].col = c; pops[i].u = u; pops[i].big = big; pops[i].life = big ? 80 : 50; return; }
}
static void sfx(int i) {
    if (!sv.sfxOn) return;
    const s8 *D[8] = { dz_s0, dz_s1, dz_s2, dz_s3, dz_s4, dz_s5, dz_s6, dz_s7 };
    const int L[8] = { DZ_S0_LEN, DZ_S1_LEN, DZ_S2_LEN, DZ_S3_LEN, DZ_S4_LEN, DZ_S5_LEN, DZ_S6_LEN, DZ_S7_LEN };
    sndFx(D[i], L[i]);
}

// ── the website's capsule contacts, in fixed point ───────────────────────
// Contacts are worked in "small units" (1/4096 of the bed, a 16th of the 16.16 numbers) so
// every product fits 32 bits: no 64-bit maths, no divides, no square roots of big numbers.
// A marker's centre line always has the same length, so its 1/length^2 is a constant (segK).
static inline __attribute__((always_inline)) void seg4(int px, int py, int x1, int y1, int dx, int dy, int *ox, int *oy) {
    int num = (px - x1) * dx + (py - y1) * dy;
    if (num <= 0) { *ox = x1; *oy = y1; return; }
    int t = (num * segK) >> 16; if (t > 4096) t = 4096;               // (0..1 as 0..4096)
    *ox = x1 + ((dx * t) >> 12); *oy = y1 + ((dy * t) >> 12);
}
static inline __attribute__((always_inline)) void closest4(const Item *a, const Item *b, int *ax, int *ay, int *bx, int *by) {
    int au = a->u >> 4, av = a->v >> 4, bu = b->u >> 4, bv = b->v >> 4;
    if (!a->h && !b->h) { *ax = au; *ay = av; *bx = bu; *by = bv; return; }
    int aex = a->ex >> 4, aey = a->ey >> 4, bex = b->ex >> 4, bey = b->ey >> 4;
    if (!b->h) { seg4(bu, bv, au - aex, av - aey, 2 * aex, 2 * aey, ax, ay); *bx = bu; *by = bv; return; }
    if (!a->h) { seg4(au, av, bu - bex, bv - bey, 2 * bex, 2 * bey, bx, by); *ax = au; *ay = av; return; }
    int pax = au, pay = av, pbx = bu, pby = bv;
    for (int i = 0; i < 2; i++) {
        seg4(pbx, pby, au - aex, av - aey, 2 * aex, 2 * aey, &pax, &pay);
        seg4(pax, pay, bu - bex, bv - bey, 2 * bex, 2 * bey, &pbx, &pby);
    }
    *ax = pax; *ay = pay; *bx = pbx; *by = pby;
}
static inline __attribute__((always_inline)) int isqrt32(u32 v) {
    u32 r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) { if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return (int)r;
}
IWRAM_LOCAL static void contact(Item *a, Item *b) {   // (fast RAM, called only from fast RAM)
    int ax, ay, bx, by; closest4(a, b, &ax, &ay, &bx, &by);
    int dx = bx - ax, dy = by - ay, mn = (a->rad + b->rad) >> 4;
    int d2 = dx * dx + dy * dy, ms = mn - 25;                            // (25: 0.006 of the bed)
    if (d2 >= mn * mn || d2 == 0 || d2 > ms * ms) return;                  // apart, or settled: no jitter
    int d = isqrt32(d2); if (d < 1) d = 1;
    int nx = (dx * (int)recipT[d]) >> 10, ny = (dy * (int)recipT[d]) >> 10;   // the normal, 1.0 = 16384
    int push = (mn - d) << 3;                                               // half the overlap, in 16.16
    int px = (nx * push) >> 14, py = (ny * push) >> 14;
    a->u -= px; a->v -= py; b->u += px; b->v += py; a->awake = b->awake = 6;
    int rel = ((b->du - a->du) * nx + (b->dv - a->dv) * ny) >> 14;
    if (rel < 0) { int imp = (-rel * 31457) >> 16, ix = (nx * imp) >> 14, iy = (ny * imp) >> 14; a->du -= ix; a->dv -= iy; b->du += ix; b->dv += iy; }
    if (a->h) a->spin += (((ax - (a->u >> 4)) * -ny - (ay - (a->v >> 4)) * -nx) * 57) >> 18;
    if (b->h) b->spin += (((bx - (b->u >> 4)) * ny - (by - (b->v >> 4)) * nx) * 57) >> 18;
}
static int uExt(Item *a) { return a->h ? (a->ex < 0 ? -a->ex : a->ex) + a->rad : a->rad; }
static inline __attribute__((always_inline)) int backV(Item *a) { int y1 = a->v - a->ey, y2 = a->v + a->ey; return (y1 < y2 ? y1 : y2) - a->rad; }
// (The GBA is much slower than the DS: instead of testing every pair of up to 150 pieces, the
//  pieces are kept sorted by their left edge across the bed and each is tested only against the
//  ones whose left edge is within its reach. Same contacts, same responses; in fast RAM.)
#ifdef PROF
volatile u32 pcIn, pcC, pcPanel;
#endif
static int ext_[MAXI];
// The first pass of a step finds the pairs close enough to touch (sorted sweep) and keeps them;
// the step's other passes only re-test those (pieces move a hair between passes).
#define MAXP 640
static u8 pairA[MAXP], pairB[MAXP]; static int nPair;
// two coins: settled (touching less than 0.006 of the bed) or apart? -- the usual case, tested
// right here without a call
static inline __attribute__((always_inline)) int coinsIdle(const Item *a, const Item *b) {
    if (!(a->coin & b->coin)) return 0;
    int dx = (b->u - a->u) >> 4, dy = (b->v - a->v) >> 4, ms = ((R_COIN * 2) >> 4) - 25;
    return dx * dx + dy * dy > ms * ms;
}
IWRAM_CODE static void pairsAgain(void) {
    for (int k = 0; k < nPair; k++) { Item *a = &it[pairA[k]], *b = &it[pairB[k]]; if (!coinsIdle(a, b)) contact(a, b); }
}
IWRAM_CODE static void pairs(void) {
    nPair = 0;
    for (int i = 0; i < nI; i++) { Item *a = &it[i]; ext_[i] = a->rad + (a->ex < 0 ? -a->ex : a->ex) + (a->ey < 0 ? -a->ey : a->ey); }
    if (sweepN != nI) { for (int i = 0; i < nI; i++) sweepOrd[i] = i; sweepN = nI; }
    for (int i = 1; i < nI; i++) {                                   // insertion sort: nearly sorted already
        int k = sweepOrd[i], key = it[k].u - ext_[k], j = i - 1;
        while (j >= 0 && it[sweepOrd[j]].u - ext_[sweepOrd[j]] > key) { sweepOrd[j + 1] = sweepOrd[j]; j--; }
        sweepOrd[j + 1] = k;
    }
    for (int oi = 0; oi < nI; oi++) {
        int i = sweepOrd[oi]; Item *a = &it[i]; int right = a->u + ext_[i];
        for (int oj = oi + 1; oj < nI; oj++) {
            int j = sweepOrd[oj]; Item *b = &it[j];
            if (b->u - ext_[j] > right) break;                         // everything further right is out of reach
#ifdef PROF
            pcIn++;
#endif
            if (a->shelf != b->shelf || !(a->awake | b->awake)) continue;   // (two sleeping pieces can't have moved into each other)
            int du = b->u - a->u, dv = b->v - a->v, reach = ext_[i] + ext_[j];
            if (du > reach || du < -reach || dv > reach || dv < -reach) continue;
#ifdef PROF
            pcC++;
#endif
            {                                                           // centres further apart than both reaches: not a pair
                int x = du >> 4, y = dv >> 4, m = ((a->rad + a->h + b->rad + b->h) >> 4) + 41;   // (+0.01 of the bed)
                if (x * x + y * y >= m * m) continue;
            }
            if (nPair < MAXP) { pairA[nPair] = (u8)i; pairB[nPair] = (u8)j; nPair++; }
            if (!coinsIdle(a, b)) contact(a, b);
        }
    }
}
static void chairPush(Item *a) {
    Item c = { 0 }; c.u = F(0.5); c.v = F(0.5);
    int ax, ay, bx, by; closest4(a, &c, &ax, &ay, &bx, &by);
    int dx = ax - (F(0.5) >> 4), dy = ay - (F(0.5) >> 4), mn = (a->rad + R_CHAIR) >> 4;   // (small units)
    int d2 = dx * dx + dy * dy;
    if (d2 >= mn * mn || d2 == 0) return;
    int d = isqrt32(d2); if (d < 1) d = 1;
    int nx = (dx * (int)recipT[d]) >> 10, ny = (dy * (int)recipT[d]) >> 10;   // (1.0 = 16384)
    int push = (mn - d) << 4;
    a->u += (nx * push) >> 14; a->v += (ny * push) >> 14; a->awake = 6;
    float kick = a->h ? 0.020f : 0.006f, jit = a->h ? (frand() - 0.5f) * 1.8f : (frand() - 0.5f) * 0.5f, c2 = fcos(jit), s2 = fsin(jit);
    float fnx = nx / 16384.0f, fny = ny / 16384.0f;
    a->du += F((fnx * c2 - fny * s2) * kick); a->dv += F((fnx * s2 + fny * c2) * kick);
    if (a->h) a->spin += rand() % 7300 - 3650;
}
// the bed's sides and back (fast RAM)
IWRAM_CODE static void walls(void) {
    for (int i = 0; i < nI; i++) {
        Item *a = &it[i];
        int ex = a->h ? (a->ex < 0 ? -a->ex : a->ex) + a->rad : a->rad;
        if (a->u < ex) { a->u = ex; int d = a->du < 0 ? -a->du : a->du; a->du = (d * 26214) >> 16; }
        if (a->u > FX - ex) { a->u = FX - ex; int d = a->du < 0 ? -a->du : a->du; a->du = -((d * 26214) >> 16); }
        int vmin = (a->rad * 13107) >> 16; if (a->v < vmin) a->v = vmin;
    }
}
static void separate(int first) {
#ifdef PROF
    u32 q0 = tnw();
#endif
    if (first) pairs(); else pairsAgain();
#ifdef PROF
    profT[4] = tnw() - q0;
#endif
    if (chairOn) for (int i = 0; i < nI; i++) {                      // the chair: only pieces near it
        Item *a = &it[i]; int r = a->rad + R_CHAIR + MK_H;
        if (!a->shelf && a->u > F(0.5) - r && a->u < F(0.5) + r && a->v > F(0.5) - r && a->v < F(0.5) + r) chairPush(a);
    }
    walls();
}
static void reset(void) {
    tables();
    nI = 0; coinsWon = 0; nQ = 0; cycleN = 0; grant(3);
    tLeft = tTotal; pz = pzPrev = PZ_MIN; pzPhase = 0; dragU = 0.5f; dropping = 0; chairOn = 0; chairT = 0;
    memset(pops, 0, sizeof pops); paidOut = 0; refillFr = 132; nFall = 0;
    int r = 0;
    for (float v = 0.525f; v <= 0.80f; v += 0.108f, r++) {           // rows of coins, as the website lays them
        int n = (r % 2 == 0) ? 9 : 8; float off = (r % 2 == 0) ? 0 : 0.84f / 16;
        for (int i = 0; i < n; i++) { float u = 0.08f + off + i * (0.84f / (n - 1)) + (frand() - 0.5f) * 0.02f;
            u = u < 0.05f ? 0.05f : u > 0.95f ? 0.95f : u; addCoin(F(u), F(v + (frand() - 0.5f) * 0.016f)); }
    }
    for (int i = 0; i < 9; i++) addMarker(F(0.15f + frand() * 0.7f), F(0.56f + frand() * 0.2f), rand() % 3, frand() * 3.14159f);
    const int minBack = PZ_MAX + F(0.025), maxFront = F(0.82);
    for (int pass = 0; pass < 4; pass++) {
        for (int k = 0; k < 6; k++) separate(1);
        for (int i = 0; i < nI; i++) { Item *a = &it[i]; int bv = backV(a); if (bv < minBack) a->v += minBack - bv; if (a->v > maxFront) a->v = maxFront - rndi(0, F(0.02)); }
    }
    int k = 0; for (int i = 0; i < nI; i++) if (it[i].v <= maxFront + F(0.005)) it[k++] = it[i]; nI = k; sweepN = -1;
    state = ST_PLAY;
}
static void doRefill(void) {
    int n = 30 + rand() % 8;        // the website tips in 50-60; the DS's smaller bed holds about 30-37 well
    for (int i = 0; i < n && nI < MAXI; i++) {
        int u = F(0.5) + rndi(-F(0.24), F(0.24)); if (u < R_COIN) u = R_COIN; if (u > FX - R_COIN) u = FX - R_COIN;
        addCoin(u, F(0.54) + rndi(-F(0.12), F(0.14))); Item *c = &it[nI - 1];
        c->du = rndi(-F(0.014), F(0.014)); c->dv = rndi(-F(0.012), F(0.016)); c->drop = FX;
    }
    refillFr = 132; popAdd("COIN DROP!", YELLOW, 0.5f, 1);
    for (int i = 0; i < 3; i++) sfx(rand() % 5);
}
static void endGame(void) {
    state = ST_OVER;
    if (!paidOut && coinsWon > 0) { addCoins(coinsWon); paidOut = 1; }
    if (coinsWon > sv.arcadeBest[ARC_DOZER]) sv.arcadeBest[ARC_DOZER] = coinsWon;
    sv.arcadePlays[ARC_DOZER]++; saveWrite();
    screen = S_DOZER_OVER;
}
// the bed's perspective, in whole numbers: u,v (16.16) -> screen x,y and the size factor (16.16)
#define BXL ((int)(256 * BED_BACKL * FX))
#define BXR ((int)(256 * BED_BACKR * FX))
#define FXL ((int)(256 * BED_FRONTL * FX))
#define FXR ((int)(256 * BED_FRONTR * FX))
static inline __attribute__((always_inline)) void bedXYi(int u, int v, int *x, int *y, int *s) {
    // (pixels in 8.8 so every product fits 32 bits)
    const int bl = (int)(256 * BED_BACKL * 256), br = (int)(256 * BED_BACKR * 256), fl = (int)(256 * BED_FRONTL * 256), fr = (int)(256 * BED_FRONTR * 256);
    int v4 = v >> 4, u4 = u >> 4;
    int l = bl + (((fl - bl) * v4) >> 12), r = br + (((fr - br) * v4) >> 12);
    *x = (l + (((r - l) * u4) >> 12)) >> 8;
    *y = (int)(384 * BED_BACKY) + ((((int)(384 * (BED_FRONTY - BED_BACKY) * 256)) * v4) >> 20);
    *s = FX + ((((int)(((BED_FRONTR - BED_FRONTL) / (BED_BACKR - BED_BACKL) - 1) * FX)) * v4) >> 12);
}
static void bedXY(int u, int v, float *x, float *y, float *s) { int ix, iy, is; bedXYi(u, v, &ix, &iy, &is); *x = ix; *y = iy; *s = is / (float)FX; }
IWRAM_CODE static void moveItems(int dpz, int pSpeed) {      // each piece's slide, turn and the pusher's shove (fast RAM)
    for (int i = 0; i < nI; i++) {
        Item *a = &it[i];
        a->u += a->du; a->v += a->dv; a->du = damp9(a->du); a->dv = damp9(a->dv);
        if (a->h && a->spin) { a->rot += a->spin; a->spin = a->spin * 225 / 256; setEnds(a); }
        if (a->shelf) {
            if (dpz > 0) a->v += dpz;
            if (a->v > pz) { a->shelf = 0; a->drop = FX; if (a->dv < F(0.003)) a->dv = F(0.003); }
        } else { int bv = backV(a); if (bv < pz) { a->v += pz - bv; a->awake = 6; int k = fm(pSpeed, F(1.7)); if (a->dv < k) a->dv = k; } }   // the face drives it
        if (a->drop > 0) { a->drop -= F(3.2f / 60); if (a->drop < 0) a->drop = 0; }
        if (a->du > 24 || a->du < -24 || a->dv > 24 || a->dv < -24 || a->spin || a->drop || a->shelf) a->awake = 6;   // moving: keep testing it
        else if (a->awake) a->awake--;
    }
}
void updateDozer(void) {
    const float dt = 1.0f / 60;
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) pops[i].life--;
    for (int i = nFall - 1; i >= 0; i--) {                          // tumbling over the front, then gone
        fall[i].vy += 384 * 1.2f * dt; fall[i].y += fall[i].vy * dt; fall[i].x += fall[i].vx * dt; fall[i].rot += fall[i].spin * dt; fall[i].sz *= 1 - dt * 0.16f;
        if (fall[i].y > 384 * 1.14f) fall[i] = fall[--nFall];
    }
    if (state != ST_PLAY) return;
    tLeft -= dt;
    if (!chairOn && tTotal - tLeft >= CHAIR_AT) { chairOn = 1; chairT = 0; popAdd("CHAIR DROPPED!", RED, 0.5f, 1); }
    if (chairOn && chairT < 1) { chairT += dt * 2.2f; if (chairT > 1) chairT = 1; }
    if (tLeft <= 0) { tLeft = 0; endGame(); return; }
    if (refillFr <= 0) { int nc = 0; for (int i = 0; i < nI; i++) nc += it[i].coin; if (nc < 5) doRefill(); }
    pzPhase += dt * 2.7f; pzPrev = pz;                               // (the website's 1.55, quickened for the DS)
    pz = PZ_MIN + (int)((PZ_MAX - PZ_MIN) * (0.5f - 0.5f * fcos(pzPhase)));
    int pSpeed = pz - pzPrev > 0 ? pz - pzPrev : 0, dpz = pz - pzPrev;
    if (dropping) {
        dropT += dt * 4.2f;                                   // markers drop through quicker
        if (dropT >= 1) {
            int landV = fmul(MK_RAD, F(1.2)) + fmul(MK_H, F(0.2));
            Item *m = addMarker(F(dragU), landV, dropCol, dropRot);
            if (m) m->shelf = landV < pz;                          // caught on the pusher's shelf
            dropping = 0;
        }
    }
    moveItems(dpz, pSpeed);
#ifdef PROF
    u32 q0 = tnw();
#endif
    for (int k = 0; k < 4; k++) separate(k == 0);                  // (the DS's 4 passes; only the first looks for new pairs)
#ifdef PROF
    profT[5] = tnw() - q0;
#endif
    if (refillFr > 0) { refillFr--; for (int i = 0; i < nI; i++) if (it[i].v > F(0.97)) it[i].v = F(0.97); }
    for (int i = nI - 1; i >= 0; i--) {
        Item *a = &it[i];
        if (a->shelf || refillFr > 0) continue;
        if (a->v > FX - fm(a->rad, F(0.30))) {                   // over the ledge
            float x, y, s; bedXY(a->u, FX, &x, &y, &s);
            if (nFall < 24) fall[nFall++] = (Fall){ x, y, 384 * 0.10f, (a->u / (float)FX - 0.5f) * 256 * 0.04f, a->rot * ROT2RAD, (frand() - 0.5f) * 3, s, a->coin, a->col };
            if (a->coin) { coinsWon++; popAdd("+1", YELLOW, a->u / (float)FX, 0); sfx(rand() % 5); }
            else { if (nQ < 64) queueC[nQ++] = a->col; popAdd("MARKER BACK", C_LBLUE, a->u / (float)FX, 0); sfx(7); }
            removeItem(i);
        }
    }
}

// ── input ─────────────────────────────────────────────────────────────────
static void doDrop(void) {
    if (state != ST_PLAY || nQ <= 0 || dropping) return;
    dropCol = queueC[0]; memmove(queueC, queueC + 1, sizeof(int) * (--nQ));
    dropping = 1; dropT = 0; dropRot = upright ? 1.5707963f : 0;      // vertical = along the push
    sfx(5 + rand() % 2);
}
void inputDozer(void) {
    if (screen == S_DOZER_PAUSE) {
        if (kDown & KEY_START) screen = S_DOZER;
        if (kDown & KEY_SELECT) { endGame(); goScreen(S_ARCADE); }
        return;
    }
    if (kDown & KEY_START) { screen = S_DOZER_PAUSE; return; }
    if (kDown & (KEY_L | KEY_R)) upright = !upright;
    if (kDown & KEY_B && nQ <= 0 && !dropping && sv.coins >= 10) { spendCoins(10); grant(3); popAdd("+3 MARKERS", LIME, 0.5f, 1); }
    if (kHeld & KEY_LEFT) { dragU -= 0.02f; if (dragU < 0) dragU = 0; }
    if (kHeld & KEY_RIGHT) { dragU += 0.02f; if (dragU > 1) dragU = 1; }
    if (kDown & KEY_A) doDrop();
}

// ── drawing: the DS's picture (256 x 384), from its row 56 down, at 4/7 ───
#define ZS (4.0f / 7.0f)
#define PX0 (DZ_W + 1)
#define PW (SW - PX0)
static int zx(float x) { return (int)(x * ZS); }
static int zy(float y) { return (int)((y - DZ_Y0) * ZS); }
// tiles: the three markers (16x16), the main game's chair (64x64); palettes 0-2, 3
enum { T_DM = 512, T_DCH = 528 };
void dozerObjLoad(void) {
    const u8 *M[3] = { dz_m0, dz_m1, dz_m2 }; const u16 *MP[3] = { dz_m0_pal, dz_m1_pal, dz_m2_pal };
    for (int i = 0; i < 3; i++) { platObjTiles(T_DM + i * 4, M[i], 128); memcpy(&objPal[i * 16], MP[i], 32); }
    platObjTiles(T_DCH, obj_chair, 2048); memcpy(&objPal[48], obj_chair_pal, 32);
}
static int nObj;
// ang in 65536ths of a turn; the sines come from the table
static void aff(int tile, int pal, int size, int w, int cx, int cy, float k, int ang, int dbl) {
    if (nObj >= 32 || k < 0.02f || cx < -w * 2 || cx > SW + w * 2 || cy < -w * 2 || cy > SH + w * 2) return;
    int i = nObj++, ik = (int)(256.0f / k), q = (ang >> 8) & 255;
    int c = (sinT[(q + 64) & 255] * ik) >> 14, s = (sinT[q] * ik) >> 14;
    oam[i * 4].a3 = (u16)(s16)c; oam[i * 4 + 1].a3 = (u16)(s16)s;
    oam[i * 4 + 2].a3 = (u16)(s16)(-s); oam[i * 4 + 3].a3 = (u16)(s16)c;
    int bw = dbl ? w : w / 2;
    oam[i].a0 = (u16)(((cy - bw) & 0xFF) | (dbl ? 0x0300 : 0x0100));
    oam[i].a1 = (u16)(((cx - bw) & 0x1FF) | (i << 9) | (size << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
// a marker 'len' DS pixels long (the website lays the square pictures across len x len, the
// blue one across len), turned; the 16x16 pictures are 16 across
static void drawMkA(int gx, int gy, float len, int rot, int col) {    // (screen position, turn in 65536ths)
    aff(T_DM + col * 4, col, 1, 16, gx, gy, len * ZS / 16, rot - 8192 * (col != 2), 1);
}
static void drawMk(float x, float y, float len, float rot, int col) { drawMkA(zx(x), zy(y), len, (int)(rot * (65536 / 6.2831853f)), col); }
static int zxi(int x) { return (x * 37449) >> 16; }                  // (x 4/7, in whole numbers)
static int zyi(int y) { return ((y - DZ_Y0) * 37449) >> 16; }
static void coinAt(int gx, int gy, float radDs) {                  // a coin, centred, from the pre-shrunk sizes
    int d = (int)(radDs * 2 * ZS + 0.5f); if (d < DZC_MIN) d = DZC_MIN; if (d > DZC_MAX) d = DZC_MAX;
    int dh = d * 82 / 100; if (dh < 2) dh = 2;
    drawImg(dzcoin[d - DZC_MIN], d, dh, gx - d / 2, gy - dh / 2);
}
IWRAM_CODE static void pileCoins(u8 *ord) {                    // the pile back to front, the coins drawn in (fast RAM)
    for (int i = 0; i < nI; i++) ord[i] = i;
    for (int i = 1; i < nI; i++) { u8 k = ord[i]; int j = i - 1; while (j >= 0 && it[ord[j]].v > it[k].v) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = k; }
    for (int n = 0; n < nI; n++) { Item *a = &it[ord[n]]; int x, y, sc; bedXYi(a->u, a->v, &x, &y, &sc);
        y -= (fm(a->shelf ? FX : a->drop, sc) * 492) >> 22;
        if (a->coin) {                                                // (all whole numbers: there can be a hundred)
            int rq = fm(sc, F(0.050 * 76.8 * 0.95)), d = ((((rq >> 6) * 74899) >> 10) + 32768) >> 16;
            if (d < DZC_MIN) d = DZC_MIN; if (d > DZC_MAX) d = DZC_MAX;
            int dh = (d * 210) >> 8; if (dh < 2) dh = 2;
            drawImg_(dzcoin[d - DZC_MIN], d, dh, ((x * 37449) >> 16) - d / 2, (((y - DZ_Y0) * 37449) >> 16) - dh / 2);
        } }
}
static void drawPusher(void) {
    float yTop = 384 * 0.405f, s, xl, xr, yF;
    bedXY(0, pz, &xl, &yF, &s); bedXY(FX, pz, &xr, &yF, &s);
    float bw = (BED_BACKR - BED_BACKL) * 256, lT = 256 * BED_BACKL - bw * 0.16f, rT = 256 * BED_BACKR + bw * 0.16f;
    float lF = xl - bw * 0.14f * s, rF = xr + bw * 0.14f * s;
    int g0 = zy(yTop), g1 = zy(yF); if (g0 < 0) g0 = 0;
    if (g1 > g0) {                                                 // the pusher's top, dark, lighter towards you
        // (each edge is a straight line down the rows: worked once, then stepped in whole numbers)
        float ta = ((g0 / ZS + DZ_Y0) - yTop) / (yF - yTop), tb = ((g1 / ZS + DZ_Y0) - yTop) / (yF - yTop);
        int xa0 = (int)((lT + (lF - lT) * ta) * ZS * 65536), xb0 = (int)((rT + (rF - rT) * ta) * ZS * 65536);
        int xa1 = (int)((lT + (lF - lT) * tb) * ZS * 65536), xb1 = (int)((rT + (rF - rT) * tb) * ZS * 65536);
        int n = g1 - g0, da = (xa1 - xa0) / n, db = (xb1 - xb0) / n;
        int gA = g0 + (int)((0.33f - ta) / (tb - ta) * n), gB = g0 + (int)((0.66f - ta) / (tb - ta) * n);
        for (int g = g0; g < g1; g++, xa0 += da, xb0 += db)
            hspan(g, xa0 >> 16, xb0 >> 16, g < gA ? C_DARK : g < gB ? C_PANEL : C_PANELLINE);
    }
    int fh = (int)(384 * 0.022f * s * ZS); if (fh < 2) fh = 2;
    int y0 = zy(yF) - fh / 2;                                       // the yellow-and-black hazard face
    int xa = zx(lF), xb = zx(rF), w = fh > 2 ? fh : 3;
    for (int j = 0; j < fh; j++) hstripe(y0 + j, xa, xb, w, C_GOLD, C_BLACK);
}
void drawDozer(void) {
    char s[32];
    drawDirect();
    for (int y = 0; y < SH; y++) {
        copy32(&page[y * (SW / 2)], &dz8[y * DZ_W], DZ_W / 4);
    }
#ifdef PROF
    u32 q0 = tnw();
#endif
    clipX0 = 0; clipX1 = DZ_W; clipY0 = 0; clipY1 = SH;
    drawPusher();
#ifdef PROF
    profT[7] = tnw() - q0; q0 = tnw();
#endif
    hideSprites(); nObj = 0;
    // the slot: where you'll drop (on top of everything), and the marker on its way down
    float sx = 256 * (SLOT_X0 + (SLOT_X1 - SLOT_X0) * dragU), sy = 384 * (SLOT_Y0 + SLOT_Y1) * 0.5f, len = ((MK_H + MK_RAD) * 2 / (float)FX) * 76.8f;
    if (state == ST_PLAY && nQ > 0 && !dropping) { drawMk(sx, sy, len, upright ? 1.5707963f : 0, queueC[0]); rect(zx(sx) - 3, zy(sy) + 8, 7, 1, GOLD); }
    if (dropping) { float t = dropT, x, y, sc; bedXY(F(dragU), 0, &x, &y, &sc);
        drawMk(sx + (x - sx) * t, sy + (y - sy) * t, len * (1 - 0.12f * t), dropRot, dropCol); }
    for (int i = 0; i < nFall; i++) {                              // pieces tumbling over the front
        if (fall[i].coin) coinAt(zx(fall[i].x), zy(fall[i].y), (R_COIN / (float)FX) * 76.8f * fall[i].sz);
        else drawMk(fall[i].x, fall[i].y, ((MK_H + MK_RAD) * 2 / (float)FX) * 76.8f * fall[i].sz, fall[i].rot, fall[i].col);
    }
    // the pile, back to front: coins into the picture, markers as sprites (nearest on top)
    static u8 ord[MAXI]; pileCoins(ord);
    for (int n = nI - 1; n >= 0; n--) { Item *a = &it[ord[n]]; if (a->coin) continue; int x, y, sc; bedXYi(a->u, a->v, &x, &y, &sc);
        y -= (fm(a->shelf ? FX : a->drop, sc) * 492) >> 22;
        drawMkA(zxi(x), zyi(y), ((MK_H + MK_RAD) * 2 / (float)FX) * 76.8f * (sc / (float)FX), a->rot, a->col); }
    if (chairOn) { float x, y, sc; bedXY(F(0.5), F(0.5), &x, &y, &sc);       // (under the markers)
        float hh = (R_CHAIR / (float)FX) * 76.8f * sc * 2.3f * (0.35f + 0.65f * chairT);
        aff(T_DCH, 3, 3, 64, zx(x), zy(y - hh * 0.22f), hh * ZS / 40, 0, 0); }
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) {
        if (pops[i].big) textCW(0, DZ_W, 44, pops[i].txt, pops[i].col, 1);
        else { float x, y, sc; bedXY(F(pops[i].u), FX, &x, &y, &sc); textS(zx(x) - textSW(pops[i].txt) / 2, zy(y) - 18 - (50 - pops[i].life) / 3, pops[i].txt, pops[i].col); }
    }
#ifdef PROF
    profT[6] = tnw() - q0;
#endif
    clipAll();
    // the panel: the labels once, then each line only when it changes (most frames, nothing)
    if (panelNeeds(0)) {
        for (int y = 0; y < SH; y++) fill32(&page[y * (SW / 2) + DZ_W / 2], C_PANEL * 0x01010101u, (SW - DZ_W) / 4);
        rect(DZ_W, 0, 1, SH, C_PANELLINE);
        clipX0 = PX0;
        textS(PX0 + 4, 4, "TIME", GREY); textS(PX0 + 4, 34, "WON", GREY);
        textS(PX0 + 4, 126, "1 / 2", GREY); textS(PX0 + 4, 140, "A: drop", GREY);
    }
    clipX0 = PX0;
    int secs = (int)(tLeft + 0.99f);
    if (panelLine(0, sigMix(secs, tLeft < 10), PX0, 13, PW, 18, C_PANEL)) { sprintf(s, "%d", secs); text(PX0 + 4, 14, s, tLeft < 10 ? RED : WHITE, 1); }
    if (panelLine(1, sigMix(coinsWon, 7), PX0, 43, PW, 18, C_PANEL)) { sprintf(s, "+%d", coinsWon); text(PX0 + 4, 44, s, GOLD, 1); }
    u32 qs = sigMix(nQ, 3); for (int k = 0; k < nQ && k < 10; k++) qs = sigMix(qs, queueC[k]);
    if (panelLine(2, qs, PX0, 66, PW, 25, C_PANEL)) {
        sprintf(s, "MARKERS %d", nQ); textS(PX0 + 4, 66, s, WHITE);
        for (int k = 0; k < nQ && k < 10; k++) {                    // what's coming, in its colours
            static const int MC[3] = { C_MRED, C_MGREEN, C_MBLUE };
            rect(PX0 + 4 + k * 8, 78, 5, 12, MC[queueC[k]]);
        }
    }
    int out = state == ST_PLAY && nQ <= 0 && !dropping;
    if (panelLine(3, sigMix(out, sv.coins >= 10), PX0, 100, PW, 22, C_PANEL) && out) {
        textS(PX0 + 4, 100, sv.coins >= 10 ? "B: +3 markers" : "out of markers", YELLOW); if (sv.coins >= 10) textS(PX0 + 4, 111, "(10 coins)", GREY); }
    if (panelLine(4, sigMix(upright, 1), PX0 + 32, 126, PW - 32, 10, C_PANEL)) textS(PX0 + 32, 126, upright ? "ALONG" : "ACROSS", C_LBLUE);
    if (panelLine(5, sigMix(sv.coins, 2), PX0, 150, PW, 10, C_PANEL)) { sprintf(s, "coins %d", sv.coins); textS(PX0 + 4, 150, s, GOLD); }
    clipAll();
    if (screen == S_DOZER_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn DB[4];
static void dmLayout(void) {
    DB[0] = (Btn){ 136, 12, 100, 28, "60 SEC", 0, 0 }; DB[1] = (Btn){ 136, 46, 100, 28, "90 SEC", 0, 0 };
    DB[2] = (Btn){ 136, 80, 100, 28, "120 SEC", 0, 0 }; DB[3] = (Btn){ 136, 118, 100, 26, "BACK", 0, 0 };
}
void drawDozerMenu(void) {
    char s[32];
    fillScreen(DARK);
    text(6, 2, "QU33PH DOZER", GOLD, 1);
    static const char *L[6] = { "Left/right along the slot,", "A to drop a marker.", "Coin off the ledge: +1 coin", "a marker off it comes back",
        "a chair drops in at 30 s", "out of markers? B: +3 for 10" };
    static const int C[6] = { WHITE, WHITE, C_GOLD, WHITE, RED, GREY };
    for (int i = 0; i < 6; i++) textS(6, 22 + i * 12, L[i], C[i]);
    sprintf(s, "BEST HAUL %d", sv.arcadeBest[ARC_DOZER]); textS(6, 102, s, GREY);
    coinCount(6, 136, 0);
    dmLayout(); drawBtns(DB, 4, menuSel);
}
void inputDozerMenu(void) {
    dmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(DB, 4, &menuSel, 1);
    if (h >= 0 && h <= 2) { tTotal = 60 + h * 30; reset(); screen = S_DOZER; }
    if (h == 3) goScreen(S_ARCADE);
}
void drawDozerOver(void) {
    char s[32];
    drawDozer();
    hideSprites();
    box(20, 16, 200, 128, C_TOAST, GOLD);
    textC(22, "TIME UP", GOLD, 2);
    sprintf(s, "+%d COINS WON", coinsWon); textC(54, s, WHITE, 1);
    sprintf(s, "BEST HAUL %d", sv.arcadeBest[ARC_DOZER]); textC(70, s, LIME, 1);
    DB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; DB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    drawBtns(DB, 2, overSel);
}
void inputDozerOver(void) {
    DB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; DB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    int h = btnInput(DB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) { reset(); screen = S_DOZER; }
    if (h == 1) goScreen(S_ARCADE);
}
