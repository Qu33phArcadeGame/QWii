// flip.c — QU33PH FLIP (the website's flip.html): flip the marker from pad to pad.
//
// The DS version's world, zoom and physics, frame for frame (its course building, chairs that
// launch you several pads ahead, jagged red ground that tips you over, and 33 levels graded
// RED / GREEN / BLUE; ENDLESS; TIMED), seen at two-thirds of the DS's size on the GBA's wide
// screen: it shows half again as far ahead. The window backdrop scrolls slowly behind (the
// website's parallax). The pads, the chairs and your marker are hardware sprites.
//
// CONTROLS: D-pad left/right steers (how far), up/down sets the height, hold A for power and
// let go (B cancels). L/R: land it upright or flat. START pauses (SELECT quits).
#include "qu.h"

#define CW 256.0f
#define CH 384.0f
#define WW 256.0f
#define Z 0.80f                                    // (the website zooms 0.60 on phones, 1.0 on wide screens; 0.80 reads best on the DS)
#define AY (CH * 0.70f)
#define HOLD 0.16f
#define G (0.0018f * CH)
#define MH (0.085f * CH)
#define CHAIR_HH (CH * 0.105f)
#define MAXP 64

typedef struct { float x, y, w; u8 uneven, hazard, chair, chairHit, graded, color; } Pad;   // color 0 R, 1 G, 2 B
static Pad P[MAXP] EWRAM_BSS; static int nP, lastFlat, stairLeft; static float stairStep;
static struct { float x, y, vx, vy, rot, spin; int pIdx, skipTo, tipT; } mk;
enum { M_LEVELS, M_ENDLESS, M_TIMED };
enum { ST_READY, ST_FLYING, ST_BOOST, ST_TIP, ST_OVER };
static int flMode, levelIdx, state, score, combo, settle, gradePts, won, timerOn, upright = 1, coinsWon, overTier, overMax;
static float camX, timeLeft, aim = 0.45f, lift = 1, power; static int charging, ph;
static const char *overTitle; static int overCol;
typedef struct { float x, y; int life; char txt[16]; int col; } Pop;
static Pop pops[6];
static int fFrames;

// the website's 11 hand-made levels: per pad its gap and width (fractions of the width), the
// height change (fraction of the height), jagged ground, and a chair
typedef struct { float g, w, d; u8 u, c; } PadSpec;
typedef struct { const char *name; int n; PadSpec p[10]; } Level;
static const Level LEVELS[11] = {
{"FIRST FLIPS",5,{{0.13,0.36,0,0,0},{0.13,0.36,0,0,0},{0.14,0.34,0,0,0},{0.14,0.34,0,0,0},{0.15,0.32,0,0,0}}},
{"UP THE STAIRS",6,{{0.13,0.32,0,0,0},{0.13,0.3,-0.045,0,0},{0.13,0.3,-0.045,0,0},{0.13,0.3,-0.045,0,0},{0.13,0.3,-0.045,0,0},{0.15,0.34,0,0,0}}},
{"RED TAPE",7,{{0.13,0.32,0,0,0},{0.12,0.22,0,1,0},{0.13,0.32,0,0,0},{0.12,0.2,0,0,0},{0.13,0.32,0,0,0},{0.12,0.22,0,1,0},{0.14,0.34,0,0,0}}},
{"HIGH ROAD",8,{{0.13,0.3,0,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,-0.05,0,0},{0.14,0.3,0,0,0},{0.14,0.28,0.06,0,0},{0.14,0.28,0.06,0,0},{0.15,0.34,0,0,0}}},
{"LAUNCH PAD",8,{{0.13,0.32,0,0,0},{0.13,0.3,0,0,1},{0.13,0.28,0,0,0},{0.12,0.22,0,1,0},{0.13,0.3,0,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,-0.05,0,0},{0.15,0.34,0,0,0}}},
{"THE CLIMB",10,{{0.13,0.3,0,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.12,0.2,0,1,0},{0.13,0.26,-0.045,0,0},{0.13,0.26,-0.045,0,0},{0.13,0.26,0,0,0},{0.13,0.28,0.05,0,0},{0.13,0.28,0.05,0,0},{0.15,0.34,0,0,0}}},
{"DOWN AND OUT",7,{{0.13,0.32,0,0,0},{0.13,0.3,0.045,0,0},{0.13,0.3,0.045,0,0},{0.13,0.3,0.045,0,0},{0.13,0.28,0.045,0,0},{0.14,0.32,0,0,0},{0.14,0.32,0,0,0}}},
{"ZIGZAG",8,{{0.13,0.3,0,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,0.05,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,0.05,0,0},{0.13,0.28,-0.05,0,0},{0.14,0.32,0,0,0},{0.14,0.32,0.05,0,0}}},
{"GREEN MILE",8,{{0.13,0.26,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.14,0.3,0,0,0},{0.14,0.32,0,0,0}}},
{"GAUNTLET",9,{{0.13,0.3,0,0,0},{0.12,0.2,0,1,0},{0.13,0.28,0,0,0},{0.12,0.2,0,0,0},{0.13,0.28,0,0,0},{0.12,0.2,0,1,0},{0.13,0.28,0,0,0},{0.12,0.2,0,0,0},{0.14,0.34,0,0,0}}},
{"SKY STEPS",10,{{0.13,0.3,0,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,0,0,0},{0.13,0.28,0.06,0,0},{0.13,0.28,0.06,0,0},{0.15,0.34,0,0,0}}} };
#define LEVEL_COUNT 33
static int tierOf(int i) { return (sv.flipTiers[i / 16] >> ((i % 16) * 2)) & 3; }
static void setTier(int i, int t) { if (tierOf(i) >= t) return; sv.flipTiers[i / 16] = (sv.flipTiers[i / 16] & ~(3u << ((i % 16) * 2))) | ((u32)t << ((i % 16) * 2)); }

// ── building the course (the website's pad(), addPlatform(), buildLevel(), makeFlyable()) ──
static float rnd(float a, float b) { return a + frand() * (b - a); }
static float maxReach(float dy) {
    float vx = 0.022f * WW, vy = -0.038f * CH, d = vy * vy + 2 * G * dy;
    return vx * ((-vy + fsqrt(d > 0.0001f ? d : 0.0001f)) / G);
}
static void addPad(float x, float y, float w, int uneven, int chair) {
    if (nP >= MAXP) return;
    Pad *p = &P[nP++]; p->x = x; p->y = y; p->w = w; p->uneven = uneven; p->hazard = uneven; p->chair = chair;
    p->chairHit = p->graded = 0; float s = frand(); p->color = s < 0.3334f ? 0 : s < 0.6667f ? 1 : 2;
}
static float clampY(float y) { return y < CH * 0.44f ? CH * 0.44f : y > CH * 0.80f ? CH * 0.80f : y; }
static void addPlatform(void) {
    Pad *last = &P[nP - 1]; int n = nP, hard = n > 25; float d = hard ? ((n - 25) / 40.0f > 1 ? 1 : (n - 25) / 40.0f) : 0;
    if (hard && stairLeft <= 0 && frand() < 0.24f) { stairLeft = 3 + rand() % 3; stairStep = (frand() < 0.5f ? -1 : 1) * rnd(0.028f, 0.050f) * CH; }
    int uneven = !last->uneven && n > 6 && frand() < (hard ? 0.10f + d * 0.15f : 0.05f);
    float w = uneven ? rnd(0.16f, 0.23f) * WW : hard ? rnd(0.24f, 0.33f) * WW - d * 0.04f * WW : rnd(0.30f, 0.40f) * WW;
    float gap = uneven ? rnd(0.10f, 0.15f) * WW : hard ? rnd(0.14f, 0.21f) * WW + d * 0.08f * WW : rnd(0.11f, 0.17f) * WW;
    float y = stairLeft > 0 ? (stairLeft--, last->y + stairStep) : last->y + (hard ? rnd(-0.045f, 0.045f) : rnd(-0.018f, 0.018f)) * CH;
    y = clampY(y);
    float x = last->x + last->w + gap;
    if (!uneven) {
        Pad *lf = &P[lastFlat]; float lim = maxReach(y - lf->y) * 0.86f;
        if (x - lf->x > lim) x = lf->x + lim;
        float minX = last->x + last->w + WW * 0.05f; if (x < minX) x = minX;
    }
    addPad(x, y, w, uneven, !uneven && n > 3 && frand() < 0.15f);
    if (!uneven) lastFlat = nP - 1;
}
static void makeFlyable(void) {
    int last = 0, i = 1;
    while (i < nP) {
        Pad *p = &P[i];
        if (p->uneven) { i++; continue; }
        float lim = maxReach(p->y - P[last].y) * 0.86f;
        if (p->x - P[last].x > lim) {
            int cleared = 0;
            for (int k = last + 1; k < i; k++) if (P[k].uneven) { P[k].uneven = 0; cleared = 1; }
            if (cleared) { i = last + 1; continue; }
            float a = P[i - 1].x + P[i - 1].w + WW * 0.05f, b = P[last].x + lim; P[i].x = a > b ? a : b;
        }
        last = i; i++;
    }
    P[nP - 1].uneven = 0;
}
static void buildLevel(int i) {
    nP = 0; addPad(WW * 0.06f, CH * 0.70f, WW * 0.42f, 0, 0); lastFlat = 0; stairLeft = 0; stairStep = 0;
    if (i < 11) {
        const Level *L = &LEVELS[i];
        for (int k = 0; k < L->n; k++) { const PadSpec *s = &L->p[k]; Pad *last = &P[nP - 1];
            float y = clampY(last->y + s->d * CH), x = last->x + last->w + s->g * WW, w = s->w * WW;
            if (!s->u) { Pad *lf = &P[lastFlat]; float lim = maxReach(y - lf->y) * 0.86f;
                if (x - lf->x > lim) x = lf->x + lim;
                float minX = last->x + last->w + WW * 0.05f; if (x < minX) x = minX; }
            addPad(x, y, w, s->u, s->c); if (!s->u) lastFlat = nP - 1; }
    } else for (int k = 0; k < 10 + i; k++) addPlatform();
    makeFlyable();
}
static void buildEndless(void) { nP = 0; addPad(WW * 0.06f, CH * 0.70f, WW * 0.42f, 0, 0); lastFlat = 0; stairLeft = 0; stairStep = 0; for (int i = 0; i < 10; i++) addPlatform(); }
static void trimEndless(void) {                     // endless: forget pads far behind to keep going forever
    int drop = mk.pIdx - 4; if (drop <= 0 || flMode == M_LEVELS) return;
    memmove(P, P + drop, sizeof(Pad) * (nP - drop)); nP -= drop; lastFlat -= drop; if (lastFlat < 0) lastFlat = 0;
    mk.pIdx -= drop; if (mk.skipTo >= 0) mk.skipTo -= drop;
}
static void startRun(void) {
    camX = 0; score = 0; combo = 0; memset(pops, 0, sizeof pops); settle = 0; won = 0; gradePts = 0;
    mk.x = P[0].x + P[0].w * 0.5f; mk.y = P[0].y; mk.vx = mk.vy = mk.rot = mk.spin = 0; mk.pIdx = 0; mk.skipTo = -1;
    state = ST_READY; charging = 0; camX = mk.x - CW * HOLD / Z; if (camX < 0) camX = 0;
    timeLeft = 45; timerOn = 0; screen = S_FLIP;
}
static void startMode(int m) { flMode = m; if (m == M_LEVELS) buildLevel(levelIdx); else buildEndless(); startRun(); }

// ── the website's flip(), chairLaunch(), land() and step() ─────────────────
static void popAdd(float x, float y, const char *t, int c) {
    for (int i = 0; i < 6; i++) if (pops[i].life <= 0) { pops[i] = (Pop){ x, y, 44, "", c }; strncpy(pops[i].txt, t, 15); return; }
}
static void flip(float p, float am, float lf) {
    sfxPlop();
    mk.vx = (0.008f + p * 0.014f) * WW * (0.55f + 0.75f * (am + 0.5f > 0 ? am + 0.5f : 0));
    mk.vy = -(0.026f + p * 0.012f) * CH * lf;
    float T = 2 * fabsf_(mk.vy) / G;
    mk.spin = (6.2831853f * (1 + (int)(p * 2 + 0.5f))) / T; mk.rot = 0;
    state = ST_FLYING; timerOn = 1;
}
static void chairLaunch(int idx) {
    int target = idx + 3 + rand() % 3;
    if (flMode == M_LEVELS) { if (target > nP - 1) target = nP - 1; }
    else while (nP <= target + 10 && nP < MAXP) addPlatform();
    if (target > nP - 1) target = nP - 1;
    Pad *t = &P[target];
    if (t->uneven) { t->uneven = 0; if (target > lastFlat) lastFlat = target; }
    t->chair = 0;
    float dy = t->y - mk.y; mk.vy = -0.044f * CH;
    float d = mk.vy * mk.vy + 2 * G * dy, T = (-mk.vy + fsqrt(d > 0.0001f ? d : 0.0001f)) / G;
    mk.vx = ((t->x + t->w * 0.5f) - mk.x) / T;
    mk.spin = (6.2831853f * 4) / T; mk.skipTo = target; state = ST_BOOST;
    popAdd(mk.x, mk.y - MH * 1.4f, "CHAIR LAUNCH!", RED);
}
static void endGame(const char *title, int col, int win) {
    state = ST_OVER; timerOn = 0; won = win; overTitle = title; overCol = col;
    if (!win && strcmp(title, "TIME UP") && sv.sfxOn) sndFx(fs2_miss, FS2_MISS_LEN);
    if (mk.pIdx > sv.flipBestP) sv.flipBestP = mk.pIdx;
    if (score > sv.flipBest) sv.flipBest = score;
    if (score > sv.arcadeBest[ARC_FLIP]) sv.arcadeBest[ARC_FLIP] = score;
    sv.arcadePlays[ARC_FLIP]++;
    int c = score / 5; if (c > 20) c = 20;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite();
    screen = S_FLIP_OVER;
}
static void finishLevel(void) {
    if (levelIdx + 1 > sv.flipDone) sv.flipDone = levelIdx + 1;
    int maxPts = 0; for (int k = 1; k < nP; k++) if (!P[k].hazard) maxPts += P[k].color + 1;
    int got = gradePts < maxPts ? gradePts : maxPts; float r = maxPts > 0 ? (float)got / maxPts : 1;
    overTier = r >= 0.90f ? 3 : r >= 0.60f ? 2 : 1; overMax = maxPts; setTier(levelIdx, overTier);
    endGame("LEVEL CLEAR", LIME, 1);
}
static void land(int i) {
    Pad *p = &P[i];
    mk.y = p->y; mk.vx = mk.vy = 0; mk.rot = 0; mk.skipTo = -1;
    if (p->uneven) { state = ST_TIP; mk.tipT = 0; return; }
    mk.pIdx = i; combo++;
    int pts = p->color + 1;                              // red 1, green 2, blue 3
    if (!p->graded) { p->graded = 1; gradePts += pts; }
    score += pts;
    char t[8]; sprintf(t, "+%d", pts);
    popAdd(mk.x, p->y - MH * 1.3f, t, p->color == 0 ? RED : p->color == 2 ? C_LBLUE : LIME);
    state = ST_READY; settle = 9;
    if (flMode == M_LEVELS) { if (i >= nP - 1) finishLevel(); }
    else { while (nP < mk.pIdx + 11 && nP < MAXP) addPlatform(); trimEndless(); }
}
static void flipStep(void);
void updateFlip(void) { flipStep(); if (state == ST_FLYING || state == ST_BOOST || state == ST_TIP) flipStep(); }   // flights run at double speed
static void flipStep(void) {
    fFrames++;
    if (state == ST_OVER) return;
    if (settle > 0) settle--;
    if (flMode == M_TIMED && timerOn) { timeLeft -= 1.0f / 60; if (timeLeft <= 0) { timeLeft = 0; endGame("TIME UP", YELLOW, 0); return; } }
    if (state == ST_TIP) { mk.tipT++; mk.rot += 0.11f; if (mk.tipT > 26) endGame("TIPPED OVER", RED, 0); return; }
    if (state == ST_FLYING || state == ST_BOOST) {
        float prevY = mk.y;
        mk.x += mk.vx; mk.vy += G; mk.y += mk.vy; mk.rot += mk.spin;
        if (state == ST_FLYING)
            for (int i = 0; i < nP; i++) { Pad *p = &P[i]; if (!p->chair || p->chairHit) continue;
                float cx = p->x + p->w * 0.5f, cw = CHAIR_HH;
                if (mk.x > cx - cw * 0.5f && mk.x < cx + cw * 0.5f && mk.y > p->y - CHAIR_HH && mk.y < p->y + MH * 0.25f) { p->chairHit = 1; chairLaunch(i); break; } }
        if (mk.vy > 0) {
            if (state == ST_BOOST) { int i = mk.skipTo; if (i >= 0 && i < nP && mk.x >= P[i].x && mk.x <= P[i].x + P[i].w && prevY <= P[i].y && mk.y >= P[i].y) land(i); }
            else for (int i = 0; i < nP; i++) { Pad *p = &P[i]; if (mk.x >= p->x && mk.x <= p->x + p->w && prevY <= p->y && mk.y >= p->y) { land(i); break; } }
        }
        if (state != ST_OVER && mk.y > CH * 1.25f) endGame("OFF THE EDGE", RED, 0);
    }
    float tc = mk.x - CW * HOLD / Z; camX += (tc - camX) * 0.12f; if (camX < 0) camX = 0;
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) { pops[i].life--; pops[i].y -= CH * 0.0012f; }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputFlip(void) {
    if (screen == S_FLIP_PAUSE) {
        if (kDown & KEY_START) screen = S_FLIP;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_FLIP_PAUSE; charging = 0; return; }
    if (kDown & (KEY_L | KEY_R)) upright = !upright;
    if (state != ST_READY) { charging = 0; return; }
    if (kHeld & KEY_LEFT)  { aim -= 0.03f; if (aim < -1) aim = -1; }
    if (kHeld & KEY_RIGHT) { aim += 0.03f; if (aim > 1) aim = 1; }
    if (kHeld & KEY_UP)    { lift += 0.02f; if (lift > 1.35f) lift = 1.35f; }
    if (kHeld & KEY_DOWN)  { lift -= 0.02f; if (lift < 0.35f) lift = 0.35f; }
    if (kDown & KEY_A) { charging = 1; ph = 0; }
    if (charging) {
        ph++; float p = (ph % 44) / 22.0f; power = p < 1 ? p : 2 - p; if (power < 0.1f) power = 0.1f;
        if (kDown & KEY_B) charging = 0;
        else if (kUp & KEY_A) { charging = 0; if (motionTap) power = motionPower < 0.1f ? 0.1f : motionPower; flip(power, aim, lift); }
    }
}

// ── drawing: the DS's screen picture (SX, SY), then 2/3 of it from DS row 90 down ──
#define FS (2.0f / 3.0f)
static float SX(float w) { return (w - camX) * Z; }
static float SY(float w) { return AY + (w - AY) * Z; }
static int gx(float dsx) { return (int)(dsx * FS); }
static int gy(float dsy) { return (int)((dsy - FL_Y0) * FS); }
// tiles: the three pads (lying markers, 64x32), the main game's chair (64x64), your marker
// (32x32); palettes 0-2 the pads, 3 the chair, 4 the marker
enum { T_FP = 512, T_FCH = 608, T_FM = 672 };
void flipObjLoad(void) {
    const u8 *Pd[3] = { fl_p0, fl_p1, fl_p2 }; const u16 *PP[3] = { fl_p0_pal, fl_p1_pal, fl_p2_pal };
    for (int i = 0; i < 3; i++) { platObjTiles(T_FP + i * 32, Pd[i], 1024); memcpy(&objPal[i * 16], PP[i], 32); }
    platObjTiles(T_FCH, obj_chair, 2048); memcpy(&objPal[48], obj_chair_pal, 32);
    platObjTiles(T_FM, fl_m, 512); memcpy(&objPal[64], fl_m_pal, 32);
}
static int nObj;
static void aff(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, float sx, float sy, float ang, int dbl) {
    if (nObj >= 32 || sx < 0.02f || sy < 0.02f || cx < -w * 2 || cx > SW + w * 2 || cy < -h * 2 || cy > SH + h * 2) return;
    int i = nObj++;
    float c = fcos(ang), s = fsin(ang);
    oam[i * 4].a3 = (u16)(s16)(c / sx * 256); oam[i * 4 + 1].a3 = (u16)(s16)(s / sx * 256);
    oam[i * 4 + 2].a3 = (u16)(s16)(-s / sy * 256); oam[i * 4 + 3].a3 = (u16)(s16)(c / sy * 256);
    int bw = dbl ? w : w / 2, bh = dbl ? h : h / 2;
    oam[i].a0 = (u16)(((cy - bh) & 0xFF) | (dbl ? 0x0300 : 0x0100) | (shape << 14));
    oam[i].a1 = (u16)(((cx - bw) & 0x1FF) | (i << 9) | (size << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
static void drawBg(void) {
    int ox = (int)(camX * 0.10f * FS);                         // the website's 0.10 parallax
    ox = (ox % FLW + FLW) % FLW;
    drawDirect();
    const u8 *img = fl8[ox & 3]; int sx0 = ox & ~3;            // (the copy shifted by ox's last 2 bits)
    for (int y = 0; y < SH; y++) {
        const u8 *row = &img[y * FLW]; u16 *d = &page[y * (SW / 2)];
        int run = FLW - sx0; if (run > SW) run = SW;
        copy32(d, row + sx0, run / 4);
        if (run < SW) copy32(d + run / 2, row, (SW - run) / 4);
    }
}
static void drawPad(Pad *p) {
    float x0 = SX(p->x), x1 = SX(p->x + p->w), y = SY(p->y);
    if (gx(x1) < -20 || gx(x0) > SW + 20) return;
    if (p->uneven) {                                          // jagged red ground
        int teeth = (int)(p->w / (CW * 0.05f)); if (teeth < 4) teeth = 4;
        for (int i = 0; i < teeth; i++) {
            float ax = x0 + (x1 - x0) * i / teeth, bx = x0 + (x1 - x0) * (i + 1) / teeth, ay = y + ((i % 2) ? -CH * 0.016f * Z : 0), by = y + (((i + 1) % 2) ? -CH * 0.016f * Z : 0);
            for (int k = 0; k <= 8; k++) { int px = gx(ax + (bx - ax) * k / 8), py = gy(ay + (by - ay) * k / 8); pset(px, py, RED); pset(px, py + 1, RED); }
        }
        return;
    }
    // a pad is a marker lying on its side; the picture is 56 long
    float len = (x1 - x0) * FS;
    aff(T_FP + p->color * 32, p->color, 1, 3, 64, 32, gx((x0 + x1) / 2), gy(y + CH * 0.015f * Z), len / 56, len / 56, 0, len > 62);
    if (p->chair && !p->chairHit) {
        float ch = CHAIR_HH * Z * FS;
        aff(T_FCH, 3, 0, 3, 64, 64, gx((x0 + x1) / 2), (int)(gy(y) - ch / 2), ch / 40, ch / 40, 0, 0);
    }
}
static void drawMk(void) {
    // the website's sizing: the marker is MH long; upright stands on its end, flat lies on its side
    float len = MH * Z * FS, sc = len / 18;
    float base = upright ? -1.5707963f : 0, visH = upright ? len : 8 * sc;
    float sq = settle > 0 ? 1 + settle * 0.012f : 1;
    float cy = gy(SY(mk.y)) - visH / 2;
    if (state == ST_TIP) cy += mk.tipT * 0.3f * FS;
    aff(T_FM, 4, 0, 2, 32, 32, gx(SX(mk.x)), (int)cy, sc * sq, sc * sq, base + mk.rot, 1);
}
void drawFlip(void) {
    char s[32];
    drawBg();
    hideSprites(); nObj = 0;
    drawMk();                                                 // (first: on top of the pads)
    for (int i = 0; i < nP; i++) drawPad(&P[i]);
    if (state == ST_READY) {                                  // where it'll go
        float p = charging ? power : 0.5f;
        float vx = (0.008f + p * 0.014f) * WW * (0.55f + 0.75f * (aim + 0.5f > 0 ? aim + 0.5f : 0)), vy = -(0.026f + p * 0.012f) * CH * lift;
        float x = mk.x, y = mk.y - MH * 0.5f;
        for (int k = 0; k < 40; k++) { x += vx; vy += G; y += vy; if (k % 3 == 0) rect(gx(SX(x)) - 1, gy(SY(y)) - 1, 2, 2, charging ? YELLOW : C_DIMWHITE); }
    }
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) text(gx(SX(pops[i].x)) - textW(pops[i].txt, 1) / 2, gy(SY(pops[i].y)), pops[i].txt, pops[i].col, 1);
    sprintf(s, "%d PTS", score); text(4, 2, s, WHITE, 1);
    if (flMode == M_LEVELS) { sprintf(s, "%d. %s", levelIdx + 1, levelIdx < 11 ? LEVELS[levelIdx].name : "LEVEL"); text(SW - 4 - textW(s, 1), 2, s, GOLD, 1); }
    else if (flMode == M_TIMED) { sprintf(s, "TIME %d", (int)(timeLeft + 0.99f)); text(SW - 4 - textW(s, 1), 2, s, timeLeft < 10 ? RED : GOLD, 1); }
    else { sprintf(s, "BEST %d", sv.flipBest); text(SW - 4 - textW(s, 1), 2, s, GOLD, 1); }
    if (state == ST_READY && score == 0 && mk.pIdx == 0) textC(18, "left/right far, up/down high, hold A", GREY, 1);
    const char *o = upright ? "1/2: UPRIGHT" : "1/2: FLAT"; textS(SW - 4 - textSW(o), SH - 11, o, WHITE);
    if (screen == S_FLIP_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── menus ─────────────────────────────────────────────────────────────────
static Btn FB[4]; static int menuSel, lvlSel, overSel;
static void menuLayout(void) {
    FB[0] = (Btn){ 136, 14, 100, 28, "LEVELS", 0, 0 }; FB[1] = (Btn){ 136, 48, 100, 28, "ENDLESS", 0, 0 };
    FB[2] = (Btn){ 136, 82, 100, 28, "TIMED", 0, 0 }; FB[3] = (Btn){ 136, 120, 100, 26, "BACK", 0, 0 };
}
void drawFlipMenu(void) {
    char s[40];
    fillScreen(DARK);
    text(6, 2, "QU33PH FLIP", GOLD, 1);
    static const char *L[7] = { "Flip the marker pad to pad.", "Left/right: how far,", "up/down: how high,", "hold A for power.",
        "red pad 1  green 2  blue 3", "chairs launch you ahead;", "jagged red ground tips you" };
    static const int C[7] = { WHITE, WHITE, WHITE, WHITE, WHITE, C_GOLD, RED };
    for (int i = 0; i < 7; i++) textS(6, 20 + i * 12, L[i], C[i]);
    sprintf(s, "LEVELS %d / %d   BEST %d", sv.flipDone, LEVEL_COUNT, sv.flipBest); textS(6, 112, s, GREY);
    coinCount(6, 136, 0);
    menuLayout(); drawBtns(FB, 4, menuSel);
}
void inputFlipMenu(void) {
    menuLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(FB, 4, &menuSel, 1);
    if (h == 0) goScreen(S_FLIP_LEVELS);
    if (h == 1) startMode(M_ENDLESS);
    if (h == 2) startMode(M_TIMED);
    if (h == 3) goScreen(S_ARCADE);
}
static Btn LB[LEVEL_COUNT + 1] EWRAM_BSS;
static void lvlLayout(void) {
    for (int i = 0; i < LEVEL_COUNT; i++) {            // little keys, coloured by the marker you earned
        int t = tierOf(i), open = i <= sv.flipDone;
        LB[i] = (Btn){ 5 + (i % 11) * 21, 52 + (i / 11) * 24, 20, 22, "", t == 3 ? C_LBLUE : t == 2 ? LIME : t == 1 ? RED : 0, !open };
        sprintf(LB[i].label, "%d", i + 1);
    }
    LB[LEVEL_COUNT] = (Btn){ 70, 128, 100, 26, "BACK", 0, 0 };
}
void drawFlipLevels(void) {
    char s[40];
    fillScreen(DARK);
    sprintf(s, "FLIP LEVELS   %d / %d cleared", sv.flipDone, LEVEL_COUNT); textS(6, 3, s, GOLD);
    textS(6, 15, "grade: RED - GREEN (60%) - BLUE (90%)", GREY);
    if (lvlSel < LEVEL_COUNT) { sprintf(s, "%d. %s", lvlSel + 1, lvlSel < 11 ? LEVELS[lvlSel].name : "BUILT FOR YOU"); textC(30, s, WHITE, 1); }
    lvlLayout(); drawBtns(LB, LEVEL_COUNT + 1, lvlSel);
}
void inputFlipLevels(void) {
    lvlLayout();
    if (kDown & KEY_B) { goScreen(S_FLIP_MENU); return; }
    int h = btnInput(LB, LEVEL_COUNT + 1, &lvlSel, 11);
    if (h == LEVEL_COUNT) goScreen(S_FLIP_MENU);
    else if (h >= 0 && h <= sv.flipDone) { levelIdx = h; startMode(M_LEVELS); }
}
static Btn OB[3];
static int overN(void) {
    int next = won && flMode == M_LEVELS && levelIdx < LEVEL_COUNT - 1, n = 0;
    if (next) { OB[n] = (Btn){ 30, 94 + n * 0, 88, 24, "NEXT LEVEL", 0, 0 }; n++; }
    OB[n] = (Btn){ next ? 122 : 30, 94, next ? 88 : 180, 24, "RETRY", 0, 0 }; n++;
    OB[n] = (Btn){ 30, 120, 180, 22, "MENU", 0, 0 }; n++;
    return n;
}
void drawFlipOver(void) {
    char s[48];
    drawFlip();
    hideSprites();
    box(20, 12, 200, 136, C_TOAST, GOLD);
    textCW(22, 196, 16, overTitle, overCol, 2);
    if (won && flMode == M_LEVELS) {
        int got = gradePts < overMax ? gradePts : overMax;
        sprintf(s, "%s - %d / %d PTS", levelIdx < 11 ? LEVELS[levelIdx].name : "LEVEL", got, overMax); textCW(22, 196, 46, s, WHITE, 1);
        const char *T[4] = { "", "RED MARKER", "GREEN MARKER", "BLUE MARKER" }; const int C[4] = { 0, RED, LIME, C_LBLUE };
        textCW(22, 196, 62, T[overTier], C[overTier], 1);
    } else { sprintf(s, "%d PTS - %d PADS - BEST %d", score, mk.pIdx, sv.flipBest); textCW(22, 196, 52, s, WHITE, 1); }
    sprintf(s, "+%d COINS", coinsWon); textCW(22, 196, 76, s, GOLD, 1);
    int n = overN(); drawBtns(OB, n, overSel);
}
void inputFlipOver(void) {
    int n = overN(), h = btnInput(OB, n, &overSel, 2);
    if (kDown & KEY_B) { goScreen(S_FLIP_MENU); return; }
    if (h < 0) return;
    const char *l = OB[h].label;
    if (!strcmp(l, "NEXT LEVEL")) { levelIdx++; startMode(M_LEVELS); }
    else if (!strcmp(l, "RETRY")) startMode(flMode);
    else goScreen(S_FLIP_MENU);
}
