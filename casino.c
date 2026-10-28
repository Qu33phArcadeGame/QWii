// casino.c — the slot machine and PlinQu33ph (the DS version's rules and odds, laid out for one screen)
#include "qu.h"

// ══ SLOT MACHINE ══════════════════════════════════════════════════════════
enum { Y_LOGO, Y_MEGA, Y_COIN, Y_CHAIR, Y_RED, Y_GREEN, Y_BLUE };
static const int WEIGHT[7] = { 1, 2, 4, 5, 8, 8, 8 };
typedef struct { signed char s[3]; int win; const char *label; } Pay;   // -1 = any symbol
static const Pay PAYS[] = {
    { { Y_LOGO, Y_LOGO, Y_LOGO }, 200, "JACKPOT" }, { { Y_MEGA, Y_MEGA, Y_MEGA }, 50, "MEGA QU33PH" },
    { { Y_COIN, Y_COIN, Y_COIN }, 30, "TRIPLE COIN" }, { { Y_CHAIR, Y_CHAIR, Y_CHAIR }, 20, "CHAIR SPIN" },
    { { Y_LOGO, Y_LOGO, -1 }, 15, "DOUBLE LOGO" }, { { Y_MEGA, Y_MEGA, -1 }, 8, "DOUBLE MEGA" },
    { { Y_CHAIR, Y_CHAIR, -1 }, 6, "DOUBLE CHAIR" }, { { Y_COIN, Y_COIN, -1 }, 5, "DOUBLE COIN" },
    { { Y_RED, Y_RED, Y_RED }, 4, "RED TRIPLE" }, { { Y_GREEN, Y_GREEN, Y_GREEN }, 4, "GREEN TRIPLE" },
    { { Y_BLUE, Y_BLUE, Y_BLUE }, 4, "BLUE TRIPLE" }, { { -1, Y_LOGO, -1 }, 3, "ANY LOGO" },
    { { Y_RED, Y_RED, -1 }, 2, "DOUBLE RED" }, { { Y_GREEN, Y_GREEN, -1 }, 2, "DOUBLE GREEN" },
    { { Y_BLUE, Y_BLUE, -1 }, 2, "DOUBLE BLUE" }, { { -1, Y_COIN, -1 }, 2, "ANY COIN" }, { { -1, Y_CHAIR, -1 }, 1, "ANY CHAIR" } };
static int reel[3] = { Y_LOGO, Y_MEGA, Y_COIN }, result[3], spinT, spinBet, lastWin; static const char *lastLabel = "";
static int slotSel, slotPayT;
static int pickSym(void) { int t = 0; for (int i = 0; i < 7; i++) t += WEIGHT[i]; int r = rand() % t; for (int i = 0; i < 7; i++) { r -= WEIGHT[i]; if (r < 0) return i; } return Y_BLUE; }
static int checkWin(const int *r, const char **label) {
    int isM[3]; for (int i = 0; i < 3; i++) isM[i] = r[i] >= Y_RED;
    if (isM[0] && isM[1] && isM[2] && r[0] != r[1] && r[1] != r[2] && r[0] != r[2]) {
        if (r[2] == Y_BLUE) { *label = "TRIO - BLUE 3RD"; return 10; }
        *label = "TRIO - ANY ORDER"; return 6;
    }
    for (unsigned k = 0; k < sizeof PAYS / sizeof PAYS[0]; k++) {
        int ok = 1; for (int i = 0; i < 3; i++) if (PAYS[k].s[i] >= 0 && PAYS[k].s[i] != r[i]) ok = 0;
        if (ok) { *label = PAYS[k].label; return PAYS[k].win; }
    }
    *label = ""; return 0;
}
static void spin(int bet) {
    if (spinT > 0) return;
    if (sv.coins < bet) { lastLabel = "not enough coins"; lastWin = 0; return; }
    spendCoins(bet); spinBet = bet; spinT = 13; lastWin = 0; lastLabel = "";
    for (int i = 0; i < 3; i++) result[i] = pickSym();
    sv.slotSpins++;
}
void updateSlot(void) {
    if (spinT <= 0) return;
    spinT--;
    for (int i = 0; i < 3; i++) { int stopAt = 9 - i * 4; if (spinT > stopAt) reel[i] = rand() % 7; else reel[i] = result[i]; }
    if (spinT == 0) {
        int w = checkWin(result, &lastLabel);
        if (w) { lastWin = w * spinBet; addCoins(lastWin); sv.slotWins++; sfxPlop(); }
        else sv.slotLost += spinBet;
        saveWrite();
    }
}
static Btn slotB[4];
static void slotLayout(void) {
    slotB[0] = (Btn){ 174, 26, 64, 24, "SPIN 1", 0, sv.coins < 1 };
    slotB[1] = (Btn){ 174, 54, 64, 24, "SPIN 3", 0, sv.coins < 3 };
    slotB[2] = (Btn){ 174, 82, 64, 24, "", 0, 0 }; strcpy(slotB[2].label, slotPayT ? "HIDE" : "PAYS");
    slotB[3] = (Btn){ 174, 110, 64, 24, "BACK", 0, 0 };
}
void drawSlot(void) {
    fillScreen(DARK);
    // the website's slot machine, its reels turning in the three windows
    drawImg(mach8, MACH_W, MACH_H, 0, 0);
    static const int WX[3] = { REEL_X0, REEL_X1, REEL_X2 };
    for (int i = 0; i < 3; i++) drawImg(sym8[reel[i]], SYM_W, SYM_W, WX[i] - SYM_W / 2, REEL_Y - SYM_W / 2);
    if (slotPayT) {                                        // the paytable, over the machine
        box(2, 2, MACH_W - 4, SH - 4, C_TOAST, GOLD);
        textS(8, 6, "QU33PH SLOTS - pays x your bet", GOLD);
        static const char *rows[11] = { "LOGO LOGO LOGO  200", "MEGA x3  50    COIN x3  30", "CHAIR x3  20   2 LOGOS  15",
            "RED GREEN BLUE (blue 3rd)  10", "any other trio  6", "2 MEGA  8   2 CHAIR  6", "2 COIN  5", "one colour x3  4",
            "logo in the middle  3", "colour pair  2   coin middle  2", "chair in the middle  1" };
        for (int i = 0; i < 11; i++) textS(8, 20 + i * 12, rows[i], i == 0 ? YELLOW : WHITE);
    } else if (lastWin || lastLabel[0]) {                  // the result, across the machine's tray
        char s[40];
        if (lastWin) sprintf(s, "%s  +%d", lastLabel, lastWin); else strcpy(s, lastLabel);
        textCW(0, MACH_W, 140, s, lastWin ? LIME : WHITE, 1);
    }
    coinCount(176, 4, 1);
    slotLayout();
    drawBtns(slotB, 4, slotSel);
    textS(176, 140, "1 or 3 a", GREY); textS(176, 150, "spin", GREY);
}
void inputSlot(void) {
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    slotLayout();
    int h = btnInput(slotB, 4, &slotSel, 1);
    if (h == 0) spin(1);
    if (h == 1) spin(3);
    if (h == 2) slotPayT = !slotPayT;
    if (h == 3) goScreen(S_TITLE);
}

// ══ PLINQU33PH ════════════════════════════════════════════════════════════
// The board keeps the DS's measurements (256 x 192) so the markers bounce exactly as they did,
// and is drawn at 4/5 size; the panel on the right has your coins and this set's winnings.
static const int BINS[7] = { 1, 2, 5, 10, 5, 2, 1 };
#define PEG_ROWS 7
#define PS 0.8f                     // GBA pixels per board unit
#define PX0 2
#define PY0 2
#define PANEL 209
typedef struct { float x, y, vx, vy, rot, spin; int live, done, bin; } Ball;
static int plBonusT; static char plBonusMsg[32];
static Ball balls[3]; static int dropIdx, paid, aimX = 128, plWin;
static int pegX(int r, int c) { return 18 + c * 32 + (r % 2 ? 16 : 0); }
static int pegY(int r) { return 34 + r * 18; }
static int pegCount(int r) { return r % 2 ? 7 : 8; }
static int bx(float x) { return PX0 + (int)(x * PS); }
static int by(float y) { return PY0 + (int)(y * PS); }
void plinkoEnter(void) { dropIdx = 0; paid = 0; plWin = 0; memset(balls, 0, sizeof balls); }
static void drop(void) {
    if (dropIdx >= 3) {                                                // set done: start a new one, once all have landed
        for (int i = 0; i < 3; i++) if (balls[i].live) return;
        plinkoEnter(); return;
    }
    if (!paid) { if (sv.coins < 5) { strcpy(plBonusMsg, "need 5 coins"); plBonusT = 90; return; } spendCoins(5); paid = 1; }
    Ball *b = &balls[dropIdx++];
    b->x = aimX; b->y = 10; b->vx = (frand() - 0.5f) * 0.6f; b->vy = 0; b->live = 1; b->done = 0;
    b->rot = 1.5708f; b->spin = (frand() - 0.5f) * 0.1f;
}
void updatePlinko(void) {
    for (int i = 0; i < 3; i++) {
        Ball *b = &balls[i]; if (!b->live) continue;
        b->vy += 0.16f; b->x += b->vx; b->y += b->vy; b->vx *= 0.995f;
        b->rot += b->spin; b->spin *= 0.985f;
        // only the peg rows the marker can touch, and only the two pegs either side of it
        int rn = (int)((b->y - 34 + 9) / 18);
        for (int r = rn - 1; r <= rn; r++) { if (r < 0 || r >= PEG_ROWS) continue;
            int pdy = (int)b->y - pegY(r); if (pdy > 9 || pdy < -9) continue;
            int c0 = (int)((b->x - 18 - (r % 2 ? 16 : 0)) / 32);
            for (int c = c0; c <= c0 + 1; c++) { if (c < 0 || c >= pegCount(r)) continue;
            float dx = b->x - pegX(r, c), dy = b->y - pegY(r), d2 = dx * dx + dy * dy;
            if (d2 < 81.0f && d2 > 0.01f) {                               // ball r 6 + peg r 3
                float d = fsqrt(d2), nx = dx / d, ny = dy / d, rel = b->vx * nx + b->vy * ny;
                b->x = pegX(r, c) + nx * 9; b->y = pegY(r) + ny * 9;
                if (rel < 0) { b->vx -= 1.5f * rel * nx; b->vy -= 1.5f * rel * ny; }
                b->vx += (frand() - 0.5f) * 0.5f;
                b->spin += (b->vx * ny - b->vy * nx) * 0.06f;             // the marker tumbles the way it was knocked
                if (b->spin > 0.4f) b->spin = 0.4f;
                if (b->spin < -0.4f) b->spin = -0.4f;
            }
        } }
        if (b->x < 6) { b->x = 6; b->vx = -b->vx * 0.5f; }
        if (b->x > 250) { b->x = 250; b->vx = -b->vx * 0.5f; }
        if (b->y > 172) {
            int bin = (int)(b->x * 7 / 256); if (bin < 0) bin = 0; if (bin > 6) bin = 6;
            // markers landing in the same hole pile up and touch: a bonus once the set's done
            int under = 0; for (int k = 0; k < 3; k++) if (k != i && balls[k].done && balls[k].bin == bin) under++;
            b->live = 0; b->done = 1; b->bin = bin; b->y = 178 - under * 7;
            addCoins(BINS[bin]); plWin += BINS[bin]; sfxPlop();
            if (balls[0].done && balls[1].done && balls[2].done) {
                int pairs = (balls[0].bin == balls[1].bin) + (balls[0].bin == balls[2].bin) + (balls[1].bin == balls[2].bin);
                int bonus = pairs >= 3 ? 25 : pairs >= 1 ? 10 : 0;
                if (bonus) { addCoins(bonus); plWin += bonus; strcpy(plBonusMsg, pairs >= 3 ? "ALL 3 TOUCH +25" : "TOUCH +10"); plBonusT = 150; sfxPlop(); }
                saveWrite();
            }
        }
    }
}
// PlinQu33ph's markers are hardware sprites (red, green 32x32; blue 64x32), turned and shrunk
// to the board's size. Tiles after the match's sprites; palettes 5-7.
enum { T_PRED = 960, T_PGREEN = 976, T_PBLUE = 992 };
void casinoInit(void) {
    platObjTiles(T_PRED, pm_red, sizeof pm_red); platObjTiles(T_PGREEN, pm_green, sizeof pm_green); platObjTiles(T_PBLUE, pm_blue, sizeof pm_blue);
    memcpy(&objPal[80], pm_red_pal, 32); memcpy(&objPal[96], pm_green_pal, 32); memcpy(&objPal[112], pm_blue_pal, 32);
}
static int nObj;
static void pmSprite(int k, int cx, int cy, float ang) {
    int i = nObj++;
    int c = (int)(fcos(ang) * 256 / PS), s = (int)(fsin(ang) * 256 / PS);
    oam[i * 4].a3 = (u16)c; oam[i * 4 + 1].a3 = (u16)s; oam[i * 4 + 2].a3 = (u16)-s; oam[i * 4 + 3].a3 = (u16)c;
    if (k == 2) {          // blue: 64x32, wide, double size -> 128x64
        oam[i].a0 = (u16)(((cy - 32) & 0xFF) | 0x0300 | 0x4000);
        oam[i].a1 = (u16)(((cx - 64) & 0x1FF) | (i << 9) | 0xC000);
        oam[i].a2 = (u16)(T_PBLUE | (7 << 12));
    } else {               // 32x32, double size -> 64x64
        oam[i].a0 = (u16)(((cy - 32) & 0xFF) | 0x0300);
        oam[i].a1 = (u16)(((cx - 32) & 0x1FF) | (i << 9) | 0x8000);
        oam[i].a2 = (u16)((k ? T_PGREEN : T_PRED) | ((5 + k) << 12));
    }
}
void drawPlinko(void) {
    char s[32];
    fillScreen(DARK);
    // pegs, bins
    for (int r = 0; r < PEG_ROWS; r++) for (int c = 0; c < pegCount(r); c++) { int x = bx(pegX(r, c)), y = by(pegY(r)); rect(x - 3, y - 3, 6, 6, BLACK); rect(x - 2, y - 2, 4, 4, WHITE); }
    for (int i = 0; i <= 7; i++) rect(bx(i * 256 / 7.0f), by(170), 1, (int)(22 * PS), GREY);
    for (int i = 0; i < 7; i++) {
        int x0 = bx(i * 256 / 7.0f), x1 = bx((i + 1) * 256 / 7.0f);
        sprintf(s, "%d", BINS[i]); text((x0 + x1) / 2 - textW(s, 1) / 2, by(173), s, BINS[i] == 10 ? GOLD : WHITE, 1);
    }
    // the panel
    rect(PANEL - 1, 0, 1, SH, C_PANELLINE); rect(PANEL, 0, SW - PANEL, SH, C_PANEL);
    clipX0 = PANEL;
    drawImg(coin8, COIN_W, COIN_H, PANEL + 6, 3);
    sprintf(s, "%d", sv.coins); textS(PANEL + 3, 24, s, GOLD);
    textS(PANEL + 3, 40, "WON", GREY); sprintf(s, "%d", plWin); textS(PANEL + 3, 50, s, LIME);
    textS(PANEL + 3, 70, "5 a", GREY); textS(PANEL + 3, 80, "set", GREY);
    textS(PANEL + 3, 100, "A", YELLOW); textS(PANEL + 3, 110, "drop", GREY);
    textS(PANEL + 3, 130, "B", YELLOW); textS(PANEL + 3, 140, "back", GREY);
    clipAll();
    if (dropIdx >= 3) { int live = 0; for (int i = 0; i < 3; i++) live |= balls[i].live; if (!live) textCW(0, PANEL, 50, "A: another set", YELLOW, 1); }
    if (plBonusT > 0) { plBonusT--; textCW(0, PANEL, 70, plBonusMsg, C_FO, 1); }
    // the markers, spinning as they fall; the next one waits at the top where you aim
    hideSprites(); nObj = 0;
    for (int i = 0; i < 3; i++) if (balls[i].live || balls[i].done) pmSprite(i, bx(balls[i].x), by(balls[i].y), balls[i].rot);
    if (dropIdx < 3) pmSprite(dropIdx, bx(aimX), by(12), 1.5708f);
}
void inputPlinko(void) {
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    if (kHeld & KEY_LEFT) aimX -= 3;
    if (kHeld & KEY_RIGHT) aimX += 3;
    if (aimX < 10) aimX = 10;
    if (aimX > 246) aimX = 246;
    if (kDown & KEY_A) drop();
}
