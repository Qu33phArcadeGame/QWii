// bowl.c — QU33PH BOWLING (the website's bowling.html): ten frames, marker pins.
//
// The DS's lane photo from the cabinet's HIGH SCORE and MEGA boxes down to the ball return, at
// 0.62 of the DS's size in a 160 x 160 column; the ten-frame scoresheet, your score and how the
// marker's turned are in the panel beside it. The lane perspective, physics (hook, carry, the
// capsule-shaped marker, pin chain reactions), washouts, scoring, the 10th-frame chair and
// MEGA QU33PH are the DS version's (the website's), in its own units. The pins, your marker,
// its shadow, the chair and the ball-return queue are hardware sprites.
//
// CONTROLS: D-pad left/right spots your marker; hold A for power and let go to roll (holding
// left or right as you let go hooks it). B cancels. L/R turn the marker: VERT / ANGLED / FLAT.
// START pauses (SELECT quits to the arcade).
#include "qu.h"

#define AWf 256.0f
#define AHf 384.0f
#define NEAR_Y 0.830f
#define NEAR_HW 0.395f
#define LANE_CX 0.4900f
#define FAR_Y 0.360f
#define FAR_HW 0.094f
#define RET_Y 0.866f
#define RET_X0 0.2600f
#define RET_X1 0.7240f
#define HS_X 0.591f
#define HS_Y 0.2585f
#define BOX_L 0.233f
#define BOX_R 0.752f
#define BOX_Y 0.2585f
#define LANE_LEN 7.0f
#define PIN_R 0.080f
#define BALL_R 0.076f
#define BALL_HL 0.165f
#define PIN_D 0.215f
#define ROW_D 0.325f
#define HEAD_Y 5.95f
#define CHAIR_Y (HEAD_Y - 0.62f)
#define CHAIR_R 0.135f
#define MEGA_CHANCE 0.42f
#define BASE_HOOK -0.085f
#define DT (1.8f / 60.0f)                  // the website's real time, played 80% quicker on the DS
static const float KK = NEAR_HW / FAR_HW - 1, S1 = FAR_HW / NEAR_HW;

typedef struct { float X, Y, vx, vy, rot, vrot, sink; int col, down, gone; } Pin;
static Pin pins[10];
static const float LAYOUT[10][2] = { {0,0}, {-PIN_D,1}, {PIN_D,1}, {-2*PIN_D,2}, {0,2}, {2*PIN_D,2}, {-3*PIN_D,3}, {-PIN_D,3}, {PIN_D,3}, {3*PIN_D,3} };
static const int PIN_COL[10] = { 2, 0, 1, 0, 1, 0, 1, 0, 1, 0 };          // red 0, green 1, blue 2
static const int QUEUE[10] = { 0, 1, 0, 1, 0, 1, 0, 1, 0, 2 };            // blue always last
static struct { float X, Y, vx, vy, spin, rot, carry, wob; int col, live, gutter; } ball; static int haveBall;
static struct { float X, Y, vx, vy, rot; int shoved; } chr; static int haveChair;
static struct { float x0, y0, t; } megaFly; static int flying;
enum { PH_AIM, PH_ROLL, PH_MEGA, PH_OVER };
static int phase, frameNo, rollNo, rolls[24], nRolls, megaRight, megaWon, standStart, orientIdx = 1, msgT, coinsWon, newBest;
static float aimX, settleT, power; static int charging, ph;
static char msg[16]; static int msgCol;
static int bFrames;

typedef struct { const char *label; float rot, hook, speed, carry; } Ori;
static const Ori ORI[3] = { { "VERT", 0, 1.65f, 0.93f, 0.84f }, { "ANGLED", 0.7853982f, 1.00f, 1.00f, 1.00f }, { "FLAT", 1.5707963f, 0.42f, 1.07f, 1.18f } };


// ── the website's lane perspective ────────────────────────────────────────
static float scaleAt(float v) { return 1 / (1 + v * KK); }
static void proj(float u, float v, float *x, float *y, float *s) {
    float sc = scaleAt(v); *s = sc;
    *x = AWf * (LANE_CX + u * NEAR_HW * sc);
    *y = AHf * (NEAR_Y + (FAR_Y - NEAR_Y) * (1 - sc) / (1 - S1));
}
static int capsuleHit(float cx, float cy, float r, float *nx, float *ny) {
    float ax = fsin(ball.rot), ay = fcos(ball.rot);
    float t = (cx - ball.X) * ax + (cy - ball.Y) * ay; if (t < -BALL_HL) t = -BALL_HL; if (t > BALL_HL) t = BALL_HL;
    float dx = cx - (ball.X + ax * t), dy = cy - (ball.Y + ay * t), d = fsqrt(dx * dx + dy * dy);
    if (d >= r + BALL_R || d <= 0.0001f) return 0;
    *nx = dx / d; *ny = dy / d; return 1;
}
static float ballHalfWidth(float rot) { return fabsf_(fsin(rot)) * BALL_HL + BALL_R; }

// ── sounds ────────────────────────────────────────────────────────────────
static void sfxThrowB(void) {
    if (!sv.sfxOn) return;
    const s8 *d[18] = { bw_t0, bw_t1, bw_t2, bw_t3, bw_t4, bw_t5, bw_t6, bw_t7, bw_t8, bw_t9, bw_t10, bw_t11, bw_t12, bw_t13, bw_t14, bw_t15, bw_t16, bw_t17 };
    const int l[18] = { BW_T0_LEN, BW_T1_LEN, BW_T2_LEN, BW_T3_LEN, BW_T4_LEN, BW_T5_LEN, BW_T6_LEN, BW_T7_LEN, BW_T8_LEN, BW_T9_LEN, BW_T10_LEN, BW_T11_LEN,
                        BW_T12_LEN, BW_T13_LEN, BW_T14_LEN, BW_T15_LEN, BW_T16_LEN, BW_T17_LEN };
    int i = rand() % 18; sndFx(d[i], l[i]);
}
static int pinSfxT;
static void sfxPin(void) {
    if (pinSfxT > 0) return;                            // a strike's cascade would otherwise flood all the channels
    pinSfxT = 4;
    if (!sv.sfxOn) return;
    const s8 *d[6] = { bw_p0, bw_p1, bw_p2, bw_p3, bw_p4, bw_p5 };
    const int l[6] = { BW_P0_LEN, BW_P1_LEN, BW_P2_LEN, BW_P3_LEN, BW_P4_LEN, BW_P5_LEN };
    int i = rand() % 6; sndFx(d[i], l[i]);
}
static void say(const char *t, int c) { strncpy(msg, t, 15); msg[15] = 0; msgCol = c; msgT = 95; }

// ── the game ──────────────────────────────────────────────────────────────
static void rackPins(void) {
    for (int i = 0; i < 10; i++) pins[i] = (Pin){ LAYOUT[i][0], HEAD_Y + LAYOUT[i][1] * ROW_D, 0, 0, 0, 0, 0, PIN_COL[i], 0, 0 };
}
static int standing(void) { int n = 0; for (int i = 0; i < 10; i++) if (!pins[i].gone && !pins[i].down) n++; return n; }
// A WASHOUT is a split that includes the head pin: after the first ball the head pin is still up
// and the pins left standing are in separate groups with a gap between them (1-2-10, 1-2-4-10,
// 1-3-7, 1-3-6-7...). Pins next to each other (same row, or one row apart and touching) count as
// one group, so leaves like 1-2-4 or 1-3-6 (all touching) are NOT washouts.
static const u16 PIN_ADJ[10] = {                 // neighbours of each pin (bit k = pin k+1)
    (1 << 1) | (1 << 2),                                  // 1: 2 3
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4),            // 2: 1 3 4 5
    (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5),            // 3: 1 2 5 6
    (1 << 1) | (1 << 4) | (1 << 6) | (1 << 7),            // 4: 2 5 7 8
    (1 << 1) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8),   // 5: 2 3 4 6 8 9
    (1 << 2) | (1 << 4) | (1 << 8) | (1 << 9),            // 6: 3 5 9 10
    (1 << 3) | (1 << 7),                                  // 7: 4 8
    (1 << 3) | (1 << 4) | (1 << 6) | (1 << 8),            // 8: 4 5 7 9
    (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9),            // 9: 5 6 8 10
    (1 << 5) | (1 << 8) };                                // 10: 6 9
static int isWashout(void) {
    if (rollNo != 0 || pins[0].gone || pins[0].down) return 0;     // only on the first ball, head pin up
    u16 up = 0; int n = 0;
    for (int i = 0; i < 10; i++) if (!pins[i].gone && !pins[i].down) { up |= 1 << i; n++; }
    if (n < 2) return 0;
    u16 seen = 1, grow = 1;                                          // flood out from the head pin
    while (grow) { u16 nxt = 0; for (int i = 0; i < 10; i++) if (grow & (1 << i)) nxt |= PIN_ADJ[i] & up & ~seen; seen |= nxt; grow = nxt; }
    return seen != up;                                               // some standing pin is cut off by a gap
}
static int scoreTotal(void) {
    int s = 0, i = 0;
    for (int f = 0; f < 10 && i < nRolls; f++) {
        int a = rolls[i], b = i + 1 < nRolls ? rolls[i + 1] : 0, c = i + 2 < nRolls ? rolls[i + 2] : 0;
        if (a == 10) { s += 10 + b + c; i += 1; }
        else if (a + b == 10 && i + 1 < nRolls) { s += 10 + c; i += 2; }
        else { s += a + b; i += 2; }
    }
    return s;
}
static int finalScore(void) { return scoreTotal() + (megaWon ? 33 : 0); }
static void newGame(void) {
    frameNo = rollNo = nRolls = 0; aimX = 0; megaRight = rand() & 1; megaWon = 0; flying = 0; haveChair = 0;
    rackPins(); haveBall = 0; phase = PH_AIM; msgT = 0; standStart = 10; charging = 0;
}
static void setChair(void) {
    haveChair = frameNo == 9 && rollNo == 2 && standing() == 10 && !megaWon;
    if (haveChair) { chr.X = 0; chr.Y = CHAIR_Y; chr.vx = chr.vy = chr.rot = 0; chr.shoved = 0; }
}
static void rackFresh(void) { rackPins(); standStart = 10; setChair(); }
static void sweepAndStand(void) { for (int i = 0; i < 10; i++) if (pins[i].down) pins[i].gone = 1; standStart = standing(); }
static void readyNext(void) { haveBall = 0; phase = PH_AIM; }
static void endGame(void) {
    phase = PH_OVER;
    int s = finalScore();
    newBest = s > sv.arcadeBest[ARC_BOWLING];
    if (newBest) sv.arcadeBest[ARC_BOWLING] = s;
    sv.arcadePlays[ARC_BOWLING]++;
    int c = s / 20; if (c > 20) c = 20;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite();
    screen = S_BOWL_OVER;
}
static void endRoll(int fromMega) {
    if (phase == PH_MEGA && !fromMega) return;
    int felled = standStart - standing(), cleared = standing() == 0;
    if (nRolls < 24) rolls[nRolls++] = felled;
    int strike = felled == 10;
    if (!fromMega) {
        if (rollNo == 0 && strike) { say("QU33PH!", C_GOLD); sfxPlop(); }
        else if (rollNo > 0 && cleared) { say("SPARE", C_LIME); sfxPlop(); }
        else if (felled == 0 && haveBall && ball.gutter) { say("PEEF", RED); if (sv.sfxOn) sndFx(bw_peef, BW_PEEF_LEN); }
        else if (isWashout()) { say("WASHOUT", C_FO); if (sv.sfxOn) sndFx(bw_wash, BW_WASH_LEN); }
    }
    if (frameNo < 9) {
        if (rollNo == 0 && !strike) { rollNo = 1; sweepAndStand(); setChair(); readyNext(); }
        else { frameNo++; rollNo = 0; rackFresh(); readyNext(); }
        return;
    }
    if (rollNo == 0) { rollNo = 1; if (strike) rackFresh(); else sweepAndStand(); setChair(); readyNext(); }
    else if (rollNo == 1) {
        int openedWithStrike = nRolls >= 2 && rolls[nRolls - 2] == 10;
        if (openedWithStrike || cleared) { rollNo = 2; rackFresh(); readyNext(); }
        else endGame();
    } else endGame();
}
static void throwBall(float dx, float pw) {
    const Ori *o = &ORI[orientIdx];
    ball.X = aimX; ball.Y = 0; ball.vx = dx * 1.7f; ball.vy = 3.5f * pw * o->speed;
    ball.spin = (dx * 1.5f + BASE_HOOK) * o->hook; ball.rot = o->rot; ball.carry = o->carry;
    ball.col = QUEUE[frameNo]; ball.live = 1; ball.wob = 0; ball.gutter = 0; haveBall = 1;
    phase = PH_ROLL; settleT = 0;
    sfxThrowB();
}
static void knock(Pin *p, float nx, float ny, float f, float spinBias) {
    p->vx += nx * f; p->vy += ny * f;
    if (!p->down) { p->down = 1; p->rot = (frand() - 0.5f) * 0.5f; p->vrot = (frand() - 0.5f) * 9 + spinBias; sfxPin(); }
}
void updateBowl(void) {
    if (pinSfxT > 0) pinSfxT--;
    if (msgT > 0) msgT--;
    bFrames++;
    const float dt = DT;
    if (phase == PH_ROLL && haveBall && ball.live) {
        float grip = ball.vy / 2.2f; if (grip > 1) grip = 1;
        ball.vx += ball.spin * grip * dt * 2.4f;
        ball.vy -= ball.vy * 0.13f * dt;
        ball.X += ball.vx * dt; ball.Y += ball.vy * dt;
        ball.wob += ball.vy * dt * 7;
        float hw = ballHalfWidth(ball.rot);
        if (fabsf_(ball.X) > 1.0f - hw) {                     // PEEF: in the gutter, the roll is dead
            ball.X = (ball.X < 0 ? -1 : 1) * (1.0f - hw);
            ball.vx = ball.vy = ball.spin = 0; ball.gutter = 1; ball.live = 0; settleT = 0;
        }
        float nx, ny;
        if (haveChair && !chr.shoved && capsuleHit(chr.X, chr.Y, CHAIR_R, &nx, &ny)) {
            if (frand() < MEGA_CHANCE) {
                float x, y, s; proj(ball.X, ball.Y / LANE_LEN, &x, &y, &s);
                megaFly.x0 = x; megaFly.y0 = y; megaFly.t = 0; flying = 1; ball.live = 0; phase = PH_MEGA;
                chr.shoved = 1; chr.vy = 0.7f; sfxPlop();
            } else {
                chr.shoved = 1; chr.vy = ball.vy * 0.95f > 2.2f ? ball.vy * 0.95f : 2.2f; chr.vx = ball.vx * 0.7f;
                ball.vy *= 0.55f; sfxPin();
            }
        }
        for (int i = 0; i < 10; i++) { Pin *p = &pins[i]; if (p->gone) continue;
            if (capsuleHit(p->X, p->Y, PIN_R, &nx, &ny)) {
                float hit = (ball.vy * 0.80f + fabsf_(ball.vx) * 0.5f) * ball.carry; if (hit < 1.15f) hit = 1.15f;
                p->vx += nx * hit; p->vy += ny * hit;
                p->down = 1; p->rot = (frand() - 0.5f) * 0.5f; p->vrot = (frand() - 0.5f) * 9 - (nx > 0 ? 1 : -1) * 3;
                ball.vx += -nx * hit * 0.09f; ball.vy = ball.vy - 0.16f > 0.55f ? ball.vy - 0.16f : 0.55f;
                sfxPin();
            }
        }
        if (ball.Y > LANE_LEN + 0.6f || ball.vy < 0.22f) { ball.live = 0; settleT = 0; }
    }
    if (haveChair && chr.shoved && phase != PH_MEGA) {
        chr.X += chr.vx * dt; chr.Y += chr.vy * dt; chr.rot += chr.vx * dt * 2;
        chr.vy -= chr.vy * 1.5f * dt; chr.vx -= chr.vx * 1.5f * dt;
        for (int i = 0; i < 10; i++) { Pin *p = &pins[i]; if (p->gone || p->down) continue;
            float dx = p->X - chr.X, dy = p->Y - chr.Y;
            if (fsqrt(dx * dx + dy * dy) < CHAIR_R + PIN_R) {
                if (!dx) dx = 0.01f;
                if (!dy) dy = 0.01f;
                float d = fsqrt(dx * dx + dy * dy), f = chr.vy * 0.8f > 1.2f ? chr.vy * 0.8f : 1.2f;
                knock(p, dx / d, dy / d, f, 0);
            } }
        if (chr.Y > LANE_LEN + 1.2f) haveChair = 0;
    }
    if (phase == PH_MEGA && flying) {
        megaFly.t += dt;
        if (megaFly.t >= 1.05f) { megaWon = 1; say("MEGA QU33PH", RED); sfxPlop(); flying = 0; haveBall = 0; endRoll(1); }
    }
    for (int i = 0; i < 10; i++) { Pin *p = &pins[i]; if (p->gone) continue;
        if (fabsf_(p->vx) > 0.001f || fabsf_(p->vy) > 0.001f) {
            p->X += p->vx * dt; p->Y += p->vy * dt;
            p->vx -= p->vx * 1.9f * dt; p->vy -= p->vy * 1.9f * dt;
            p->rot += p->vrot * dt; p->vrot -= p->vrot * 2.2f * dt;
            if (p->down) { p->sink += dt * 2.4f; if (p->sink > 1) p->sink = 1; }
            for (int j = 0; j < 10; j++) { Pin *q = &pins[j]; if (q == p || q->gone) continue;
                float ddx = q->X - p->X, ddy = q->Y - p->Y, d = fsqrt(ddx * ddx + ddy * ddy);
                if (d < PIN_R * 2 && d > 0.0001f) {
                    float nx = ddx / d, ny = ddy / d, push = (PIN_R * 2 - d) * 0.5f;
                    q->X += nx * push; p->X -= nx * push; q->Y += ny * push; p->Y -= ny * push;
                    float t = fsqrt(p->vx * p->vx + p->vy * p->vy) * 0.92f;    // the website's stronger chain reaction
                    if (t > 0.10f) { q->vx += nx * t; q->vy += ny * t;
                        if (!q->down) { q->down = 1; q->rot = (frand() - 0.5f) * 0.5f; q->vrot = (frand() - 0.5f) * 8; sfxPin(); } }
                } }
            if (fabsf_(p->X) > 1.25f || p->Y > LANE_LEN + 1.1f) p->gone = 1;
        } }
    if (phase == PH_ROLL && haveBall && !ball.live) {
        settleT += dt;
        int moving = 0;
        for (int i = 0; i < 10; i++) if (!pins[i].gone && fsqrt(pins[i].vx * pins[i].vx + pins[i].vy * pins[i].vy) > 0.14f) moving = 1;
        if (!moving || settleT > 0.9f) endRoll(0);                // (a shorter wait for the pins to settle)
    }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputBowl(void) {
    if (screen == S_BOWL_PAUSE) {
        if (kDown & KEY_START) screen = S_BOWL;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_BOWL_PAUSE; charging = 0; return; }
    if (phase != PH_AIM) { charging = 0; return; }
    if (kDown & KEY_L) orientIdx = (orientIdx + 2) % 3;
    if (kDown & KEY_R) orientIdx = (orientIdx + 1) % 3;
    if (!charging) {
        if (kHeld & KEY_LEFT)  { aimX -= 0.02f; if (aimX < -0.72f) aimX = -0.72f; }
        if (kHeld & KEY_RIGHT) { aimX += 0.02f; if (aimX > 0.72f) aimX = 0.72f; }
    }
    if (kDown & KEY_A) { charging = 1; ph = 0; }
    if (charging) {
        ph++; float p = (ph % 44) / 22.0f; p = p < 1 ? p : 2 - p; power = 0.35f + p;
        if (kDown & KEY_B) charging = 0;
        else if (kUp & KEY_A) {                        // holding left/right as you let go adds hook
            charging = 0;
            if (motionTap) power = 0.35f + motionPower;   // (a swing of the Wiimote: how hard)
            throwBall((kHeld & KEY_LEFT) ? -0.12f : (kHeld & KEY_RIGHT) ? 0.12f : 0, power);
        }
    }
}

// ── sprites ───────────────────────────────────────────────────────────────
// tiles: the three markers full size (32x64: your marker), the main game's chair (64x64), the
// MEGA card (32x16), the shadow (32x16), the pins pre-shrunk (16x32), the ball return's lying
// markers (16x8); palettes 0-2 the markers, 3 the chair, 4 the card, 5 the shadow, 6-8 the
// pins, 9-11 the ball return.
// (The GBA can only draw so many sprite pixels on each screen line: the pins and the ball
//  return use small pictures so your marker always fits.)
enum { T_PIN = 512, T_WCHAIR = 608, T_MEGA = 672, T_WSH = 680, T_SPIN = 688, T_Q = 712 };
void bowlObjLoad(void) {
    const u8 *P[3] = { bw_pin0, bw_pin1, bw_pin2 }; const u16 *PP[3] = { bw_pin0_pal, bw_pin1_pal, bw_pin2_pal };
    for (int i = 0; i < 3; i++) { platObjTiles(T_PIN + i * 32, P[i], 1024); memcpy(&objPal[i * 16], PP[i], 32); }
    platObjTiles(T_WCHAIR, obj_chair, 2048); memcpy(&objPal[48], obj_chair_pal, 32);
    platObjTiles(T_MEGA, bw_mega, 256); memcpy(&objPal[64], bw_mega_pal, 32);
    platObjTiles(T_WSH, mm_shadow, 256); objPal[80 + 1] = 0;
    const u8 *S[3] = { bw_spin0, bw_spin1, bw_spin2 }, *Q[3] = { bw_q0, bw_q1, bw_q2 };
    const u16 *SP[3] = { bw_spin0_pal, bw_spin1_pal, bw_spin2_pal }, *QP[3] = { bw_q0_pal, bw_q1_pal, bw_q2_pal };
    for (int i = 0; i < 3; i++) {
        platObjTiles(T_SPIN + i * 8, S[i], 256); memcpy(&objPal[(6 + i) * 16], SP[i], 32);
        platObjTiles(T_Q + i * 2, Q[i], 64); memcpy(&objPal[(9 + i) * 16], QP[i], 32);
    }
}
// The DS draws back to front (later on top); the GBA puts the first sprite on top, so each
// frame's sprites are collected in the DS's order and written out reversed.
typedef struct { u16 a0, a1, a2; s16 pa, pb, pc, pd; u8 affine; } Spr;
static Spr sl[48] EWRAM_BSS; static int nsl;
// a turned, scaled sprite; dbl gives it the double-size box (needed only when the turned
// picture would spill out of its own box: it costs twice the line budget)
static void aff(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, float sx, float sy, float ang, int see, int dbl) {
    if (nsl >= 48 || sx < 0.02f || sy < 0.02f) return;
    if (cx < -w * 2 || cx > SW + w * 2 || cy < -h * 2 || cy > SH + h * 2) return;
    Spr *o = &sl[nsl++];
    float c = fcos(ang), s = fsin(ang);
    o->pa = (s16)(c / sx * 256); o->pb = (s16)(s / sx * 256); o->pc = (s16)(-s / sy * 256); o->pd = (s16)(c / sy * 256);
    int bw = dbl ? w : w / 2, bh = dbl ? h : h / 2;
    o->a0 = (u16)(((cy - bh) & 0xFF) | (dbl ? 0x0300 : 0x0100) | (see ? 0x0400 : 0) | (shape << 14));
    o->a1 = (u16)(((cx - bw) & 0x1FF) | (size << 14));
    o->a2 = (u16)(tile | (pal << 12)); o->affine = 1;
}
static void plain(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, int see) {   // a plain (unturned) sprite
    if (nsl >= 48) return;
    Spr *o = &sl[nsl++];
    o->a0 = (u16)(((cy - h / 2) & 0xFF) | (see ? 0x0400 : 0) | (shape << 14));
    o->a1 = (u16)(((cx - w / 2) & 0x1FF) | (size << 14));
    o->a2 = (u16)(tile | (pal << 12)); o->affine = 0;
}
static void flushSprites(void) {
    hideSprites();
    int m = 0;
    for (int k = 0; k < nsl && k < 128; k++) {
        int i = k; Spr *o = &sl[nsl - 1 - k];
        oam[i].a0 = o->a0; oam[i].a1 = o->a1; oam[i].a2 = o->a2;
        if (o->affine && m < 32) {
            oam[i].a1 = (u16)(o->a1 | (m << 9));
            oam[m * 4].a3 = (u16)o->pa; oam[m * 4 + 1].a3 = (u16)o->pb; oam[m * 4 + 2].a3 = (u16)o->pc; oam[m * 4 + 3].a3 = (u16)o->pd;
            m++;
        } else if (o->affine) oam[i].a0 = 0x0200;      // (out of turn tables: skip)
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
#define GXb(x) ((int)((x) * BWL_S))                 // the DS's lane picture (its pixels) on the GBA
#define GYb(y) ((int)(((y) - BWL_Y0) * BWL_S))
#define PX0 (BWL_W + 1)
#define PW (SW - PX0)
static void marker(int c, float x, float y, float h, float rot, int stip) {    // your marker, 'h' DS pixels tall at x,y
    aff(T_PIN + c * 32, c, 2, 3, 32, 64, GXb(x), GYb(y), h * BWL_S / 52, h * BWL_S / 52, rot, stip, 1);
}
static void pinMarker(int c, float x, float y, float h, float rot, int stip) {  // a pin: the small picture (16 tall), a touch bigger than true
    float k = h * BWL_S * 1.2f / 16;
    aff(T_SPIN + c * 8, 6 + c, 2, 2, 16, 32, GXb(x), GYb(y), k, k, rot, stip, k > 0.9f);
}
static void shadow(float x, float y, float h, float rot) {       // the capsule's shadow: a flat oval, longer when it's turned
    float len = (fabsf_(fsin(rot)) * 0.92f + 0.30f) * h * BWL_S, wid = h * 0.10f * BWL_S + 1;
    aff(T_WSH, 5, 1, 2, 32, 16, GXb(x), GYb(y + h * 0.09f), len / 32, wid / 16, 0, 1, len > 30);
}
static void drawPins(void) {
    int order[10]; for (int i = 0; i < 10; i++) order[i] = i;
    for (int i = 0; i < 10; i++) for (int j = i + 1; j < 10; j++) if (pins[order[j]].Y > pins[order[i]].Y) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int k = 0; k < 10; k++) { Pin *p = &pins[order[k]]; if (p->gone) continue;
        float v = p->Y / LANE_LEN; if (v < 0 || v > 1.14f) continue;
        float x, y, s; proj(p->X, v < 1.13f ? v : 1.13f, &x, &y, &s);
        float h = AHf * 0.085f * s * 1.9f;
        if (p->down) { if (p->sink < 0.8f) pinMarker(p->col, x, y + h * 0.16f, h * 0.92f, 1.5707963f + p->rot, p->sink > 0.4f); }
        else pinMarker(p->col, x, y - h * 0.30f, h, 0, 0);
    }
}
static void fmtFrame(int f, char *marks, int *total) {             // the website's frameView, one frame
    int i = 0, run = 0; marks[0] = 0; *total = -1;
    for (int k = 0; k <= f; k++) {
        if (i >= nRolls) return;
        int a = rolls[i], hb = i + 1 < nRolls, hc = i + 2 < nRolls, b = hb ? rolls[i + 1] : 0, c = hc ? rolls[i + 2] : 0;
        char m[12] = ""; int tot = -1;
        #define MK(v) ((v) == 0 ? '-' : '0' + (v))
        if (k < 9) {
            if (a == 10) { strcpy(m, "X"); if (hb && hc) { run += 10 + b + c; tot = run; } i += 1; }
            else if (hb && a + b == 10) { sprintf(m, "%c /", MK(a)); if (hc) { run += 10 + c; tot = run; } i += 2; }
            else if (hb) { sprintf(m, "%c %c", MK(a), MK(b)); run += a + b; tot = run; i += 2; }
            else { sprintf(m, "%c", MK(a)); i += 1; }
        } else {
            int n = 0; char t[3] = { 0 };
            t[n++] = a == 10 ? 'X' : MK(a);
            if (hb) t[n++] = b == 10 ? 'X' : (a != 10 && a + b == 10 ? '/' : MK(b));
            if (hc) t[n++] = c == 10 ? 'X' : (b != 10 && a == 10 && b + c == 10 ? '/' : MK(c));
            for (int q = 0; q < n; q++) { m[q * 2] = t[q]; m[q * 2 + 1] = q < n - 1 ? ' ' : 0; }
            int bonus = a == 10 || (hb && a + b == 10), done = bonus ? hc : hb;
            if (done) { run += a + b + c; tot = run; }
        }
        #undef MK
        if (k == f) { strcpy(marks, m); *total = tot; return; }
    }
}
static void drawPanel(void) {
    char s[24];
    clipX0 = PX0;
    // the ten frames: 1-5 down the left, 6-10 down the right
    for (int f = 0; f < 10; f++) {
        int x = PX0 + 2 + (f / 5) * 39, y = 2 + (f % 5) * 22, cur = f == frameNo && phase != PH_OVER;
        int e = cur ? GOLD : C_PANELLINE;
        rect(x, y, 37, 21, e); rect(x + 1, y + 1, 35, 19, C_DARK);
        sprintf(s, "%d", f + 1); textS(x + 2, y + 1, s, C_DIMEDGE);
        char m[12]; int tot; fmtFrame(f, m, &tot);
        textS(x + 36 - textSW(m) - 2, y + 1, m, WHITE);
        if (tot >= 0) { sprintf(s, "%d", tot); textS(x + (37 - textSW(s)) / 2, y + 10, s, GOLD); }
    }
    textS(PX0 + 3, 114, "SCORE", GREY);
    sprintf(s, "%d", finalScore()); text(PX0 + 77 - textW(s, 1), 112, s, WHITE, 1);
    if (megaWon) textS(PX0 + 3, 126, "+33 MEGA", RED);
    if (phase == PH_AIM) {
        textS(PX0 + 3, 138, "1 / 2", GREY); textS(PX0 + 30, 138, ORI[orientIdx].label, C_LBLUE);
        textS(PX0 + 3, 149, rollNo == 0 ? "swing or hold A" : "SPARE ATTEMPT", rollNo == 0 ? GREY : YELLOW);
    }
    clipAll();
}
static void drawLaneAndPlay(void) {
    drawDirect();
    for (int y = 0; y < SH; y++) {                         // the lane, then the panel
        copy32(&page[y * (SW / 2)], &bwl8[y * BWL_W], BWL_W / 4);
        fill32(&page[y * (SW / 2) + BWL_W / 2], C_PANEL * 0x01010101u, (SW - BWL_W) / 4);
    }
    rect(BWL_W, 0, 1, SH, C_PANELLINE);
    nsl = 0;
    clipX0 = 0; clipX1 = BWL_W; clipY0 = 0; clipY1 = SH;
    // the MEGA card in one of the speaker boxes: lit in the tenth frame
    { int armed = frameNo == 9 && !megaWon;
      aff(T_MEGA, 4, 1, 2, 32, 16, GXb(AWf * (megaRight ? BOX_R : BOX_L)), GYb(AHf * BOX_Y), BWL_S, BWL_S, 0, !(armed || megaWon), 0); }
    drawPins();
    if (haveBall && phase != PH_MEGA) {
        float v = ball.Y / LANE_LEN;
        if (v <= 1.16f) { float x, y, s; proj(ball.X, v < 0 ? 0 : v > 1.15f ? 1.15f : v, &x, &y, &s);
            float h = AHf * 0.085f * s * 1.75f;
            shadow(x, y, h, ball.rot);
            marker(ball.col, x, y - h * 0.06f, h * 0.86f, ball.rot + fsin(ball.wob) * 0.10f, 0); }
    }
    if (haveChair) {
        float v = chr.Y / LANE_LEN;
        if (v >= 0 && v <= 1.16f) { float x, y, s; proj(chr.X, v < 1.15f ? v : 1.15f, &x, &y, &s);
            float h = AHf * 0.085f * s * 2.3f, k = h * BWL_S / 40;
            aff(T_WCHAIR, 3, 0, 3, 64, 64, GXb(x), GYb(y - h * 0.34f), k, k, chr.rot, 0, 0); }
    }
    if (flying) {
        float t = megaFly.t / 1.05f; if (t > 1) t = 1; float e = t * t * (3 - 2 * t);
        float tx = AWf * (megaRight ? BOX_R : BOX_L), ty = AHf * BOX_Y;
        float x = megaFly.x0 + (tx - megaFly.x0) * e, y = megaFly.y0 + (ty - megaFly.y0) * e - fsin(t * 3.14159f) * AHf * 0.10f;
        pinMarker(2, x, y, AHf * 0.045f * (1 - t * 0.45f), t * 10, 0);
    }
    if (phase == PH_AIM) {
        float x, y, s; proj(aimX, 0.012f, &x, &y, &s);
        float h = AHf * 0.085f * s * 1.75f;
        shadow(x, y, h, ORI[orientIdx].rot);
        marker(QUEUE[frameNo], x, y - h * 0.06f, h * 0.86f, ORI[orientIdx].rot, 0);
        if (charging)                                      // the power marker, pointing up the lane
            powerMarker(GXb(x), GYb(y - 30), 0, -1, (30 + (power - 0.35f) * 90) * BWL_S, power - 0.35f);
    }
    // the ball return: the ten markers in order, this frame's one bigger (and blinking)
    { float y = AHf * RET_Y, step = (RET_X1 - RET_X0) / 9, h = AHf * 0.030f;
      (void)h;
      for (int i = 0; i < 10; i++) {
          float x = AWf * (RET_X0 + step * i); int cur = i == frameNo;
          if (cur && ((bFrames / 12) & 1)) for (int j = -4; j <= 4; j++) for (int k = -4; k <= 4; k++) if (j * j + k * k <= 16) pset(GXb(x) + k, GYb(y) + j, GOLD);
          plain(T_Q + QUEUE[i] * 2, 9 + QUEUE[i], 1, 0, 16, 8, GXb(x), GYb(y), i < frameNo);   // (cheap plain sprites)
      } }
    // the best on the cabinet's "High Score:" box
    { char s[12]; sprintf(s, "%d", sv.arcadeBest[ARC_BOWLING]); textS(GXb(AWf * HS_X), GYb(AHf * HS_Y) - 5, s, RED); }
    if (msgT > 0) textCW(0, BWL_W, 40, msg, msgCol, 2);
    clipAll();
    drawPanel();
    flushSprites();
}
void drawBowl(void) {
    drawLaneAndPlay();
    if (screen == S_BOWL_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn BB[3]; static int menuSel, overSel;
static void bmLayout(void) { BB[0] = (Btn){ 136, 40, 100, 30, "BOWL", 0, 0 }; BB[1] = (Btn){ 136, 80, 100, 26, "BACK", 0, 0 }; }
void drawBowlMenu(void) {
    char s[40];
    fillScreen(DARK);
    text(6, 2, "BOWLING", GOLD, 1);
    static const char *L[9] = { "Ten frames. Markers are pins.", "D-pad: spot your marker.", "Hold A, let go to roll;", "hold left/right as you",
        "let go to hook it.", "1/2: VERT hooks most,", "FLAT is fast, carries most.", "10th frame: hit the chair", "for MEGA QU33PH (+33)" };
    for (int i = 0; i < 9; i++) textS(6, 20 + i * 11, L[i], i >= 7 ? C_GOLD : WHITE);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_BOWLING]); textS(6, 130, s, GREY);
    coinCount(6, 140, 0);
    bmLayout();
    drawBtns(BB, 2, menuSel);
}
void inputBowlMenu(void) {
    bmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(BB, 2, &menuSel, 1);
    if (h == 0) { newGame(); screen = S_BOWL; }
    if (h == 1) goScreen(S_ARCADE);
}
void drawBowlOver(void) {
    char s[40];
    drawLaneAndPlay();
    hideSprites();
    box(20, 16, 200, 128, C_TOAST, GOLD);
    sprintf(s, "FINAL %d", finalScore()); textC(22, s, GOLD, 2);
    sprintf(s, "HIGH %d%s", sv.arcadeBest[ARC_BOWLING], newBest ? "   NEW BEST!" : ""); textC(54, s, newBest ? LIME : WHITE, 1);
    sprintf(s, "%d COINS EARNED", coinsWon); textC(70, s, GOLD, 1);
    BB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; BB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    drawBtns(BB, 2, overSel);
}
void inputBowlOver(void) {
    BB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; BB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    int h = btnInput(BB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) { newGame(); screen = S_BOWL; }
    if (h == 1) goScreen(S_ARCADE);
}
