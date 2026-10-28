// ball.c — QU33PH-BALL (the website's qu33phball.html): three skee-ball machines.
//
// Each machine's cabinet photo, from its top score boxes down to the roll spot, at half the DS's
// size in a column on the left; the panel on the right has the score, the markers left and the
// best. The scoring shapes, roll maths, chair kicks, chair-tower climb and every timing are the
// DS version's (the website's), frame for frame. The markers and their shadows are hardware
// sprites.
//
// CONTROLS: D-pad left/right to aim, hold A for power and let go to roll (B cancels). L/R
// switch machine before your first roll. START pauses (SELECT then quits to the arcade).
#include "qu.h"

#define BW 256.0f
#define BPH 455.0f
#define BALL_OFF 71
#define BS 0.5f                                // GBA pixels per DS pixel
#define GX(fx) ((int)((fx) * BW * BS))
#define GY(fy) ((int)(((fy) * BPH - BALL_OFF) * BS))
#define ASPECT 1.78f                         // the website's PH/100: makes circles round on the tall field
#define TOTAL 9
#define LAND_HOLD 13                         // (the website's 20, 46 and 16 frames, quickened a third for the DS)
#define FEED_DUR 11
#define HOP_T 19                              // one chair hop (the website's 26)
#define QSLOTS 8
#define QX0 0.325f
#define QSPAN 0.35f
#define MEGA_VAL 33000

// ── the three machines (fractions of the whole cabinet photo, from the website) ──────────
enum { SH_RECT, SH_CIRCLE, SH_ARC, SH_POLY };
typedef struct {
    u8 kind; float a, b, c, d;               // rect x0,x1,y0,y1 | circle cx,cy,r | arc cx,cy,rIn,rOut
    float a0, a1, clipY, clipX0, clipX1;      // arc angles (degrees, 0 = east, 90 = south) and flat cut-offs
    int p; u8 chair; float tox, toy; s8 side; // chair: lobs the marker to (tox,toy); side: chair-tower climb
    const float *pts; int n;                  // poly
} Shape;
#define NOCLIP -1, -1, 9
#define RECT(x0, x1, y0, y1, p) { SH_RECT, x0, x1, y0, y1, 0, 0, NOCLIP, p, 0, 0, 0, 0, 0, 0 }
#define CHAIR(x0, x1, y0, y1, p, tx, ty) { SH_RECT, x0, x1, y0, y1, 0, 0, NOCLIP, p, 1, tx, ty, 0, 0, 0 }
#define CLIMB(x0, x1, y0, y1, s) { SH_RECT, x0, x1, y0, y1, 0, 0, NOCLIP, 0, 1, 0, 0, s, 0, 0 }
#define CIRC(cx, cy, r, p) { SH_CIRCLE, cx, cy, r, 0, 0, 0, NOCLIP, p, 0, 0, 0, 0, 0, 0 }
#define ARC(cx, cy, ri, ro, a0, a1, cy0, cx0, cx1, p) { SH_ARC, cx, cy, ri, ro, a0, a1, cy0, cx0, cx1, p, 0, 0, 0, 0, 0, 0 }
#define POLY(pt, clip, p) { SH_POLY, 0, 0, 0, 0, 0, 0, clip, -1, 9, p, 0, 0, 0, 0, pt, sizeof(pt) / sizeof(float) / 2 }

static const float POLY_C1[] = { 0.312f,0.290f, 0.317f,0.264f, 0.340f,0.269f, 0.367f,0.282f, 0.383f,0.289f, 0.356f,0.316f, 0.352f,0.295f, 0.317f,0.293f };
static const float POLY_C2[] = { 0.701f,0.271f, 0.705f,0.288f, 0.677f,0.291f, 0.658f,0.325f, 0.630f,0.288f, 0.669f,0.289f, 0.688f,0.279f };
static const float POLY_T[] = { 0.393f,0.370f, 0.420f,0.357f, 0.443f,0.357f, 0.461f,0.365f, 0.479f,0.388f, 0.456f,0.408f, 0.437f,0.415f,
    0.418f,0.413f, 0.395f,0.401f, 0.413f,0.421f, 0.462f,0.439f, 0.469f,0.397f, 0.491f,0.387f, 0.544f,0.394f, 0.556f,0.412f, 0.550f,0.443f,
    0.599f,0.425f, 0.620f,0.409f, 0.568f,0.405f, 0.553f,0.389f, 0.551f,0.370f, 0.606f,0.356f, 0.620f,0.366f, 0.624f,0.347f, 0.595f,0.327f,
    0.556f,0.310f, 0.493f,0.304f, 0.437f,0.322f, 0.394f,0.356f, 0.457f,0.351f, 0.481f,0.313f, 0.497f,0.310f, 0.541f,0.317f, 0.556f,0.328f,
    0.566f,0.340f, 0.555f,0.359f };
static const Shape SHAPES_CLASSIC[] = {
    RECT(0.473f, 0.554f, 0.243f, 0.270f, 50), RECT(0.469f, 0.550f, 0.270f, 0.301f, 40), RECT(0.458f, 0.564f, 0.303f, 0.344f, 30),
    ARC(0.515f, 0.127f, 0.257f, 0.433f, 69, 114, -1, -1, 9, 20), ARC(0.510f, 0.290f, 0.165f, 0.239f, 0, 176, -1, -1, 9, 10),
    CIRC(0.374f, 0.263f, 0.036f, 100), CIRC(0.651f, 0.260f, 0.047f, 100),
    POLY(POLY_C1, -1, 10), POLY(POLY_C2, -1, 10) };
static const Shape SHAPES_ADVANCED[] = {
    CHAIR(0.247f, 0.338f, 0.462f, 0.575f, 75, 0.2245f, 0.399f), CHAIR(0.564f, 0.658f, 0.462f, 0.575f, 75, 0.6715f, 0.399f),
    RECT(0.249f, 0.318f, 0.295f, 0.342f, 50), RECT(0.318f, 0.409f, 0.276f, 0.332f, 100), RECT(0.426f, 0.480f, 0.282f, 0.312f, 333),
    RECT(0.502f, 0.588f, 0.275f, 0.336f, 100), RECT(0.593f, 0.660f, 0.294f, 0.343f, 50),
    RECT(0.276f, 0.369f, 0.354f, 0.415f, 30), RECT(0.404f, 0.502f, 0.353f, 0.416f, 40), RECT(0.543f, 0.631f, 0.357f, 0.412f, 30),
    RECT(0.192f, 0.253f, 0.367f, 0.473f, 75), RECT(0.655f, 0.720f, 0.368f, 0.465f, 75) };
static const Shape SHAPES_TOWER[] = {
    CLIMB(0.180f, 0.370f, 0.635f, 0.765f, -1), CLIMB(0.630f, 0.820f, 0.635f, 0.765f, 1),
    CIRC(0.514f, 0.352f, 0.038f, 5000), CIRC(0.429f, 0.385f, 0.039f, 2000), CIRC(0.598f, 0.386f, 0.036f, 2000), CIRC(0.511f, 0.418f, 0.036f, 1000),
    RECT(0.305f, 0.445f, 0.290f, 0.380f, 0), RECT(0.581f, 0.721f, 0.290f, 0.380f, 0),
    ARC(0.513f, 0.381f, 0.128f, 0.152f, 5, 350, 0.300f, 0.297f, 0.689f, 500),
    ARC(0.513f, 0.381f, 0.170f, 0.220f, 5, 350, 0.300f, 0.341f, 0.695f, 250),
    ARC(0.513f, 0.381f, 0.220f, 0.270f, 5, 350, 0.300f, 0.339f, 0.691f, 100),
    POLY(POLY_T, 0.290f, 500) };

typedef struct {
    const char *name; float sx, sy, queueY; int dur; float shrink, hudx[3], hudy[3], topY, pullDiv, aimDiv, cx, aimSpan, aimScat,
    baseY, powY, jitX, jitY, overFrom, overK; int scat; const Shape *sh; int nsh; int coinDiv;
} Machine;
static const Machine MACH[3] = {
    { "CLASSIC", 0.490f, 0.775f, 0.735f, 32, 0.55f, { 0.364f, 0.511f, 0.655f }, { 0.197f, 0.196f, 0.196f }, 0.218f, 0.46f, 0.28f,
      0.523f, 0.175f, 0.95f, 0.505f, 0.265f, 0.059f, 0.062f, 2.0f, 0.0f, 0, SHAPES_CLASSIC, sizeof SHAPES_CLASSIC / sizeof(Shape), 50 },
    { "ADVANCED", 0.490f, 0.820f, 0.790f, 34, 0.58f, { 0.319f, 0.455f, 0.585f }, { 0.231f, 0.231f, 0.230f }, 0.232f, 0.46f, 0.28f,
      0.443f, 0.256f, 1.15f, 0.560f, 0.320f, 0.043f, 0.048f, 0.84f, 7.0f, 1, SHAPES_ADVANCED, sizeof SHAPES_ADVANCED / sizeof(Shape), 100 },
    { "CHAIR TOWER", 0.500f, 0.820f, 0.790f, 34, 0.58f, { 0.408f, 0.513f, 0.617f }, { 0.281f, 0.279f, 0.281f }, 0.340f, 0.46f, 0.28f,
      0.500f, 0.285f, 1.08f, 0.765f, 0.589f, 0.040f, 0.045f, 0.92f, 5.0f, 2, SHAPES_TOWER, sizeof SHAPES_TOWER / sizeof(Shape), 1000 } };
static float scatOf(int k, float p) {                  // the website's scatter curve per machine
    float u = p - (k == 0 ? 0.70f : k == 1 ? 0.74f : 0.73f); if (u < 0) u = -u;
    if (k == 0) return 0.55f + u * 1.5f;
    if (k == 1) return 0.55f + u * 1.2f + (p > 0.80f ? (p - 0.80f) * 4.5f : 0);
    return 0.52f + u * 1.25f + (p > 0.84f ? (p - 0.84f) * 3.6f : 0);
}

// ── maths ─────────────────────────────────────────────────────────────────
static float fatan2(float y, float x) {                // degrees 0..360 is all the arcs need
    float ax = fabsf_(x), ay = fabsf_(y);
    if (ax < 1e-6f && ay < 1e-6f) return 0;
    float a = (ax < ay ? ax / ay : ay / ax), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) r = -r;
    return r * 57.2957795f;
}
static int inShape(const Shape *r, float lx, float ly) {
    if (r->kind == SH_CIRCLE) { float dx = lx - r->a, dy = (ly - r->b) * ASPECT; return dx * dx + dy * dy <= r->c * r->c; }
    if (r->kind == SH_ARC) {
        if (ly < r->clipY || lx < r->clipX0 || lx > r->clipX1) return 0;
        float dx = lx - r->a, dy = (ly - r->b) * ASPECT, d2 = dx * dx + dy * dy;
        if (d2 < r->c * r->c || d2 > r->d * r->d) return 0;
        float ang = fatan2(dy, dx); if (ang < 0) ang += 360;
        return r->a0 <= r->a1 ? (ang >= r->a0 && ang <= r->a1) : (ang >= r->a0 || ang <= r->a1);
    }
    if (r->kind == SH_POLY) {
        if (ly < r->clipY) return 0;
        int in = 0; const float *p = r->pts;
        for (int i = 0, j = r->n - 1; i < r->n; j = i++) {
            float xi = p[i * 2], yi = p[i * 2 + 1], xj = p[j * 2], yj = p[j * 2 + 1];
            if (((yi > ly) != (yj > ly)) && (lx < (xj - xi) * (ly - yi) / (yj - yi) + xi)) in = !in;
        }
        return in;
    }
    return lx >= r->a && lx <= r->b && ly >= r->c && ly <= r->d;
}
static float ease(float t) { return 1 - (1 - t) * (1 - t); }
static void fmtK(char *o, int n) {                      // 12,400 -> "12.4K", 5,000 -> "5K"
    if (n < 1000) { sprintf(o, "%d", n); return; }
    int t = (n + 50) / 100;                            // tenths of a thousand
    if (t % 10) sprintf(o, "%d.%dK", t / 10, t % 10); else sprintf(o, "%dK", t / 10);
}

// ── state ─────────────────────────────────────────────────────────────────
enum { ST_READY, ST_ROLL, ST_LANDED, ST_FEED, ST_OVER };
typedef struct { float x, y; int p; char label[16]; } Stop;
typedef struct { float x, y; int delay, life, pts; char txt[16]; } Pop;
static int mach, curIdx, resolved, score, state, landT, pendingScore, coinsWon, newBest, hopN, hopAt, hasChair;
static struct { float x0, y0, x1, y1, spin; int t, dur, col, caught; } ball;
static struct { float x0, y0, x1, y1; int t, on; } hop;
static struct { float fromX; int t, col; } feed;
static Stop hopQ[5], lastChair;
static Pop pops[8];
static int charging, chargeT, ph;
static float aim, power;

static const Machine *M(void) { return &MACH[mach]; }
static int colOf(int i) { return i % 3; }               // red, green, blue, red ...
static float qx(int k) { return QX0 + ((k - 1) / (float)(QSLOTS - 1)) * QSPAN; }
static int best(void) { return sv.ballBest[mach]; }

static void reset(void) {
    curIdx = resolved = score = pendingScore = 0; state = ST_READY; landT = 0; hop.on = 0; hopN = hopAt = 0; hasChair = 0;
    memset(pops, 0, sizeof pops); charging = chargeT = 0; aim = 0;
}
static void startBall(int m) { mach = m; reset(); screen = S_BALL; }
static int canRoll(void) { return screen == S_BALL && state == ST_READY; }

static void roll(float pw, float am) {
    const Machine *f = M();
    if (sv.sfxOn) sndFx(bs_roll, BS_ROLL_LEN);
    float p = pw < 0 ? 0 : pw > 1 ? 1 : pw, a = am < -1 ? -1 : am > 1 ? 1 : am;
    float sc = scatOf(f->scat, p) + fabsf_(a) * f->aimScat;
    float lx = f->cx + a * f->aimSpan + (frand() - 0.5f) * f->jitX * sc;
    float ly = f->baseY - p * f->powY + (frand() - 0.5f) * f->jitY * sc;
    int over = p > f->overFrom && frand() < ((p - f->overFrom) * f->overK > 0.85f ? 0.85f : (p - f->overFrom) * f->overK);
    if (over) ly = f->topY + 0.006f;
    if (ly < f->topY) ly = f->topY;                      // nothing lands above the board
    ball.x0 = f->sx; ball.y0 = f->sy; ball.x1 = lx; ball.y1 = ly; ball.t = 0; ball.dur = f->dur;
    ball.col = colOf(curIdx); ball.spin = 0; ball.caught = 0;
    state = ST_ROLL; charging = 0;
}

static void stop(Stop *s, float x, float y, int p, const char *l) { s->x = x; s->y = y; s->p = p; strcpy(s->label, l); }
static int scoreLanding(float lx, float ly) {
    const Machine *f = M(); hasChair = 0; hopN = hopAt = 0;
    for (int i = 0; i < f->nsh; i++) {
        const Shape *r = &f->sh[i];
        if (!inShape(r, lx, ly)) continue;
        if (r->chair && r->side) {
            // the chair tower: the bottom chair always pays 1,000; 75% climb on to 5,000; 45% of
            // those reach the top chair, which splits evenly between 20,000 and the MEGA
            int L = r->side < 0, fin = 1000;
            stop(&hopQ[hopN++], L ? 0.220f : 0.780f, 0.585f, 1000, "+1,000");
            if (frand() < 0.75f) {
                stop(&hopQ[hopN++], L ? 0.260f : 0.740f, 0.445f, 5000, "+5,000"); fin = 5000;
                if (frand() < 0.45f) {
                    stop(&hopQ[hopN++], L ? 0.255f : 0.745f, 0.250f, 0, "TOP CHAIR");
                    if (frand() < 0.5f) { stop(&hopQ[hopN++], L ? 0.245f : 0.755f, 0.175f, 20000, "+20,000"); fin = 20000; }
                    else { stop(&hopQ[hopN++], 0.510f, 0.174f, MEGA_VAL, "MEGA +33K"); fin = MEGA_VAL; }
                }
            }
            lastChair = hopQ[hopAt++]; hasChair = 1;
            return fin;
        }
        if (r->chair) { char l[16]; sprintf(l, "CHAIR +%d", r->p); stop(&lastChair, r->tox, r->toy, r->p, l); hasChair = 1; }
        return r->p;
    }
    return 0;
}
static void popAdd(float x, float y, int delay, const char *t, int life, int pts) {
    for (int i = 0; i < 8; i++) if (pops[i].life <= 0) {
        pops[i].x = x; pops[i].y = y; pops[i].delay = delay; pops[i].life = life; pops[i].pts = pts;
        strncpy(pops[i].txt, t, 15); pops[i].txt[15] = 0; return;
    }
}
static void gameOver(void) {
    state = ST_OVER;
    newBest = score > best();
    if (newBest) sv.ballBest[mach] = score;
    if (score > sv.arcadeBest[ARC_BALL]) sv.arcadeBest[ARC_BALL] = score;
    sv.arcadePlays[ARC_BALL]++;
    int c = score / M()->coinDiv; if (c > 40) c = 40;
    int before = sv.coins; if (c > 0) addCoins(c);
    coinsWon = sv.coins - before;
    saveWrite();
    screen = S_BALL_OVER;
}

// ── the website's step(), once per frame ─────────────────────────────────
void updateBall(void) {
    if (state == ST_ROLL) {
        ball.t++; ball.spin += 0.4f;
        if (!ball.caught) {                              // chairs catch the marker the moment its path touches them
            float k = ease(ball.t >= ball.dur ? 1 : ball.t / (float)ball.dur);
            float cx = ball.x0 + (ball.x1 - ball.x0) * k, cy = ball.y0 + (ball.y1 - ball.y0) * k;
            for (int i = 0; i < M()->nsh; i++) { const Shape *r = &M()->sh[i];
                if (r->chair && inShape(r, cx, cy)) { ball.x1 = cx; ball.y1 = cy; ball.t = ball.dur; ball.caught = 1; break; } }
        }
        if (ball.t >= ball.dur) {
            ball.t = ball.dur;
            int pts = scoreLanding(ball.x1, ball.y1); resolved++;
            if (pts > 0 || hasChair) sfxPlop();
            if (hasChair) pendingScore = pts; else score += pts;   // a chair's points arrive when the climb lands
            int left = hopN - hopAt, hops = (hasChair ? 1 : 0) + left;
            char t[16];
            if (hasChair) {
                const Stop *e = left ? &hopQ[hopN - 1] : &lastChair;
                if (left) sprintf(t, "+%d", pts); else if (pts > 0) strcpy(t, lastChair.label); else strcpy(t, "MISS");
                popAdd(e->x, e->y, HOP_T * hops, t, 80, pts);
                hop.x0 = ball.x1; hop.y0 = ball.y1; hop.x1 = lastChair.x; hop.y1 = lastChair.y; hop.t = 0; hop.on = 1;
            } else {
                if (pts > 0) sprintf(t, "+%d", pts); else strcpy(t, "MISS");
                popAdd(ball.x1, ball.y1, 0, t, 52, pts);
            }
            state = ST_LANDED; landT = 0;
        }
    } else if (state == ST_LANDED) {
        landT++;
        if (hop.on) {
            ball.spin += 0.55f;
            if (++hop.t >= HOP_T) {
                ball.x1 = hop.x1; ball.y1 = hop.y1; sfxPlop();
                if (hopAt < hopN) { Stop *n = &hopQ[hopAt++]; hop.x0 = ball.x1; hop.y0 = ball.y1; hop.x1 = n->x; hop.y1 = n->y; hop.t = 0; }
                else { hop.on = 0; landT = 0; score += pendingScore; pendingScore = 0; }
            }
        }
        if (landT >= LAND_HOLD && !hop.on) {
            if (resolved >= TOTAL) gameOver();
            else { curIdx++; feed.t = 0; feed.fromX = qx(curIdx); feed.col = colOf(curIdx); state = ST_FEED; }
        }
    } else if (state == ST_FEED) {
        if (++feed.t >= FEED_DUR) state = ST_READY;
    }
    for (int i = 0; i < 8; i++) { Pop *p = &pops[i]; if (p->life <= 0) continue;
        if (p->delay > 0) { p->delay--; continue; }
        p->life--; p->y -= 0.0015f; }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputBall(void) {
    if (screen == S_BALL_PAUSE) {
        if (kDown & (KEY_START | KEY_A)) screen = S_BALL;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_BALL_PAUSE; charging = 0; return; }
    if ((kDown & (KEY_L | KEY_R)) && state == ST_READY && resolved == 0) {     // change machine before the first roll
        startBall((mach + ((kDown & KEY_R) ? 1 : 2)) % 3); return;
    }
    // aim with left/right, hold A (the power marker grows and shrinks), let go to roll
    if (canRoll()) {
        if (kHeld & KEY_LEFT)  { aim -= 0.025f; if (aim < -1) aim = -1; chargeT = 90; }
        if (kHeld & KEY_RIGHT) { aim += 0.025f; if (aim > 1) aim = 1; chargeT = 90; }
        if (kDown & KEY_A) { charging = 1; ph = 0; power = 0; }
    }
    if (charging) {
        ph++; float p = (ph % 44) / 22.0f; power = p < 1 ? p : 2 - p;
        chargeT = 90;
        if (kDown & KEY_B) charging = 0;
        else if (kUp & KEY_A) { charging = 0; if (motionTap) power = motionPower; if (canRoll()) roll(power, aim); }
    }
    if (chargeT > 0 && !charging) chargeT--;
}

// ── sprites: the main game's three markers (red, green, blue) and a shadow ──
enum { T_BR = 512, T_BG = 576, T_BB = 640, T_BSH = 704, T_BQ = 712 };
// (the queue uses small plain pictures: the GBA can only draw so many sprite pixels per line)
void ballObjLoad(void) {
    platObjTiles(T_BR, obj_mk_red, 2048); platObjTiles(T_BG, obj_mk_green, 2048); platObjTiles(T_BB, obj_mk_blue, 2048);
    platObjTiles(T_BSH, mm_shadow, 256);
    memcpy(&objPal[0], obj_mk_red_pal, 32); memcpy(&objPal[16], obj_mk_green_pal, 32); memcpy(&objPal[32], obj_mk_blue_pal, 32);
    objPal[48 + 1] = 0;
    const u8 *Q[3] = { bq_0, bq_1, bq_2 }; const u16 *QP[3] = { bq_0_pal, bq_1_pal, bq_2_pal };
    for (int i = 0; i < 3; i++) { platObjTiles(T_BQ + i * 4, Q[i], 128); memcpy(&objPal[(4 + i) * 16], QP[i], 32); }
}
static int nObj;
static int nPlain;
static void aff(int tile, int pal, int shape, int size, int w, int h, int cx, int cy, float sx, float sy, float ang, int see) {
    if (nObj >= 32 || sx < 0.02f || sy < 0.02f) return;
    int i = nObj++;
    float c = fcos(ang), s = fsin(ang);
    oam[i * 4].a3 = (u16)(s16)(c / sx * 256); oam[i * 4 + 1].a3 = (u16)(s16)(s / sx * 256);
    oam[i * 4 + 2].a3 = (u16)(s16)(-s / sy * 256); oam[i * 4 + 3].a3 = (u16)(s16)(c / sy * 256);
    oam[i].a0 = (u16)(((cy - h) & 0xFF) | 0x0300 | (see ? 0x0400 : 0) | (shape << 14));
    oam[i].a1 = (u16)(((cx - w) & 0x1FF) | (i << 9) | (size << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
// a plain 16x16 sprite (after the turned ones in the table, so drawn under them)
static void plain16(int tile, int pal, int cx, int cy, int see) {
    int i = 32 + nPlain++; if (i >= 128) return;
    oam[i].a0 = (u16)(((cy - 8) & 0xFF) | (see ? 0x0400 : 0));
    oam[i].a1 = (u16)(((cx - 8) & 0x1FF) | (1 << 14));
    oam[i].a2 = (u16)(tile | (pal << 12));
}
// the DS draws each marker 0.11 of the cabinet's width across (blue, the long one, shrunk to that
// too); the sprite pictures are the main game's: green and red 40 px across, blue 64
static void markerAt(int col, float fx, float fy, float scale, float rot, int see) {
    float w = 0.11f * BW * scale * BS;
    static const int T[3] = { T_BR, T_BG, T_BB };
    float k = w / (col == 2 ? 64.0f : 40.0f);
    aff(T[col], col, 0, 3, 64, 64, GX(fx), GY(fy), k, k, rot, see);
}

// ── drawing ───────────────────────────────────────────────────────────────
#define PX0 (BB_W + 1)
#define PW (SW - PX0)
static void textSO(int x, int y, const char *t, int c) { textS(x + 1, y + 1, t, C_BLACK); textS(x, y, t, c); }   // small, with a shadow
void drawBall(void) {
    const Machine *f = M(); char s[24];
    drawDirect();
    for (int y = 0; y < SH; y++) {                        // the machine, a divider, the panel
        copy32(&page[y * (SW / 2)], &bb_field[mach][y * BB_W], BB_W / 4);
        fill32(&page[y * (SW / 2) + BB_W / 2], C_PANEL * 0x01010101u, (SW - BB_W) / 4);
    }
    rect(BB_W, 0, 1, SH, C_PANELLINE);
    clipX0 = 0; clipX1 = BB_W; clipY0 = 0; clipY1 = SH;
    if (mach == 2) textSO(GX(0.510f) - textSW("33K") / 2, GY(0.218f) - 5, "33K", C_GOLD);   // the cabinet's unlabelled jackpot
    // (the cabinet's own MARKERS / SCORE / HIGH SCORE boxes are too small to read at this size:
    //  the panel shows them instead)
    hideSprites(); nObj = 0; nPlain = 0;
    int showAim = state == ST_READY && (charging || chargeT > 0) && screen == S_BALL;
    if (state == ST_READY) {
        markerAt(colOf(curIdx), f->sx, f->sy, 1.0f, 0, 0);
        if (showAim) {                                   // the power marker, and where it's headed
            float p = charging ? power : 0.5f;
            float tx = f->cx + aim * f->aimSpan, ty = f->baseY - p * f->powY; if (ty < f->topY) ty = f->topY;
            int x0 = GX(f->sx), y0 = GY(f->sy), x1 = GX(tx), y1 = GY(ty);
            float dx = x1 - x0, dy = y1 - y0, len = fsqrt(dx * dx + dy * dy); if (len < 1) len = 1;
            powerMarker(x0, y0, dx / len, dy / len, 14 + (len - 14) * (charging ? 1.0f : 0.35f), charging ? p : 0);
            for (int a = -3; a <= 3; a++) { pset(x1 + a, y1, YELLOW); pset(x1, y1 + a, YELLOW); }
        }
    }
    if (state == ST_ROLL || state == ST_LANDED) {
        float t = ease(ball.t >= ball.dur ? 1 : ball.t / (float)ball.dur);
        float cx = ball.x0 + (ball.x1 - ball.x0) * t, cy = ball.y0 + (ball.y1 - ball.y0) * t, sc = 1 - t * f->shrink, al = 1;
        int lift = 0;
        if (hop.on) {
            float k = hop.t / (float)HOP_T, e = k * k * (3 - 2 * k), sn = fsin(k * 3.14159265f);
            cx = hop.x0 + (hop.x1 - hop.x0) * e; cy = hop.y0 + (hop.y1 - hop.y0) * e;
            lift = (int)(sn * BPH * 0.10f); sc = (1 - f->shrink) * (1.15f + sn * 0.55f);
        } else if (state == ST_LANDED) {                 // sinks into the hole and fades
            float k = landT >= LAND_HOLD ? 1 : landT / (float)LAND_HOLD;
            al = k < 0.55f ? 1 : 1 - (k - 0.55f) / 0.45f; sc *= 1 - 0.45f * k; cy += 1.7f * 2.56f * k / BPH;
        }
        if (al > 0.2f) {
            markerAt(ball.col, cx, cy - lift / BPH, sc, ball.spin, al < 0.6f);
            int x = GX(cx), y = GY(cy), oy = (int)((5.1f * sc + lift) * BS);
            float rx = 12.8f * sc * BS, ry = (5.1f * sc + 1) * BS;
            aff(T_BSH, 3, 1, 2, 32, 16, x, y + oy, rx * 2 / 32, ry * 2 / 16, 0, 1);   // its shadow on the lane
        }
    }
    // the queue: every marker keeps its own slot, faded
    for (int k = curIdx + 1; k < TOTAL; k++) plain16(T_BQ + colOf(k) * 4, 4 + colOf(k), GX(qx(k)), GY(f->queueY), 1);
    if (state == ST_FEED) { float t = ease(feed.t / (float)FEED_DUR);
        markerAt(feed.col, feed.fromX + (f->sx - feed.fromX) * t, f->queueY + (f->sy - f->queueY) * t, 0.5f + 0.5f * t, 0, t < 0.5f); }
    for (int i = 0; i < 8; i++) { Pop *p = &pops[i]; if (p->life <= 0 || p->delay > 0 || p->life < 8) continue;
        int c = p->pts >= 100 ? C_GOLD : p->pts >= 30 ? LIME : p->pts > 0 ? C_LBLUE : RED;
        textSO(GX(p->x) - textSW(p->txt) / 2, GY(p->y) - 5, p->txt, c); }
    clipAll();
    // the panel
    clipX0 = PX0;
    textCW(PX0, PW, 2, f->name, GOLD, 1);
    char b[12];
    textS(PX0 + 6, 22, "SCORE", GREY); fmtK(b, score); textCW(PX0, PW, 32, b, YELLOW, 2);
    sprintf(s, "MARKERS  %d", TOTAL - resolved); textS(PX0 + 6, 64, s, WHITE);
    fmtK(b, best()); sprintf(s, "BEST  %s", b); textS(PX0 + 6, 76, s, newBest && state == ST_OVER ? LIME : GREY);
    rect(PX0 + 4, 92, PW - 8, 1, C_PANELLINE);
    if (state == ST_READY && screen == S_BALL) {
        textS(PX0 + 6, 98, "left/right: aim", GREY);
        textS(PX0 + 6, 109, "hold A, let go: roll", GREY);
        if (resolved == 0) { textS(PX0 + 6, 126, "1 / 2: machine", GOLD); }
        else textS(PX0 + 6, 126, "soft & wide for pockets", GREY);
    }
    sprintf(s, "coins  %d", sv.coins); textS(PX0 + 6, 146, s, GOLD);     // (no coin picture: the machine's colours are loaded)
    clipAll();
    if (screen == S_BALL_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── the menu before a game, and the one after ─────────────────────────────
static int bmSel, boSel;
static Btn BB[4];
static void bmLayout(void) {
    for (int i = 0; i < 3; i++) { BB[i] = (Btn){ 136, 22 + i * 32, 100, 26, "", 0, 0 }; strcpy(BB[i].label, MACH[i].name); }
    BB[3] = (Btn){ 136, 118, 100, 26, "BACK", 0, 0 };
}
void drawBallMenu(void) {
    char s[40];
    bmLayout();
    fillScreen(DARK);
    text(6, 2, "QU33PH-BALL", GOLD, 1);
    static const char *L[7] = { "9 markers a game", "Roll it up the lane;", "soft & wide for pockets.", "Chairs lob it to the 75s.",
        "CHAIR TOWER chairs climb", "for 5K, 20K, 33K MEGA!", "" };
    static const int C[7] = { GREY, WHITE, WHITE, WHITE, WHITE, C_GOLD, 0 };
    for (int i = 0; i < 6; i++) textS(6, 20 + i * 11, L[i], C[i]);
    for (int i = 0; i < 3; i++) { char b[10]; fmtK(b, sv.ballBest[i]); sprintf(s, "%s  %s", MACH[i].name, b);
        textS(6, 94 + i * 11, s, i == bmSel ? YELLOW : GREY); }
    coinCount(6, 140, 0);
    drawBtns(BB, 4, bmSel);
}
void inputBallMenu(void) {
    bmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(BB, 4, &bmSel, 1);
    if (h >= 0 && h < 3) startBall(h);
    if (h == 3) goScreen(S_ARCADE);
}
static void boLayout(void) {
    BB[0] = (Btn){ 30, 90, 180, 22, "PLAY AGAIN", 0, 0 }; BB[1] = (Btn){ 30, 114, 88, 22, "MACHINES", 0, 0 };
    BB[2] = (Btn){ 122, 114, 88, 22, "ARCADE", 0, 0 };
}
void drawBallOver(void) {
    char s[40], b[12];
    drawBall();
    hideSprites();
    box(20, 12, 200, 132, C_TOAST, GOLD);
    textC(16, "GAME OVER", RED, 2);
    sprintf(s, "SCORE %d", score); textC(46, s, WHITE, 1);
    fmtK(b, best()); sprintf(s, "%s BEST %s%s", M()->name, b, newBest ? "  NEW!" : ""); textC(60, s, newBest ? LIME : GREY, 1);
    sprintf(s, "%d COINS EARNED", coinsWon); textC(74, s, GOLD, 1);
    boLayout();
    drawBtns(BB, 3, boSel);
}
void inputBallOver(void) {
    boLayout();
    int h = btnInput(BB, 3, &boSel, 1);
    if (kDown & KEY_B) h = 2;
    if (h == 0) startBall(mach);
    if (h == 1) goScreen(S_BALL_MENU);
    if (h == 2) goScreen(S_ARCADE);
}
int ballMachine(void) { return mach; }
