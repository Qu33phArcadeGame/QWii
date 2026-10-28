// mini.c — the arcade menu, and MINI QU33PH (the website's mini.html, as on the DS).
//
// The website's far-end table photo, the whole lane on one screen at a bit under half the DS's
// size (just the top of the brick wall is cropped), with the scores and the turn's popups in the
// panel beside it. All positions use the website's own lane maths (u across the table 0..1, v up
// the lane 0..1) and its photo calibration, so the physics, scoring and timings are the
// website's, step for step at 60 fps. The markers, their shadows, the case and the chairs are
// hardware sprites, scaled and turned by the GBA itself.
//
// CONTROLS: D-pad left/right moves along the near edge, L/R angles the throw, hold A for power
// and let go (B cancels). START pauses (SELECT then quits to the arcade).
#include "qu.h"

// ══ ARCADE MENU ═══════════════════════════════════════════════════════════
static const char *ARC_NAME[ARC_COUNT] = { "MINI QU33PH", "QU33PH-BALL", "FIDGET", "BOWLING", "STACK", "FLIP", "DOZER", "JUMP", "PINBALL" };
static const int ARC_READY[ARC_COUNT] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const char *ARC_BLURB[ARC_COUNT] = { "four mini markers, three tables", "three machines, nine markers", "spinner air hockey",
    "ten frames, marker pins", "build the tower", "flick from pad to pad", "coin pusher", "climb, run, bounce & flap", "the ball is a marker" };
static const char *CAB_LABEL[ARC_COUNT] = { "MINI", "BALL", "FIDGET", "BOWLING", "STACK", "FLIP", "DOZER", "JUMP", "PINBALL" };
// the website's arcade layout: five cabinets across, four centred below (its order)
static const int SLOT_GAME[9] = { ARC_JUMP, ARC_PINBALL, ARC_BOWLING, ARC_STACK, ARC_FIDGET, ARC_FLIP, ARC_BALL, ARC_DOZER, ARC_MINI };
static int arcSel = 8, arcMsgT;
static void cabPos(int k, int *x, int *y) {
    int row = k < 5 ? 0 : 1, col = row ? k - 5 : k, n = row ? 4 : 5, x0 = (SW - (n * 46 + (n - 1) * 2)) / 2;
    *x = x0 + col * 48; *y = 20 + row * 64;
}
void drawArcade(void) {
    fillScreen(DARK);
    textS(4, 2, "ARCADE", GOLD);
    { char c[16]; sprintf(c, "coins %d", sv.coins); textS(SW - 4 - textSW(c), 2, c, GOLD); }
    for (int k = 0; k < 9; k++) {
        int g = SLOT_GAME[k], x, y, on = k == arcSel; cabPos(k, &x, &y);
        if (on) { rect(x - 1, y - 3, 48, 2, GOLD); rect(x - 1, y + 64, 48, 2, GOLD); rect(x - 1, y - 3, 2, 69, GOLD); rect(x + 45, y - 3, 2, 69, GOLD); }
        drawImg(arc8[g], ARC_W, ARC_H, x + 1, y - (on ? 2 : 0));
        if (!ARC_READY[g]) stipple(x + 1, y - (on ? 2 : 0), ARC_W, ARC_H, C_DARK);   // not here yet: dimmed
        const char *l = CAB_LABEL[g];
        textS(x + 23 - textSW(l) / 2, y + 55, l, on ? YELLOW : ARC_READY[g] ? WHITE : GREY);
    }
    int g = SLOT_GAME[arcSel];
    char s[48];
    rect(0, 14, SW, 1, C_PANELLINE);
    if (arcMsgT > 0) { arcMsgT--; textS(4, 148, "coming soon to the Wii", C_FO); }
    else if (sv.arcadePlays[g]) { sprintf(s, "%s   best %d   played %d", ARC_NAME[g], sv.arcadeBest[g], sv.arcadePlays[g]); textS(4, 148, s, YELLOW); }
    else { sprintf(s, "%s - %s", ARC_NAME[g], ARC_BLURB[g]); textS(4, 148, s, ARC_READY[g] ? YELLOW : GREY); }
}
static int arcNav(int cur, int dx, int dy) {
    int cx, cy, best = cur, bd = 1 << 30; cabPos(cur, &cx, &cy);
    for (int i = 0; i < 9; i++) { if (i == cur) continue;
        int x, y; cabPos(i, &x, &y); x -= cx; y -= cy;
        int along = dx ? x * dx : y * dy, across = dx ? (y < 0 ? -y : y) : (x < 0 ? -x : x);
        if (along <= 0 || (dx && across > 30)) continue;
        int d = along + across * 2; if (d < bd) { bd = d; best = i; } }
    return best;
}
void inputArcade(void) {
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    if (arcSel < 0 || arcSel > 8) arcSel = 8;
    if (kDown & KEY_UP) arcSel = arcNav(arcSel, 0, -1);
    if (kDown & KEY_DOWN) arcSel = arcNav(arcSel, 0, 1);
    if (kDown & KEY_LEFT) arcSel = arcNav(arcSel, -1, 0);
    if (kDown & KEY_RIGHT) arcSel = arcNav(arcSel, 1, 0);
    if (kDown & KEY_A) {
        int g = SLOT_GAME[arcSel];
        if (g == ARC_MINI) goScreen(S_MINI_MENU);
        else if (g == ARC_BALL) goScreen(S_BALL_MENU);
        else if (g == ARC_FIDGET) goScreen(S_FIDGET_MENU);
        else if (g == ARC_BOWLING) goScreen(S_BOWL_MENU);
        else if (g == ARC_STACK) goScreen(S_STACK_MENU);
        else if (g == ARC_FLIP) goScreen(S_FLIP_MENU);
        else if (g == ARC_DOZER) goScreen(S_DOZER_MENU);
        else if (g == ARC_PINBALL) goScreen(S_PIN_MENU);
        else if (g == ARC_JUMP) goScreen(S_JUMP_MENU);
        else arcMsgT = 120;
    }
}

// ══ MINI QU33PH ═══════════════════════════════════════════════════════════
// the lane, measured from the website's far-end photo (VIEW.far in mini.html)
#define INV_A   2.311f
#define INV_B   2.694f
#define HW0     0.2094f
#define FY0     0.167f
#define SLOPE   0.3414f
#define VCX     0.4968f
#define MINI_PH 455
#define MINI_OFF 16
#define LANE_AR 4.0f
#define V_DESTINY 0.5f                 // the seam between the first two tables
#define V_WALL  1.0f
#define WALL_BOUNCE 0.42f
#define MK_LEN  0.115f
#define RAD     (MK_LEN * 0.42f)
#define TOUCH_AR 3.2f
#define TOUCH_R (RAD * 2.6f)
#define BOXV 0.74f
#define BOXU 0.5f
#define BOXW 0.15f
#define BOXH 0.045f
#define DT (1.0f / 60.0f)
#define DRAG 0.964801f                 // exp(-2.15 / 60)
#define SPIN_DECAY 0.957592f           // exp(-2.6 / 60)
#define TW MINIT_W                     // the table's width on screen; the panel is the rest
enum { MC_GREEN, MC_PINK, MC_YELLOW, MC_BLUE };

typedef struct { int col; float u, v, du, dv, rot, spin, sndCd; int off, cap, hitByBlue; } MM;
static MM mk[4];
static int nmk, order[4], idx, live = -1, scored;
static int matchSecs = 60, sudden, score, turnPts, turn, coinsWon, newBest;
static float tLeft, suddenAt, turnPause, aimU = 0.5f, aimA, shakeT, shakeAmt;
static int shY;

// ── popups & the end-of-turn breakdown ────────────────────────────────────
typedef struct { char t[32]; int c; int life, big, peef; } Pop;
static Pop pops[8];
static void popsClear(void) { memset(pops, 0, sizeof pops); }
static void popAdd(const char *t, int c, int big, int peef) {     // newest at the end, like the website's list
    int n = 0; while (n < 8 && pops[n].life > 0) n++;
    if (n == 8) { for (int i = 0; i < 7; i++) pops[i] = pops[i + 1]; n = 7; }
    Pop *p = &pops[n]; strncpy(p->t, t, 31); p->t[31] = 0; p->c = c; p->big = big; p->peef = peef; p->life = big ? 96 : 70;
}
static char msgL[6][32]; static int msgN, msgT;
static void addShake(float a) { if (a > shakeAmt) shakeAmt = a; shakeT = 0.42f; }

// ── sound (one effect at a time on the GBA: the newest plays) ─────────────
static void play(const s8 *d, int len) { if (sv.sfxOn) sndFx(d, len); }
static void sCap(void) { play(ms_cap, MS_CAP_LEN); }
static void playImpact(float hard) {
    int r = rand() & 1;
    if (hard < 1.0f) { if (r) play(ms_short1, MS_SHORT1_LEN); else play(ms_short2, MS_SHORT2_LEN); }
    else if (hard < 2.0f) { if (r) play(ms_hit1, MS_HIT1_LEN); else play(ms_hit2, MS_HIT2_LEN); }
    else { if (r) play(ms_long2, MS_LONG2_LEN); else play(ms_long3, MS_LONG3_LEN); }
}
static void playThrow(float p) {
    if (p < 0.3f) play(ms_throw1, MS_THROW1_LEN);
    else if (p < 0.6f) play(ms_throw2, MS_THROW2_LEN);
    else if (p < 0.85f) play(ms_throw3, MS_THROW3_LEN);
    else play(ms_throw4, MS_THROW4_LEN);
}
static void playPeef(float v) {
    if (v < 0.33f) play(ms_peef1, MS_PEEF1_LEN);
    else if (v < 0.66f) play(ms_peef2, MS_PEEF2_LEN);
    else play(ms_peef3, MS_PEEF3_LEN);
}

// ── the lane → the screen ─────────────────────────────────────────────────
// (the DS's projection onto its 256-wide, two-screen picture, then shrunk to the GBA's)
static float hwOf(float v) { return 1.0f / (INV_A + INV_B * v); }
static void proj(float u, float v, int *x, int *y, float *s) {
    float hw = hwOf(v), fy = FY0 + (hw - HW0) / SLOPE;
    *x = (int)((VCX + (u - 0.5f) * 2 * hw) * 256 * MINIT_S);
    *y = (int)((fy * MINI_PH - MINI_OFF - MINIT_Y0) * MINIT_S) + shY;
    if (s) *s = hw * 256 * MINIT_S;
}

static int charging, chargeT, ph;
static float power;

// ── a match ───────────────────────────────────────────────────────────────
static void newTurn(void) {
    memset(mk, 0, sizeof mk); nmk = 0; live = -1; idx = 0; turnPts = 0; scored = 0;
    int three[3] = { MC_GREEN, MC_PINK, MC_YELLOW };
    for (int i = 2; i > 0; i--) { int j = rand() % (i + 1), t = three[i]; three[i] = three[j]; three[j] = t; }
    order[0] = three[0]; order[1] = three[1]; order[2] = three[2]; order[3] = MC_BLUE;   // blue always goes last
}
static void startMini(int secs) {
    matchSecs = secs; suddenAt = secs * 0.2f; tLeft = secs;
    sudden = 0; score = 0; turn = 1; coinsWon = 0; newBest = 0; turnPause = 0;
    popsClear(); msgN = 0; msgT = 0; shakeT = 0; shakeAmt = 0;
    newTurn(); charging = 0; chargeT = 0;
    screen = S_MINI;
}
static int nextColor(void) { return idx < 4 ? order[idx] : -1; }
static void launch(float pw, float aim) {
    int col = nextColor(); if (col < 0 || live >= 0) return;
    float p = pw < 0.06f ? 0.06f : pw > 1 ? 1 : pw, a = aim < -1 ? -1 : aim > 1 ? 1 : aim;
    MM *m = &mk[nmk];
    memset(m, 0, sizeof *m);
    m->col = col; m->u = aimU; m->v = 0.02f;
    m->du = a * 0.52f * (0.5f + p) + (frand() - 0.5f) * 0.05f;
    m->dv = 0.55f + p * 1.52f;
    m->rot = (frand() - 0.5f) * 0.5f; m->spin = (frand() - 0.5f) * 7 * (0.4f + p);
    m->cap = 1;
    live = nmk; nmk++; idx++;
    playThrow(p);
}
static void fellOff(MM *m, int byBlue) {
    m->off = 1;
    int d;
    if (sudden) d = (m->v > V_DESTINY) ? 10 : 6;              // SD: off past the line, or short of the chair
    else if (m->col == MC_BLUE) d = (m->v < V_DESTINY) ? 10 : 0;
    else d = (m->v < V_DESTINY) ? 50 : 0;                   // any other marker off before it clears the first table
    if (byBlue) d += 6;                                     // blue knocking another marker off
    playPeef(m->v);
    popAdd("PEEF", RED, 1, 1);
    if (d > 0) { char t[32]; turnPts -= d; sprintf(t, "-%d%s", d, byBlue ? " KNOCKED OFF" : ""); popAdd(t, RED, 0, 1); }
    addShake(12);
}
static void popCap(MM *m) {
    if (!m->cap) return;
    m->cap = 0;
    int d = m->col == MC_BLUE ? 6 : 3; char t[24];
    turnPts -= d; sprintf(t, "-%d CAP OFF", d); popAdd(t, C_FO, 0, 0);
    sCap();
}
static float lenUV(float du, float dv) { return fsqrt(du * du + dv * dv); }
static int touchingM(MM *a, MM *b) { float dx = a->u - b->u, dy = (a->v - b->v) * TOUCH_AR; return lenUV(dx, dy) < TOUCH_R; }
static int scoreTable(void) {
    MM *on[4]; int n = 0;
    for (int i = 0; i < nmk; i++) if (!mk[i].off) on[n++] = &mk[i];
    int par[4]; for (int i = 0; i < n; i++) par[i] = i;
    int pairs = 0, bluePairs = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
        if (!touchingM(on[i], on[j])) continue;
        pairs++;
        if (on[i]->col == MC_BLUE || on[j]->col == MC_BLUE) bluePairs++;
        int a = i, b = j; while (par[a] != a) a = par[a]; while (par[b] != b) b = par[b];
        if (a != b) par[a] = b;
    }
    int sizes[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < n; i++) { int r = i; while (par[r] != r) r = par[r]; sizes[r]++; }
    int pts = 0; msgN = 0;
    if (sudden) {
        if (pairs) { pts += pairs * 6; sprintf(msgL[msgN++], "TOUCHING x%d  +%d", pairs, pairs * 6); }
        if (bluePairs) { pts += bluePairs * 10; sprintf(msgL[msgN++], "BLUE TOUCHING  +%d", bluePairs * 10); }
        for (int i = 0; i < 4; i++) { if (sizes[i] == 3) { pts += 15; strcpy(msgL[msgN++], "THREE TOUCHING  +15"); }
                                      if (sizes[i] == 4) { pts += 20; strcpy(msgL[msgN++], "ALL FOUR  +20"); } }
    } else {
        if (pairs) { pts += pairs; sprintf(msgL[msgN++], "TOUCHING x%d  +%d", pairs, pairs); }
        if (bluePairs) { pts += bluePairs * 3; sprintf(msgL[msgN++], "BLUE TOUCHING  +%d", bluePairs * 3); }
        for (int i = 0; i < 4; i++) { if (sizes[i] == 3) { pts += 5; strcpy(msgL[msgN++], "THREE TOUCHING  +5"); }
                                      if (sizes[i] == 4) { pts += 10; strcpy(msgL[msgN++], "QU33PH!  +10"); } }
    }
    int mega = 0;                                            // MEGA QU33PH: any marker resting on the box
    for (int i = 0; i < n; i++) if (fabsf_(on[i]->u - BOXU) < BOXW / 2 && fabsf_(on[i]->v - BOXV) < BOXH / 2) mega++;
    if (mega && msgN < 6) { pts += mega * 30; sprintf(msgL[msgN++], "MEGA QU33PH x%d  +%d", mega, mega * 30); }
    return pts;
}
static void endTurn(void) {
    turnPts += scoreTable();
    int extra = 0;
    if (turnPts >= 33) { turnPts += 10; extra = 1; }          // 33 in a turn buys another turn and 10 more
    score += turnPts;
    if (!msgN) { strcpy(msgL[0], turnPts < 0 ? "ROUGH TURN" : "NOTHING TOUCHING"); msgN = 1; }
    msgT = 150;
    char t[32]; sprintf(t, "%s%d TURN %d", turnPts >= 0 ? "+" : "", turnPts, turn);
    popAdd(t, turnPts >= 0 ? YELLOW : RED, 1, 0);
    if (extra) popAdd("33+ EXTRA TURN +10", C_CYAN, 1, 0);
    else turn++;
    turnPause = 0.45f;
}
static void gameOver(void) {
    live = -1; shakeT = 0; charging = 0; chargeT = 0;
    sv.arcadePlays[ARC_MINI]++;
    newBest = score > sv.arcadeBest[ARC_MINI];               // as on the website, the best starts at 0
    if (newBest) sv.arcadeBest[ARC_MINI] = score;
    coinsWon = score > 0 ? score / 10 : 0;
    if (coinsWon > 0) addCoins(coinsWon);
    saveWrite();
    screen = S_MINI_OVER;
}

// ── physics: the website's step(), at a fixed 1/60 s ──────────────────────
static int chairCount(void) { return sudden ? 2 : 0; }
static const float CH_U[2] = { 0.5f, 0.5f }, CH_V[2] = { 0.62f, 0.86f }, CH_W = 0.17f, CH_H = 0.045f;
void updateMini(void) {
    tLeft -= DT;
    if (!sudden && tLeft <= suddenAt) { sudden = 1; popAdd("SUDDEN DEATH", RED, 1, 0); }
    if (tLeft <= 0) { tLeft = 0; gameOver(); return; }
    int moving = 0;
    for (int i = 0; i < nmk; i++) {
        MM *m = &mk[i];
        if (m->off) continue;
        if (m->sndCd > 0) m->sndCd -= DT;
        m->u += m->du * DT; m->v += m->dv * DT;
        m->rot += m->spin * DT; m->spin *= SPIN_DECAY;
        m->du *= DRAG; m->dv *= DRAG;
        if (fabsf_(m->du) + fabsf_(m->dv) < 0.045f) { m->du = 0; m->dv = 0; m->spin *= 0.7f; } else moving = 1;
        for (int c = 0; c < chairCount(); c++) {                 // chairs are solid in sudden death
            if (fabsf_(m->u - CH_U[c]) < CH_W / 2 + RAD && fabsf_(m->v - CH_V[c]) < CH_H / 2 + RAD) {
                float push = (m->u < CH_U[c]) ? -1 : 1, preDv = m->dv;
                m->u = CH_U[c] + push * (CH_W / 2 + RAD); m->du = fabsf_(m->du) * push * 0.55f; m->spin += push * 4;
                if (!(m->sndCd > 0)) { playImpact(fabsf_(preDv) * 1.4f); m->sndCd = 0.09f; }
                if (fabsf_(m->dv) > 1.1f) popCap(m);
            }
        }
        if (m->v > V_WALL - RAD) {                              // the far end is a wall, not a drop
            m->v = V_WALL - RAD;
            if (m->dv > 0) {
                m->dv = -m->dv * WALL_BOUNCE;
                m->spin += (frand() - 0.5f) * 3 - m->du * 1.5f;
                if (!(m->sndCd > 0)) { play(ms_wall, MS_WALL_LEN); m->sndCd = 0.09f; }
                if (fabsf_(m->dv) > 1.35f) popCap(m);
                addShake(4);
            }
        }
        if (m->u < -0.02f || m->u > 1.02f) { fellOff(m, m->hitByBlue); if (i == live) live = -1; }   // PEEF: only ever the sides
    }
    for (int i = 0; i < nmk; i++) for (int j = i + 1; j < nmk; j++) {     // marker on marker
        MM *a = &mk[i], *b = &mk[j]; if (a->off || b->off) continue;
        float dx = a->u - b->u, dy = (a->v - b->v) * LANE_AR, d = lenUV(dx, dy);
        if (d >= RAD * 2 || d < 1e-6f) continue;
        float nx = dx / d, ny = dy / d, ov = (RAD * 2 - d) / 2;
        a->u += nx * ov; a->v += ny * ov / LANE_AR; b->u -= nx * ov; b->v -= ny * ov / LANE_AR;
        float rvx = a->du - b->du, rvy = (a->dv - b->dv) * LANE_AR, sep = rvx * nx + rvy * ny;
        if (sep < 0) {
            float imp = sep * 0.86f;
            a->du -= imp * nx; a->dv -= imp * ny / LANE_AR; b->du += imp * nx; b->dv += imp * ny / LANE_AR;
            a->spin += (frand() - 0.5f) * 7; b->spin += (frand() - 0.5f) * 7;
            if (!(a->sndCd > 0) && !(b->sndCd > 0)) { playImpact(fabsf_(sep)); a->sndCd = b->sndCd = 0.09f; }
            if (fabsf_(sep) > 1.5f) { if (frand() < 0.30f) popCap(a); if (frand() < 0.30f) popCap(b); }
            if (a->col == MC_BLUE) b->hitByBlue = 1;
            if (b->col == MC_BLUE) a->hitByBlue = 1;
            moving = 1;
        }
    }
    if (live >= 0 && !moving) live = -1;
    if (turnPause > 0) { turnPause -= DT; if (turnPause <= 0) newTurn(); }
    else if (live < 0 && idx >= 4 && !moving && nmk > 0 && !scored) { scored = 1; endTurn(); }
    int k = 0;                                               // age the popups, dropping finished ones in order
    for (int i = 0; i < 8; i++) if (pops[i].life > 0 && --pops[i].life > 0) pops[k++] = pops[i];
    for (; k < 8; k++) pops[k].life = 0;
    if (msgT > 0) msgT--;
    if (shakeT > 0) { shakeT -= DT; if (shakeT <= 0) { shakeT = 0; shakeAmt = 0; } }
}

// ── input ─────────────────────────────────────────────────────────────────
static int canThrow(void) { return screen == S_MINI && live < 0 && nextColor() >= 0 && turnPause <= 0; }
void inputMini(void) {
    if (screen == S_MINI_PAUSE) {
        if (kDown & (KEY_START | KEY_A)) screen = S_MINI;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_MINI_PAUSE; charging = 0; return; }
    // walk the throw spot along the near edge, angle it, hold A for power
    if (canThrow()) {
        if (kHeld & KEY_LEFT)  { aimU -= 0.45f * DT; if (aimU < 0.06f) aimU = 0.06f; chargeT = 90; }
        if (kHeld & KEY_RIGHT) { aimU += 0.45f * DT; if (aimU > 0.94f) aimU = 0.94f; chargeT = 90; }
        if (kHeld & KEY_L) { aimA -= 1.2f * DT; if (aimA < -1) aimA = -1; chargeT = 90; }
        if (kHeld & KEY_R) { aimA += 1.2f * DT; if (aimA > 1) aimA = 1; chargeT = 90; }
        if (kDown & KEY_A) { charging = 1; ph = 0; power = 0; }
    }
    if (charging) {
        ph++; float p = (ph % 44) / 22.0f; power = p < 1 ? p : 2 - p;
        chargeT = 90;
        if (kDown & KEY_B) charging = 0;
        else if (kUp & KEY_A) { charging = 0; if (motionTap) power = motionPower; if (canThrow()) launch(power, aimA); }
    }
    if (chargeT > 0 && !charging) chargeT--;
}

// ── sprites ───────────────────────────────────────────────────────────────
// tiles (4 bits a pixel): 4 markers 32x64, the case 32x64, a chair 32x32, a shadow 32x16;
// palettes 0-3 the markers, 4 the case, 5 the chair, 6 the shadow (see-through black)
enum { T_MM = 512, T_CASE = 640, T_CHAIR = 672, T_SHADOW = 688 };
void miniObjLoad(void) {
    const u8 *M[4] = { mm_green, mm_pink, mm_yellow, mm_blue }; const u16 *P[4] = { mm_green_pal, mm_pink_pal, mm_yellow_pal, mm_blue_pal };
    for (int i = 0; i < 4; i++) { platObjTiles(T_MM + i * 32, M[i], 1024); memcpy(&objPal[i * 16], P[i], 32); }
    platObjTiles(T_CASE, mm_case, 1024); memcpy(&objPal[64], mm_case_pal, 32);
    platObjTiles(T_CHAIR, mm_chair, 512); memcpy(&objPal[80], mm_chair_pal, 32);
    platObjTiles(T_SHADOW, mm_shadow, 256); objPal[96 + 1] = 0;
}
static int nObj;
// an affine sprite: tile/size/shape, centred on (cx, cy), shrunk to sx x sy of its picture, turned by ang
// (dbl: the double-size box, only when a turned picture would spill out of its own box. The GBA
//  can only draw so many sprite pixels per screen line, and the double box costs twice as much.)
static void affD(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, float sx, float sy, float ang, int see, int dbl) {
    if (nObj >= 32 || sx < 0.02f || sy < 0.02f) return;
    int i = nObj++;
    float c = fcos(ang), s = fsin(ang);
    oam[i * 4].a3 = (u16)(s16)(c / sx * 256); oam[i * 4 + 1].a3 = (u16)(s16)(s / sx * 256);
    oam[i * 4 + 2].a3 = (u16)(s16)(-s / sy * 256); oam[i * 4 + 3].a3 = (u16)(s16)(c / sy * 256);
    int bw = dbl ? w : w / 2, bh = dbl ? h : h / 2;
    oam[i].a0 = (u16)(((cy - bh) & 0xFF) | (dbl ? 0x0300 : 0x0100) | (see ? 0x0400 : 0) | (shape << 14));
    oam[i].a1 = (u16)(((cx - bw) & 0x1FF) | (i << 9) | (size << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
static void aff(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, float sx, float sy, float ang, int see) {
    affD(tile, pal, shape, size, w, h, cx, cy, sx, sy, ang, see, ang != 0 || sx > 1 || sy > 1);
}
static void markerAt(float u, float v, float rot, int col) {
    int x, y; float s; proj(u, v, &x, &y, &s);
    float len = MK_LEN * s * 2 * 1.35f, k = len / 48;                     // (the marker picture is 48 tall)
    affD(T_MM + col * 32, col, 2, 3, 32, 64, x, y, k, k, rot, 0, k > 0.62f);   // 32x64 (turned: fits its own box below 0.62)
}
static void shadowAt(float u, float v) {                                   // a soft dark oval just below it
    int x, y; float s; proj(u, v, &x, &y, &s);
    float len = MK_LEN * s * 2 * 1.35f, rx = len * 0.36f, ry = len * 0.13f + 1;
    aff(T_SHADOW, 6, 1, 2, 32, 16, x, y + (int)(len * 0.10f), rx * 2 / 32, ry * 2 / 16, 0, 1);
}

// ── drawing ───────────────────────────────────────────────────────────────
static void drawAimGuide(void) {                             // where this throw would slide
    float p = charging ? (power < 0.06f ? 0.06f : power) : 0.5f;
    float u = aimU, v = 0.02f, du = aimA * 0.52f * (0.5f + p), dv = 0.55f + p * 1.52f;
    int hx, hy, px = 0, py = 0; proj(aimU, 0.02f, &hx, &hy, 0);
    for (int f = 1; f <= 36; f++) {
        u += du * DT; v += dv * DT; du *= DRAG; dv *= DRAG;
        if (f == 10) proj(u, v, &px, &py, 0);
        if (f % 3 || charging) continue;
        int x, y; proj(u, v, &x, &y, 0);
        rect(x - 1, y - 1, 2, 2, WHITE);
    }
    if (charging) {                                          // the power marker, pointing the way it'll go
        float dx = px - hx, dy = py - hy, l = fsqrt(dx * dx + dy * dy); if (l < 1) l = 1;
        powerMarker(hx, hy, dx / l, dy / l, (30 + power * 120) * MINIT_S, p);
    }
}
#define PX0 (TW + 1)                   // the panel's left edge
#define PW (SW - PX0)
void drawMini(void) {
    shY = 0;
    if (shakeT > 0 && screen == S_MINI) shY = (int)((frand() - 0.5f) * shakeAmt * (shakeT / 0.42f) * 0.5f * MINIT_S);
    // the table (a shake jolts it up and down), then the panel: a row at a time, by DMA
    for (int y = 0; y < SH; y++) {
        drawDirect();
        int sy = y - shY; if (sy < 0) sy = 0; if (sy >= MINIT_H) sy = MINIT_H - 1;
        copy32(&page[y * (SW / 2)], &minit8[sy * TW], TW / 4);
        fill32(&page[y * (SW / 2) + TW / 2], C_PANEL * 0x01010101u, (SW - TW) / 4);
    }
    rect(TW, 0, 1, SH, C_PANELLINE);
    clipX0 = 0; clipX1 = TW; clipY0 = 0; clipY1 = SH;
    int ready = canThrow() || (screen == S_MINI_PAUSE && live < 0 && nextColor() >= 0 && turnPause <= 0);
    if (ready && (charging || chargeT > 0)) drawAimGuide();
    clipAll();

    // sprites, front to back: the marker in your hand, the markers (nearest first), what's left
    // to throw (in the panel), the case and chairs, then all the shadows underneath
    hideSprites(); nObj = 0;
    if (ready) markerAt(aimU, 0.02f, 0, nextColor());
    int ordr[4], n = 0;
    for (int i = 0; i < nmk; i++) if (!mk[i].off) ordr[n++] = i;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (mk[ordr[j]].v < mk[ordr[i]].v) { int t = ordr[i]; ordr[i] = ordr[j]; ordr[j] = t; }
    for (int i = 0; i < n; i++) markerAt(mk[ordr[i]].u, mk[ordr[i]].v, mk[ordr[i]].rot, mk[ordr[i]].col);
    int first = idx + (ready ? 1 : 0);
    for (int i = first, k = 0; i < 4; i++, k++) aff(T_MM + order[i] * 32, order[i], 2, 3, 32, 64, PX0 + 14 + k * 16, 140, 0.4f, 0.4f, 0, 0);
    { int x, y; float s; proj(BOXU, BOXV, &x, &y, &s); float bs = BOXW * s * 2; aff(T_CASE, 4, 2, 3, 32, 64, x, y, bs / 32, bs / 32, 0, 0); }
    for (int c = 0; c < chairCount(); c++) { int x, y; float s; proj(CH_U[c], CH_V[c], &x, &y, &s); float cs = CH_W * s * 2;
        aff(T_CHAIR, 5, 0, 2, 32, 32, x, y - (int)(cs / 2), cs / 32, cs / 32, 0, 0); }
    if (ready) shadowAt(aimU, 0.02f);
    for (int i = 0; i < n; i++) shadowAt(mk[ordr[i]].u, mk[ordr[i]].v);

    // the panel: clock, score, best & turn, then the turn's popups and breakdown
    char s[40];
    clipX0 = PX0;
    sprintf(s, "%d.%d", (int)tLeft, (int)(tLeft * 10) % 10);
    textCW(PX0, PW, 0, s, sudden ? RED : YELLOW, 2);
    sprintf(s, "%d PTS", score); textCW(PX0, PW, 30, s, score < 0 ? RED : WHITE, 1);
    if (sudden) textS(PX0 + (PW - textSW("SUDDEN DEATH")) / 2, 47, "SUDDEN DEATH", RED);
    else { sprintf(s, "TURN %d   BEST %d", turn, sv.arcadeBest[ARC_MINI]); textS(PX0 + (PW - textSW(s)) / 2, 47, s, C_LBLUE); }
    rect(PX0 + 4, 59, PW - 8, 1, C_PANELLINE);
    int py = 62;
    for (int i = 0; i < 8 && py < 112; i++) {
        Pop *p = &pops[i]; if (p->life <= 0) continue;
        int jx = p->peef ? (int)((frand() - 0.5f) * 7 * p->life / 70) : 0;
        if (p->big) { textCW(PX0 + jx, PW, py, p->t, p->c, 1); py += 15; }
        else { textS(PX0 + (PW - textSW(p->t)) / 2 + jx, py + 1, p->t, p->c); py += 11; }
    }
    if (msgT > 0) for (int i = 0; i < msgN && py < 116; i++) { textS(PX0 + (PW - textSW(msgL[i])) / 2, py, msgL[i], WHITE); py += 10; }
    if (ready) {
        int blue = nextColor() == MC_BLUE;
        textS(PX0 + 4, 118, blue ? "BLUE - THROW IT LAST" : "NEXT", blue ? C_LBLUE : GREY);
        textS(PX0 + PW - 4 - textSW("hold A"), 150, "hold A", GREY);
    }
    clipAll();
    if (screen == S_MINI_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── the menu before a match, and the one after ────────────────────────────
static int mmSel, moSel;
static Btn MB[4];
static void mmLayout(void) {
    MB[0] = (Btn){ 140, 20, 96, 26, "60 SEC", 0, 0 };
    MB[1] = (Btn){ 140, 52, 96, 26, "90 SEC", 0, 0 };
    MB[2] = (Btn){ 140, 84, 96, 26, "120 SEC", 0, 0 };
    MB[3] = (Btn){ 140, 116, 96, 26, "BACK", 0, 0 };
}
void drawMiniMenu(void) {
    mmLayout();
    fillScreen(DARK);
    text(6, 2, "MINI QU33PH", GOLD, 1);
    static const char *L[10] = { "4 mini markers, 3 tables", "BLUE ALWAYS GOES LAST", "", "All four touching: QU33PH", "On the box: MEGA +30",
        "Off the side costs points;", "blue off early costs most.", "Last fifth: SUDDEN DEATH", "", "left/right: move   1/2: angle" };
    static const int C[10] = { GREY, C_LBLUE, 0, WHITE, WHITE, WHITE, WHITE, RED, 0, GREY };
    for (int i = 0; i < 10; i++) if (L[i][0]) textS(6, 20 + i * 11, L[i], C[i]);
    textS(6, 130, "hold A for power, let go", GREY);
    coinCount(6, 142, 0);
    char s[32];
    if (sv.arcadePlays[ARC_MINI]) { sprintf(s, "BEST %d", sv.arcadeBest[ARC_MINI]); text(234 - textW(s, 1), 144, s, YELLOW, 1); }
    drawBtns(MB, 4, mmSel);
}
void inputMiniMenu(void) {
    mmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(MB, 4, &mmSel, 1);
    if (h == 0) startMini(60);
    if (h == 1) startMini(90);
    if (h == 2) startMini(120);
    if (h == 3) goScreen(S_ARCADE);
}
void drawMiniOver(void) {
    char s[40];
    drawMini();                                              // the final layout stays behind the results
    hideSprites();
    box(20, 16, 200, 128, C_TOAST, GOLD);
    textC(22, "TIME UP", GOLD, 2);
    sprintf(s, "%d POINTS", score); textC(54, s, WHITE, 1);
    sprintf(s, "BEST %d%s", sv.arcadeBest[ARC_MINI], newBest ? "   NEW BEST!" : ""); textC(70, s, newBest ? LIME : GREY, 1);
    sprintf(s, "%d COINS EARNED", coinsWon); textC(86, s, GOLD, 1);
    MB[0] = (Btn){ 28, 108, 100, 26, "AGAIN", 0, 0 }; MB[1] = (Btn){ 134, 108, 78, 26, "ARCADE", 0, 0 };
    drawBtns(MB, 2, moSel);
}
void inputMiniOver(void) {
    int h = btnInput(MB, 2, &moSel, 2);
    if (kDown & KEY_B) h = 1;
    if (h == 0) goScreen(S_MINI_MENU);                       // as on the website: back to pick a length
    if (h == 1) goScreen(S_ARCADE);
}
