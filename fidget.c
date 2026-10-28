// fidget.c — FIDGET QU33PH (the website's fidget.html): spinner air hockey.
//
// The DS's tall field (your half at the bottom) turned on its side to fill the GBA's screen:
// you on the left, the CPU (or player 2) on the right. Knock the ball off the far end to score;
// first to 3, 5 or 7. Physics, CPU, kicks and timings are the DS's (the website's), in its own
// units (the DS's 256 x 384 field); only the drawing is turned. The spinners and the ball are
// hardware sprites.
//
// CONTROLS: the D-pad moves your spinner. In 2 PLAYER, player 2 uses L / R (up / down) and
// B / A (left / right). START pauses (SELECT quits).
#include "qu.h"

#define CW 256.0f
#define CH 384.0f
#define CY 192.0f                                  // the centre line: the gap between the screens
#define UN 256.0f
#define R_BALL (UN * 0.052f)
#define R_SPIN (UN * 0.13f)
enum { ST_PLAY, ST_GOAL, ST_WIN };
typedef struct { float x, y, px, py, vx, vy, spin; int touching; } Spin;
static Spin s1, s2;
static struct { float x, y, vx, vy, rot; } ball;
static int state, goalT, fScore1, fScore2, mode2p, winGoals = 3, c1, menuSel, overSel, coinsWon, youWon;
static char msg[16], winner[16];

static void sfx(int which) {                          // 0 your spinner, 1 theirs, 2 out of the field
    if (!sv.sfxOn) return;
    int r = rand();
    if (which == 0) { const s8 *d[3] = { fs_hitc1, fs_hitc2, fs_hitc3 }; int l[3] = { FS_HITC1_LEN, FS_HITC2_LEN, FS_HITC3_LEN }; sndFx(d[r % 3], l[r % 3]); }
    else if (which == 1) { const s8 *d[3] = { fs_hitf1, fs_hitf2, fs_hitf3 }; int l[3] = { FS_HITF1_LEN, FS_HITF2_LEN, FS_HITF3_LEN }; sndFx(d[r % 3], l[r % 3]); }
    else { if (r & 1) sndFx(fs_oob1, FS_OOB1_LEN); else sndFx(fs_oob2, FS_OOB2_LEN); }
}
static void resetPositions(void) {
    ball.x = CW / 2; ball.y = CY; ball.vx = ball.vy = 0;
    s1 = (Spin){ CW / 2, CH * 0.80f, CW / 2, CH * 0.80f, 0, 0, s1.spin, 0 };
    s2 = (Spin){ CW / 2, CH * 0.20f, CW / 2, CH * 0.20f, 0, 0, s2.spin, 0 };
}
static void startGame(void) { fScore1 = fScore2 = 0; resetPositions(); state = ST_PLAY; msg[0] = 0; screen = S_FIDGET; }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static void place(Spin *sp, float x, float y, int bottom) {
    sp->x = clampf(x, R_SPIN, CW - R_SPIN);
    sp->y = bottom ? clampf(y, CY + R_SPIN * 0.6f, CH - R_SPIN) : clampf(y, R_SPIN, CY - R_SPIN * 0.6f);
}
static void collide(Spin *sp, int close) {
    float dx = ball.x - sp->x, dy = ball.y - sp->y, d = fsqrt(dx * dx + dy * dy), rr = R_BALL + R_SPIN * 0.82f;
    if (d < rr && d > 0.001f) {
        if (!sp->touching) { sp->touching = 1; sfx(close ? 0 : 1); }
        float nx = dx / d, ny = dy / d; ball.x = sp->x + nx * rr; ball.y = sp->y + ny * rr;
        float relv = sp->vx * nx + sp->vy * ny; if (relv < 0) relv = 0;
        float kick = relv + UN * 0.010f;
        ball.vx = nx * kick + sp->vx * 0.6f; ball.vy = ny * kick + sp->vy * 0.6f;
        float s = fsqrt(ball.vx * ball.vx + ball.vy * ball.vy), MX = UN * 0.075f;
        if (s > MX) { ball.vx *= MX / s; ball.vy *= MX / s; }
    } else sp->touching = 0;
}
static void scoreGoal(int who) {
    if (who == 1) { fScore1++; strcpy(msg, "GOAL!"); } else { fScore2++; strcpy(msg, mode2p ? "P2 SCORES" : "CPU SCORES"); }
    if (fScore1 >= winGoals || fScore2 >= winGoals) {
        youWon = fScore1 >= winGoals;
        strcpy(winner, mode2p ? (youWon ? "P1 WINS!" : "P2 WINS!") : (youWon ? "YOU WIN!" : "CPU WINS"));
        state = ST_WIN;
        sv.arcadePlays[ARC_FIDGET]++;
        int before = sv.coins;
        if (!mode2p && youWon) { sv.arcadeBest[ARC_FIDGET]++; addCoins(winGoals); }   // best = wins against the CPU
        coinsWon = sv.coins - before;
        saveWrite();
        screen = S_FIDGET_OVER;
        return;
    }
    state = ST_GOAL; goalT = 70;
}
void updateFidget(void) {
    float m1 = fsqrt(s1.vx * s1.vx + s1.vy * s1.vy), m2 = fsqrt(s2.vx * s2.vx + s2.vy * s2.vy);
    s1.spin += 0.25f + m1 * 0.02f; s2.spin += 0.25f + m2 * 0.02f;
    if (state == ST_GOAL) { if (--goalT <= 0) { resetPositions(); state = ST_PLAY; msg[0] = 0; } return; }
    if (state != ST_PLAY) return;
    s1.vx = s1.x - s1.px; s1.vy = s1.y - s1.py; s1.px = s1.x; s1.py = s1.y;
    if (mode2p) { s2.vx = s2.x - s2.px; s2.vy = s2.y - s2.py; s2.px = s2.x; s2.py = s2.y; }
    else {                                              // the website's CPU
        float tx, ty;
        if (ball.y < CY) { tx = ball.x; ty = ball.y - R_SPIN * 0.7f; } else { tx = CW / 2 + (ball.x - CW / 2) * 0.5f; ty = CH * 0.14f; }
        float adx = tx - s2.x, ady = ty - s2.y, ad = fsqrt(adx * adx + ady * ady); if (ad < 1e-3f) ad = 1;
        float step = UN * 0.0105f < ad ? UN * 0.0105f : ad;
        s2.px = s2.x; s2.py = s2.y; s2.x += adx / ad * step; s2.y += ady / ad * step;
        place(&s2, s2.x, s2.y, 0);
        s2.vx = s2.x - s2.px; s2.vy = s2.y - s2.py;
    }
    ball.x += ball.vx; ball.y += ball.vy; ball.rot += fsqrt(ball.vx * ball.vx + ball.vy * ball.vy) * 0.03f;
    ball.vx *= 0.99f; ball.vy *= 0.99f;
    if (fabsf_(ball.vx) < 0.03f) ball.vx = 0;
    if (fabsf_(ball.vy) < 0.03f) ball.vy = 0;
    if (ball.x < R_BALL) { ball.x = R_BALL; ball.vx = fabsf_(ball.vx) * 0.9f; }
    if (ball.x > CW - R_BALL) { ball.x = CW - R_BALL; ball.vx = -fabsf_(ball.vx) * 0.9f; }
    if (ball.y < R_BALL) { sfx(2); scoreGoal(1); return; }        // off the top end: you score
    if (ball.y > CH - R_BALL) { sfx(2); scoreGoal(2); return; }   // off the bottom end: they score
    collide(&s1, 1); collide(&s2, 0);
}

// ── input (in the screen's directions: the field is on its side) ──────────
// screen right = up the DS's field (smaller y), screen down = the DS's right (bigger x)
void inputFidget(void) {
    if (screen == S_FIDGET_PAUSE) {
        if (kDown & KEY_START) screen = S_FIDGET;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_FIDGET_PAUSE; return; }
    if (state != ST_PLAY && state != ST_GOAL) return;
    float sp = 4.0f, dx = 0, dy = 0;                          // the D-pad: a steady glide
    if (kHeld & KEY_UP) dx -= sp;
    if (kHeld & KEY_DOWN) dx += sp;
    if (kHeld & KEY_LEFT) dy += sp;
    if (kHeld & KEY_RIGHT) dy -= sp;
    if (dx || dy) place(&s1, s1.x + dx, s1.y + dy, 1);
    if (mode2p) {
        float ex = 0, ey = 0;
        if (kHeld & KEY_L) ex -= sp;
        if (kHeld & KEY_R) ex += sp;
        if (kHeld & KEY_B) ey += sp;
        if (kHeld & KEY_A) ey -= sp;
        if (ex || ey) place(&s2, s2.x + ex, s2.y + ey, 0);
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
#define FS 0.625f                                          // GBA pixels per DS pixel
static int sx(float y) { return (int)((CH - y) * FS); }    // the DS's (x, y) on the turned screen
static int sy(float x) { return (int)(x * FS); }
enum { T_SPIN0 = 512, T_SPIN1 = 576, T_FBALL = 640 };
void fidgetObjLoad(void) {
    platObjTiles(T_SPIN0, fd_spin0, 2048); platObjTiles(T_SPIN1, fd_spin1, 2048); platObjTiles(T_FBALL, fd_ball, 512);
    memcpy(&objPal[0], fd_spin0_pal, 32); memcpy(&objPal[16], fd_spin1_pal, 32); memcpy(&objPal[32], fd_ball_pal, 32);
}
static int nObj;
static void spr(int tile, int pal, int size, int half, int cx, int cy, float ang) {   // a turned square sprite, double size
    if (nObj >= 32) return;
    int i = nObj++;
    int c = (int)(fcos(ang) * 256), s = (int)(fsin(ang) * 256);
    oam[i * 4].a3 = (u16)c; oam[i * 4 + 1].a3 = (u16)s; oam[i * 4 + 2].a3 = (u16)-s; oam[i * 4 + 3].a3 = (u16)c;
    oam[i].a0 = (u16)(((cy - half * 2) & 0xFF) | 0x0300);
    oam[i].a1 = (u16)(((cx - half * 2) & 0x1FF) | (i << 9) | (size << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
static int lineCol(int c) { return c ? C_MBLUE : C_MGREEN; }
void drawFidget(void) {
    char s[24];
    drawDirect();
    copy32(page, fd8, SW * SH / 4);                         // the field, then its ends in each player's colour
    rect(0, 0, 3, SH, lineCol(c1));                         // your end (the left)
    rect(SW - 3, 0, 3, SH, lineCol(!c1));                   // theirs
    hideSprites(); nObj = 0;
    spr(T_FBALL, 2, 2, 16, sx(ball.y), sy(ball.x), ball.rot);
    spr(T_SPIN0 + (c1 ? 64 : 0), c1, 3, 32, sx(s1.y), sy(s1.x), s1.spin);
    spr(T_SPIN0 + (c1 ? 0 : 64), !c1, 3, 32, sx(s2.y), sy(s2.x), s2.spin);
    sprintf(s, "%d - %d", fScore1, fScore2); textC(2, s, WHITE, 1);
    text(SW / 2 - 30 - textW(mode2p ? "P1" : "YOU", 1), 2, mode2p ? "P1" : "YOU", C_CYAN, 1);
    text(SW / 2 + 30, 2, mode2p ? "P2" : "CPU", RED, 1);
    if (state == ST_GOAL) textC(64, msg, WHITE, 2);
    if (screen == S_FIDGET_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── the pick screen (the website's: mode, first to, spinner colour) ───────
static Btn FB[6];
static void menuLayout(void) {
    FB[0] = (Btn){ 120, 8, 116, 24, "", 0, 0 }; strcpy(FB[0].label, mode2p ? "2 PLAYER" : "1 PLAYER");
    FB[1] = (Btn){ 120, 36, 116, 24, "", 0, 0 }; sprintf(FB[1].label, "FIRST TO %d", winGoals);
    FB[2] = (Btn){ 120, 64, 116, 24, "PLAY GREEN", C_MGREEN, 0 };
    FB[3] = (Btn){ 120, 92, 116, 24, "PLAY BLUE", C_LBLUE, 0 };
    FB[4] = (Btn){ 120, 120, 116, 24, "BACK", 0, 0 };
}
void drawFidgetMenu(void) {
    char s[40];
    fillScreen(DARK);
    text(4, 2, "FIDGET QU33PH", GOLD, 1);
    textS(4, 20, mode2p ? "P1 - pick your spinner" : "pick your spinner", WHITE);
    static float a; a += 0.03f;
    hideSprites(); nObj = 0;
    spr(T_SPIN0, 0, 3, 32, 32, 66, a); spr(T_SPIN1, 1, 3, 32, 86, 66, -a);
    if (mode2p) { textS(4, 104, "P1: D-pad", GREY); textS(4, 115, "P2: 1/2 up-down,", GREY); textS(4, 126, "    B/A left-right", GREY); }
    else { textS(4, 104, "knock it off the", GREY); textS(4, 115, "far end to score", GREY); }
    sprintf(s, "WINS VS CPU %d", sv.arcadeBest[ARC_FIDGET]); textS(4, 140, s, GREY);
    sprintf(s, "coins %d", sv.coins); textS(4, 150, s, GOLD);
    menuLayout();
    drawBtns(FB, 5, menuSel);
}
void inputFidgetMenu(void) {
    menuLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(FB, 5, &menuSel, 1);
    if (h == 0) mode2p = !mode2p;
    if (h == 1) winGoals = winGoals == 3 ? 5 : winGoals == 5 ? 7 : 3;
    if (h == 2 || h == 3) { c1 = h - 2; startGame(); }
    if (h == 4) goScreen(S_ARCADE);
}
static Btn OB[3];
void drawFidgetOver(void) {
    char s[32];
    drawFidget();
    hideSprites();
    box(20, 16, 200, 128, C_TOAST, GOLD);
    textC(22, winner, C_GOLD, 2);
    sprintf(s, "%d - %d", fScore1, fScore2); textC(54, s, WHITE, 1);
    if (!mode2p) { sprintf(s, "%d COINS EARNED", coinsWon); textC(70, s, GOLD, 1); }
    OB[0] = (Btn){ 30, 88, 180, 24, "PLAY AGAIN", 0, 0 }; OB[1] = (Btn){ 30, 114, 88, 24, "SETUP", 0, 0 }; OB[2] = (Btn){ 122, 114, 88, 24, "ARCADE", 0, 0 };
    drawBtns(OB, 3, overSel);
}
void inputFidgetOver(void) {
    int h = btnInput(OB, 3, &overSel, 1);
    if (kDown & KEY_B) h = 2;
    if (h == 0) startGame();
    if (h == 1) goScreen(S_FIDGET_MENU);
    if (h == 2) goScreen(S_ARCADE);
}
