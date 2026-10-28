// pin.c — QU33PH PINBALL (the website's arcade pinball): the ball is a Qu33ph marker.
//
// The DS version's table art from the MEGA target down to the drain, at 0.58 of the DS's size:
// the whole playfield on one screen, with the score in the panel beside it. Its physics are
// the website's own (a flat logical table drawn in perspective onto the art), as the DS plays
// them: bumpers +100, slings +25, the chair +250 and a ride up to MEGA (+1000). The ball and
// the flippers are hardware sprites; the bumpers' rings and the MEGA target are drawn on.
//
// CONTROLS: L / left / down = left flipper, R / A / B = right flipper. A or up launches.
// START pauses (SELECT quits).
#include "qu.h"
#define PB_IMG_W 382
#define PB_IMG_H 828
#define PB_OX 76
#define PB_OY 284

#define LW 1.0f
#define LH 1.5f
#define G 0.0006f
#define PERSP 0.5f
#define DAMP 0.62f
#define R 0.040f
#define RX (R * 0.62f)
#define RY (R * 1.35f)
static const float QTL[2] = { 0.25f, 0.43f }, QTR[2] = { 0.77f, 0.42f }, QBL[2] = { 0.23f, 0.805f }, QBR[2] = { 0.82f, 0.805f };
typedef struct { float x, y, r; int hit, sling; } Bump;
static Bump bumps[6] = { {0.325f,0,0.105f,0,0}, {0.485f,-0.10f,0.105f,0,0}, {0.64f,0.03f,0.105f,0,0}, {0.485f,0.12f,0.105f,0,0}, {0.22f,0.92f,0.09f,0,1}, {0.71f,0.92f,0.09f,0,1} };
// the flippers: an even, matched pair. The website's sit at slightly different heights and
// spacings (0.22,1.02 and 0.70,1.05); these share one height and mirror each other about the
// middle of the same 0.13-wide drain gap.
#define FL_Y 1.035f
#define FL_LX 0.215f
#define FL_RX 0.705f
static const float CH_X = 0.14f, CH_Y = 0.55f, CH_W = 0.22f, CH_H = 0.09f, MG_X = 0.70f, MG_Y = -0.17f, MG_W = 0.275f, MG_H = 0.16f;
static const float TRIS[2][7][2] = {
    { {0.000f,0.936f},{0.033f,0.789f},{0.092f,0.780f},{0.205f,0.957f},{0.198f,1.035f},{0.125f,1.041f},{0.003f,0.948f} },
    { {0.915f,0.891f},{0.870f,1.059f},{0.785f,1.074f},{0.715f,1.038f},{0.792f,0.822f},{0.877f,0.798f},{0.912f,0.876f} } };
static struct { float x, y, vx, vy, spin; int launched, toMega, inChute, color, upT, megaT, chkT; float chkY; } b;
enum { ST_READY, ST_PLAY, ST_OVER };
static int state, score, balls, seq, megaFlash, chairCD, msgT, coinsWon, newBest, overSel, menuSel, tl, tr;
static float fl, fr; static char msg[16];

// ── the website's perspective: the flat table onto the tilted quad of the art ─────
static float vp(float y) { float v = y / LH; if (v < -0.13f) v = -0.13f; if (v > 1.15f) v = 1.15f; return v / (1 + PERSP * (1 - v)); }
static void proj(float x, float y, float *sx, float *sy) {
    float u = x / LW, v = vp(y);
    float tx = QTL[0] + (QTR[0] - QTL[0]) * u, ty = QTL[1] + (QTR[1] - QTL[1]) * u;
    float bx = QBL[0] + (QBR[0] - QBL[0]) * u, by = QBL[1] + (QBR[1] - QBL[1]) * u;
    *sx = (tx + (bx - tx) * v) * PB_IMG_W - PB_OX; *sy = (ty + (by - ty) * v) * PB_IMG_H - PB_OY;
}
static float sizePx(float l, float y) { return l * PB_IMG_W * 0.52f * (0.45f + vp(y) * 1.15f); }

static void newBall(void) { memset(&b, 0, sizeof b); b.x = 1.10f; b.y = 1.14f; b.inChute = 1; b.color = seq++ % 2; b.chkY = -1; state = ST_READY; }
static void resetGame(void) { score = 0; balls = 3; seq = 0; msgT = 0; newBall(); screen = S_PIN; }
static void launch(void) { if (state == ST_READY && !b.launched) { b.vy = -0.055f; b.vx = -0.015f; b.launched = 1; state = ST_PLAY; } }
static void say(const char *t, int frames) { strcpy(msg, t); msgT = frames; }
static void endGame(void) {
    state = ST_OVER; say("GAME OVER", 999);
    newBest = score > sv.arcadeBest[ARC_PINBALL]; if (newBest) sv.arcadeBest[ARC_PINBALL] = score;
    sv.arcadePlays[ARC_PINBALL]++;
    int c = score / 100; if (c > 25) c = 25;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite(); screen = S_PIN_OVER;
}
static void flipper(float px, float py, float ang, float len, float active) {
    float tx = px + fcos(ang) * len, ty = py + fsin(ang) * len, dx = tx - px, dy = ty - py, L2 = dx * dx + dy * dy;
    float t = ((b.x - px) * dx + (b.y - py) * dy) / L2; t = t < 0 ? 0 : t > 1 ? 1 : t;
    float cx = px + dx * t, cy = py + dy * t, ox = b.x - cx, oy = b.y - cy, d = fsqrt(ox * ox + oy * oy); if (d < 1e-6f) d = 1e-6f;
    float thick = R + 0.022f;
    if (d < thick) {
        float nx = ox / d, ny = oy / d; b.x = cx + nx * thick; b.y = cy + ny * thick;
        float dot = b.vx * nx + b.vy * ny;
        if (dot < 0) { float m = active > 0.35f ? 1.8f : 0.5f; b.vx -= m * dot * nx; b.vy -= m * dot * ny; }
        b.vy -= active * 0.05f;
        if (active > 0.4f) sfxPlop();
    }
}
static int inPoly(float px, float py, const float (*p)[2], int n) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) { float xi = p[i][0], yi = p[i][1], xj = p[j][0], yj = p[j][1];
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / ((yj - yi) != 0 ? (yj - yi) : 1e-9f) + xi)) in = !in; }
    return in;
}
static void triCollide(void) {
    for (int t = 0; t < 2; t++) {
        const float (*p)[2] = TRIS[t];
        if (!inPoly(b.x, b.y, p, 7)) continue;
        float best = 1e9f, bcx = 0, bcy = 0;
        for (int i = 0, j = 6; i < 7; j = i++) {
            float ax = p[j][0], ay = p[j][1], dx = p[i][0] - ax, dy = p[i][1] - ay, L2 = dx * dx + dy * dy;
            float u = ((b.x - ax) * dx + (b.y - ay) * dy) / L2; u = u < 0 ? 0 : u > 1 ? 1 : u;
            float qx = ax + dx * u, qy = ay + dy * u, d = fsqrt((b.x - qx) * (b.x - qx) + (b.y - qy) * (b.y - qy));
            if (d < best) { best = d; bcx = qx; bcy = qy; }
        }
        float nx = b.x - bcx, ny = b.y - bcy, nl = fsqrt(nx * nx + ny * ny);
        if (nl < 1e-6f) { nx = b.x < 0.5f ? -1 : 1; ny = 0.3f; nl = fsqrt(nx * nx + ny * ny); }
        nx = -nx / nl; ny = -ny / nl;
        b.x = bcx + nx * R * 1.9f; b.y = bcy + ny * R * 1.9f;
        float dot = b.vx * nx + b.vy * ny; if (dot < 0) { b.vx -= 1.7f * dot * nx; b.vy -= 1.7f * dot * ny; }
        float out = b.vx * nx + b.vy * ny; if (out < 0.010f) { b.vx += nx * (0.010f - out); b.vy += ny * (0.010f - out); }
        b.vy += 0.004f; return;
    }
}
static void pinStep(void);
static int pinAcc;
void updatePin(void) {                                 // 4 physics steps every 3 frames: a livelier table
    pinStep(); if (state == ST_PLAY && ++pinAcc >= 3) { pinAcc = 0; pinStep(); }
}
static void pinStep(void) {
    fl += (tl - fl) * 0.5f; fr += (tr - fr) * 0.5f;
    if (msgT > 0 && msgT < 900) msgT--;
    if (megaFlash > 0) megaFlash--;
    for (int j = 0; j < 6; j++) if (bumps[j].hit > 0) bumps[j].hit--;
    if (state != ST_PLAY) return;
    b.vy += G; b.x += b.vx; b.y += b.vy; b.vx *= 0.99f; b.vy *= 0.996f;
    float mv = b.inChute ? 0.05f : 0.042f;
    if (b.vx > mv) b.vx = mv; if (b.vx < -mv) b.vx = -mv; if (b.vy > mv) b.vy = mv; if (b.vy < -mv) b.vy = -mv;
    b.spin += (fabsf_(b.vx) + fabsf_(b.vy)) * 2.4f;
    if (b.inChute) {
        if (b.x < 0.98f + RX) { b.x = 0.98f + RX; if (b.vx < 0) b.vx *= -0.3f; }
        if (b.x > 1.14f - RX) { b.x = 1.14f - RX; b.vx = -b.vx * DAMP; }
        if (b.y < 0.42f) { b.inChute = 0; b.x = 0.9f; b.vx = -0.02f; }
    } else {
        float lw, rw;
        if (b.y < 0.6f) { lw = 0.04f; rw = 0.94f; }
        else { float tf = (b.y - 0.6f) / 0.58f; tf = tf < 0 ? 0 : tf > 1 ? 1 : tf; float e = tf * tf * (3 - 2 * tf); lw = 0.04f + e * 0.15f; rw = 0.94f - e * 0.23f; }
        if (b.x < lw + RX) { b.x = lw + RX; if (b.vx < 0) b.vx = -b.vx * DAMP; }
        if (b.x > rw - RX) { b.x = rw - RX; if (b.vx > 0) b.vx = -b.vx * DAMP; }
    }
    if (b.y < 0.02f + RY) { b.y = 0.02f + RY; b.vy = -b.vy * DAMP; }
    if (chairCD > 0) chairCD--;
    if (b.toMega) { b.vx += ((MG_X + MG_W / 2) - b.x) * 0.006f; b.upT = 0; b.megaT++;
        if (b.megaT > 70 || (b.y > 0.42f && b.vy > 0.006f)) { b.toMega = 0; b.megaT = 0; } }
    if (b.y < 0.96f && !b.toMega) b.upT++; else b.upT = 0;
    int forceDown = b.upT > 135;                       // the website's anti-trap
    if (forceDown) { b.vy += 0.0035f; b.vx += (0.5f - b.x) * 0.0018f; b.vx *= 0.97f; }
    float spd = fabsf_(b.vx) + fabsf_(b.vy);
    if (!b.toMega && !b.inChute && b.y > 0.24f && b.y < 0.99f && spd < 0.010f) { b.vy += 0.0028f; b.vx += (0.5f - b.x) * 0.0007f; }
    if (!b.toMega && !b.inChute && b.y > 0.84f && (b.x < 0.34f || b.x > 0.62f)) b.vx += (0.5f - b.x) * 0.009f;
    if (++b.chkT >= 16) { if (!b.toMega && !b.inChute && b.y > 0.24f && b.y < 0.94f && (b.y - (b.chkY < 0 ? b.y : b.chkY)) < 0.02f) { b.vx *= 0.4f; if (b.vy < 0.016f) b.vy = 0.016f; b.vx += (0.5f - b.x) * 0.002f; } b.chkY = b.y; b.chkT = 0; }
    for (int i = 0; i < 6 && !forceDown && !b.toMega; i++) { Bump *p = &bumps[i];
        float qx = b.x - RX > p->x ? b.x - RX : b.x + RX < p->x ? b.x + RX : p->x, qy = b.y - RY > p->y ? b.y - RY : b.y + RY < p->y ? b.y + RY : p->y;
        float dx = qx - p->x, dy = qy - p->y, d = fsqrt(dx * dx + dy * dy); if (d < 1e-6f) d = 1;
        if (d < p->r) { float nx = dx / d, ny = dy / d, push = p->r - d; b.x += nx * push; b.y += ny * push;
            if (p->sling) { b.vx = (0.5f - p->x) * 0.05f; b.vy = 0.03f; }
            else { float dot = b.vx * nx + b.vy * ny; b.vx -= 2 * dot * nx; b.vy -= 2 * dot * ny; b.vx = b.vx * 0.5f + nx * 0.006f; b.vy = b.vy * 0.5f + ny * 0.006f; }
            p->hit = 8; score += p->sling ? 25 : 100; } }
    if (b.x > CH_X - RX && b.x < CH_X + CH_W + RX && b.y > CH_Y - RY && b.y < CH_Y + CH_H + RY && b.vy > -0.01f && chairCD <= 0) {
        chairCD = 22; b.y = CH_Y - RY; b.vx = 0.026f; b.vy = -0.078f; b.toMega = 1; b.megaT = 0; score += 250; say("TO MEGA!", 45); }
    if (b.toMega && b.y < 0.11f && b.x > MG_X - 0.05f && b.x < MG_X + MG_W + 0.05f) {
        score += 1000; megaFlash = 50; say("MEGA! +1000", 70); b.toMega = 0; b.megaT = 0; b.vy = 0.014f; b.vx = (0.5f - b.x) * 0.02f; sfxPlop(); }
    triCollide();
    flipper(FL_LX, FL_Y, 0.5f - fl, 0.20f, fl);
    flipper(FL_RX, FL_Y, (3.14159265f - 0.5f) + fr, 0.20f, fr);
    if (b.y > 1.18f) { if (--balls <= 0) endGame(); else newBall(); }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputPin(void) {
    if (screen == S_PIN_PAUSE) {
        if (kDown & KEY_START) screen = S_PIN;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_PIN_PAUSE; return; }
    if (state == ST_READY && (kDown & (KEY_A | KEY_UP))) { launch(); return; }
    int nl = (kHeld & (KEY_L | KEY_LEFT | KEY_DOWN)) != 0, nr = (kHeld & (KEY_R | KEY_A | KEY_B)) != 0;
    if (((nl && !tl) || (nr && !tr)) && sv.sfxOn) sndFx(pb_snd, PB_SND_LEN);
    tl = nl; tr = nr;
}

// ── drawing: the DS's picture (256 x 384), from its row 25 down, at 0.58 ──
#define PX0 (PBT_W + 1)
#define PW (SW - PX0)
static int px_(float x) { return (int)(x * PBT_S); }
static int py_(float y) { return (int)((y - PBT_Y0) * PBT_S); }
// tiles: the two balls (16x32), the flipper and its mirror image (32x16); palettes 0-3
enum { T_PB = 512, T_PFL = 528 };
void pinObjLoad(void) {
    platObjTiles(T_PB, pb_ball0, 256); platObjTiles(T_PB + 8, pb_ball1, 256);
    platObjTiles(T_PFL, pb_fl, 256); platObjTiles(T_PFL + 8, pb_flm, 256);
    memcpy(&objPal[0], pb_ball0_pal, 32); memcpy(&objPal[16], pb_ball1_pal, 32);
    memcpy(&objPal[32], pb_fl_pal, 32); memcpy(&objPal[48], pb_flm_pal, 32);
}
static int nObj;
static void aff(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, float k, float ang) {   // double size
    if (nObj >= 32 || k < 0.02f) return;
    int i = nObj++;
    float c = fcos(ang) / k, s = fsin(ang) / k;
    oam[i * 4].a3 = (u16)(s16)(c * 256); oam[i * 4 + 1].a3 = (u16)(s16)(s * 256);
    oam[i * 4 + 2].a3 = (u16)(s16)(-s * 256); oam[i * 4 + 3].a3 = (u16)(s16)(c * 256);
    oam[i].a0 = (u16)(((cy - h) & 0xFF) | 0x0300 | (shape << 14));
    oam[i].a1 = (u16)(((cx - w) & 0x1FF) | (i << 9) | (size << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
static void ring(float cx, float cy, float rx, float ry, int c, int thick) {
    for (int a = 0; a < 48; a++) { float t = a * 6.2831853f / 48; int x = px_(cx + rx * fcos(t)), y = py_(cy + ry * fsin(t));
        for (int k = 0; k < thick; k++) pset(x, y + k, c); }
}
static void drawFlipper(float fx, float fy, float ang, float len, int mirror) {
    float ax, ay, bx, by; proj(fx, fy, &ax, &ay); proj(fx + fcos(ang) * len, fy + fsin(ang) * len, &bx, &by);
    float dx = bx - ax, dy = by - ay, L = fsqrt(dx * dx + dy * dy) * 1.08f, a = fatan2r(dy, dx);
    if (mirror) a -= 3.14159265f;          // the right flipper: the left one's reflection, so the pair matches
    aff(T_PFL + mirror * 8, 2 + mirror, 1, 2, 32, 16, px_((ax + bx) / 2), py_((ay + by) / 2), L * PBT_S / 32, a);
}
void drawPin(void) {
    char s[32];
    drawDirect();
    for (int y = 0; y < SH; y++) {
        copy32(&page[y * (SW / 2)], &pb8[y * PBT_W], PBT_W / 4);
        fill32(&page[y * (SW / 2) + PBT_W / 2], C_PANEL * 0x01010101u, (SW - PBT_W) / 4);
    }
    rect(PBT_W, 0, 1, SH, C_PANELLINE);
    clipX0 = 0; clipX1 = PBT_W; clipY0 = 0; clipY1 = SH;
    for (int i = 0; i < 6; i++) { Bump *p = &bumps[i]; if (p->sling) continue;
        float x, y; proj(p->x, p->y, &x, &y); float rx = sizePx(p->r, p->y), ry = rx * 0.52f;
        int c = i % 2 ? C_MRED : C_CYAN; if (p->hit > 0) c = WHITE;
        ring(x, y, rx, ry, c, p->hit > 0 ? 2 : 1); }
    {   // the MEGA target
        float ax, ay, bx, by, cx, cy, dx, dy; proj(MG_X, MG_Y, &ax, &ay); proj(MG_X + MG_W, MG_Y, &bx, &by); proj(MG_X + MG_W, MG_Y + MG_H, &cx, &cy); proj(MG_X, MG_Y + MG_H, &dx, &dy);
        int c = megaFlash > 0 ? YELLOW : GOLD;
        for (int k = 0; k <= 16; k++) { float t = k / 16.0f;
            pset(px_(ax + (bx - ax) * t), py_(ay + (by - ay) * t), c); pset(px_(dx + (cx - dx) * t), py_(dy + (cy - dy) * t), c);
            pset(px_(ax + (dx - ax) * t), py_(ay + (dy - ay) * t), c); pset(px_(bx + (cx - bx) * t), py_(by + (cy - by) * t), c); }
        if (megaFlash > 0) stipple(px_(ax), py_(ay), px_(bx) - px_(ax), py_(dy) - py_(ay), GOLD);
        textS(px_((ax + bx) / 2) - textSW("MEGA") / 2, py_(ay) - 11, "MEGA", GOLD);
    }
    hideSprites(); nObj = 0;
    if (state != ST_OVER) { float x, y; proj(b.x, b.y, &x, &y);                // the ball, on top
        aff(T_PB + b.color * 8, b.color, 2, 2, 16, 32, px_(x), py_(y), sizePx(RY * 2, b.y) * PBT_S / 22, b.spin); }
    drawFlipper(FL_LX, FL_Y, 0.5f - fl, 0.20f, 0);
    drawFlipper(FL_RX, FL_Y, (3.14159265f - 0.5f) + fr, 0.20f, 1);
    if (msgT > 0 && state != ST_OVER) textCW(0, PBT_W, 70, msg, GOLD, 1);
    clipAll();
    // the panel
    clipX0 = PX0;
    textS(PX0 + 4, 4, "SCORE", GREY); sprintf(s, "%d", score); text(PX0 + 4, 14, s, WHITE, 1);
    textS(PX0 + 4, 36, "BALLS", GREY);
    for (int k = 0; k < balls; k++) { rect(PX0 + 6 + k * 10, 48, 4, 10, k % 2 ? C_MGREEN : C_MRED); }
    textS(PX0 + 4, 66, "HIGH", GREY); sprintf(s, "%d", sv.arcadeBest[ARC_PINBALL]); textS(PX0 + 4, 76, s, C_LBLUE);
    if (state == ST_READY) { textS(PX0 + 4, 100, "A: LAUNCH", YELLOW); }
    textS(PX0 + 4, 118, "1: left flip", GREY); textS(PX0 + 4, 129, "2/A: right flip", GREY);
    sprintf(s, "coins %d", sv.coins); textS(PX0 + 4, 148, s, GOLD);
    clipAll();
    if (screen == S_PIN_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}
static Btn PBB[2];
static void pmLayout(void) { PBB[0] = (Btn){ 136, 40, 100, 30, "PLAY", 0, 0 }; PBB[1] = (Btn){ 136, 80, 100, 26, "BACK", 0, 0 }; }
void drawPinMenu(void) {
    char s[32];
    fillScreen(DARK);
    text(6, 2, "QU33PH PINBALL", GOLD, 1);
    static const char *L[6] = { "The ball is a marker.", "3 balls. bumpers +100,", "slings +25. The chair +250", "sends you to MEGA: +1000",
        "1 / left: left flipper", "2 / A: right flipper" };
    static const int C[6] = { WHITE, WHITE, WHITE, C_GOLD, GREY, GREY };
    for (int i = 0; i < 6; i++) textS(6, 22 + i * 12, L[i], C[i]);
    sprintf(s, "HIGH %d", sv.arcadeBest[ARC_PINBALL]); textS(6, 104, s, C_LBLUE);
    coinCount(6, 136, 0);
    pmLayout(); drawBtns(PBB, 2, menuSel);
}
void inputPinMenu(void) {
    pmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(PBB, 2, &menuSel, 1);
    if (h == 0) resetGame();
    if (h == 1) goScreen(S_ARCADE);
}
void drawPinOver(void) {
    char s[32];
    drawPin();
    hideSprites();
    box(20, 16, 200, 128, C_TOAST, GOLD);
    textC(22, "GAME OVER", GOLD, 2);
    sprintf(s, "SCORE %d", score); textC(54, s, WHITE, 1);
    if (newBest) sprintf(s, "NEW HIGH!  +%d coins", coinsWon); else sprintf(s, "HIGH %d   +%d coins", sv.arcadeBest[ARC_PINBALL], coinsWon);
    textC(70, s, newBest ? LIME : C_LBLUE, 1);
    PBB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; PBB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    drawBtns(PBB, 2, overSel);
}
void inputPinOver(void) {
    PBB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; PBB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    int h = btnInput(PBB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) resetGame();
    if (h == 1) goScreen(S_ARCADE);
}
