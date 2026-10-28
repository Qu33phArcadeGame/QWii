// stack.c — QU33PH STACK (the website's stack.html): drop markers into a tower.
//
// The DS version's own little rigid-body engine (a C port of Box2D-Lite), as the DS plays it,
// with two changes for the GBA (which has no floating-point hardware): only the plate, the
// newest markers and the one you hold take part in the physics (everything lower is locked in
// place anyway), and the solver makes 5 passes instead of 7. The field is 5/8 of the DS's
// size in a column on the left; the camera follows the tower up. The markers are hardware
// sprites: pictures for upright and lying flat, turned for anything in between.
//
// CONTROLS: D-pad moves the marker, A drops it, L/R turn it upright / flat. START pauses
// (SELECT quits to the arcade).
#include "qu.h"

#define CW 256.0f
#define CH 384.0f
#define ML 92.0f                                   // marker length (the website's WW*0.36)
#define MW 29.0f                                   // WW*0.115
#define PLATE_W 102.0f                             // WW*0.40
#define PLATE_Y 346.0f                             // CH*0.90
#define VH 256.0f                                  // the DS-sized height the GBA screen shows (160 / 0.625)
#define GRAV 1050.0f                                // px/s^2: about twice the website's pull, so the DS plays snappier
#define LOOSE 4                                    // how many of the top placed markers stay loose
#define MAXB 160

// ── a little Box2D-Lite ──────────────────────────────────────────────────
typedef struct { float x, y; } V2;
static V2 v2(float x, float y) { V2 r = { x, y }; return r; }
static V2 vadd(V2 a, V2 b) { return v2(a.x + b.x, a.y + b.y); }
static V2 vsub(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
static V2 vmul(V2 a, float s) { return v2(a.x * s, a.y * s); }
static float dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
static float crossVV(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }
static V2 crossSV(float s, V2 a) { return v2(-s * a.y, s * a.x); }
static float fabs_(float v) { return v < 0 ? -v : v; }
typedef struct { V2 c1, c2; } M22;                // columns
static M22 rotm(float a) { float c = fcos(a), s = fsin(a); M22 m = { { c, s }, { -s, c } }; return m; }
static M22 tr(M22 m) { M22 t = { { m.c1.x, m.c2.x }, { m.c1.y, m.c2.y } }; return t; }
static V2 mv(M22 m, V2 v) { return v2(m.c1.x * v.x + m.c2.x * v.y, m.c1.y * v.x + m.c2.y * v.y); }
static M22 mm(M22 a, M22 b) { M22 r = { mv(a, b.c1), mv(a, b.c2) }; return r; }
static M22 mabs(M22 a) { M22 r = { { fabs_(a.c1.x), fabs_(a.c1.y) }, { fabs_(a.c2.x), fabs_(a.c2.y) } }; return r; }

typedef struct {
    V2 p, v; float a, w, hw, hh, invM, invI, fric; int col, isStatic, used, settle, dropped, held;
} Body;
typedef union { struct { u8 in1, out1, in2, out2; } e; u32 key; } Feat;
typedef struct { V2 pos, n, r1, r2; float sep, Pn, Pt, mN, mT, bias; Feat f; } Contact;
typedef struct { int a, b, n; Contact c[2]; float fric; } Arb;
// (these tables are big: they live in the GBA's 256 KB work RAM, not its 32 KB fast RAM)
static Body B[MAXB] EWRAM_BSS; static int nB;
static Arb arbs[96] EWRAM_BSS, arbsOld[96] EWRAM_BSS; static int nArb, nArbOld;
enum { NOE, E1, E2, E3, E4 };
typedef struct { V2 v; Feat f; } CV;

static void setMass(Body *b, int isStatic) {
    b->isStatic = isStatic;
    if (isStatic) { b->invM = b->invI = 0; return; }
    float m = (b->hw * 2) * (b->hh * 2) * 0.004f;              // the website's density
    b->invM = 1 / m; b->invI = 1 / (m * ((b->hw * 2) * (b->hw * 2) + (b->hh * 2) * (b->hh * 2)) / 12);
}
static int clipLine(CV out[2], CV in[2], V2 n, float off, u8 edge) {
    int k = 0; float d0 = dot(n, in[0].v) - off, d1 = dot(n, in[1].v) - off;
    if (d0 <= 0) out[k++] = in[0];
    if (d1 <= 0) out[k++] = in[1];
    if (d0 * d1 < 0) {
        float t = d0 / (d0 - d1);
        out[k].v = vadd(in[0].v, vmul(vsub(in[1].v, in[0].v), t));
        if (d0 > 0) { out[k].f = in[0].f; out[k].f.e.in1 = edge; out[k].f.e.in2 = NOE; }
        else { out[k].f = in[1].f; out[k].f.e.out1 = edge; out[k].f.e.out2 = NOE; }
        k++;
    }
    return k;
}
static void incident(CV c[2], V2 h, V2 pos, M22 R, V2 n0) {
    V2 n = mv(tr(R), n0); n.x = -n.x; n.y = -n.y;
    if (fabs_(n.x) > fabs_(n.y)) {
        if (n.x > 0) { c[0].v = v2(h.x, -h.y); c[0].f.e.in2 = E3; c[0].f.e.out2 = E4; c[1].v = v2(h.x, h.y); c[1].f.e.in2 = E4; c[1].f.e.out2 = E1; }
        else { c[0].v = v2(-h.x, h.y); c[0].f.e.in2 = E1; c[0].f.e.out2 = E2; c[1].v = v2(-h.x, -h.y); c[1].f.e.in2 = E2; c[1].f.e.out2 = E3; }
    } else {
        if (n.y > 0) { c[0].v = v2(h.x, h.y); c[0].f.e.in2 = E4; c[0].f.e.out2 = E1; c[1].v = v2(-h.x, h.y); c[1].f.e.in2 = E1; c[1].f.e.out2 = E2; }
        else { c[0].v = v2(-h.x, -h.y); c[0].f.e.in2 = E2; c[0].f.e.out2 = E3; c[1].v = v2(h.x, -h.y); c[1].f.e.in2 = E3; c[1].f.e.out2 = E4; }
    }
    c[0].f.e.in1 = c[0].f.e.out1 = c[1].f.e.in1 = c[1].f.e.out1 = NOE;
    c[0].v = vadd(pos, mv(R, c[0].v)); c[1].v = vadd(pos, mv(R, c[1].v));
}
static int collide(Contact *out, Body *A, Body *Bb) {
    V2 hA = v2(A->hw, A->hh), hB = v2(Bb->hw, Bb->hh);
    M22 RA = rotm(A->a), RB = rotm(Bb->a), RAT = tr(RA), RBT = tr(RB);
    V2 dp = vsub(Bb->p, A->p), dA = mv(RAT, dp), dB = mv(RBT, dp);
    M22 C = mm(RAT, RB), aC = mabs(C), aCT = tr(aC);
    V2 t1 = mv(aC, hB), fA = v2(fabs_(dA.x) - hA.x - t1.x, fabs_(dA.y) - hA.y - t1.y);
    if (fA.x > 0 || fA.y > 0) return 0;
    V2 t2 = mv(aCT, hA), fB = v2(fabs_(dB.x) - t2.x - hB.x, fabs_(dB.y) - t2.y - hB.y);
    if (fB.x > 0 || fB.y > 0) return 0;
    int axis = 0; float sep = fA.x; V2 n = dA.x > 0 ? RA.c1 : vmul(RA.c1, -1);
    const float rt = 0.95f, at = 0.01f;
    if (fA.y > rt * sep + at * hA.y) { axis = 1; sep = fA.y; n = dA.y > 0 ? RA.c2 : vmul(RA.c2, -1); }
    if (fB.x > rt * sep + at * hB.x) { axis = 2; sep = fB.x; n = dB.x > 0 ? RB.c1 : vmul(RB.c1, -1); }
    if (fB.y > rt * sep + at * hB.y) { axis = 3; sep = fB.y; n = dB.y > 0 ? RB.c2 : vmul(RB.c2, -1); }
    V2 fn, sn; CV ie[2]; float front, negS, posS, side; u8 negE, posE;
    if (axis == 0) { fn = n; front = dot(A->p, fn) + hA.x; sn = RA.c2; side = dot(A->p, sn); negS = -side + hA.y; posS = side + hA.y; negE = E3; posE = E1; incident(ie, hB, Bb->p, RB, fn); }
    else if (axis == 1) { fn = n; front = dot(A->p, fn) + hA.y; sn = RA.c1; side = dot(A->p, sn); negS = -side + hA.x; posS = side + hA.x; negE = E2; posE = E4; incident(ie, hB, Bb->p, RB, fn); }
    else if (axis == 2) { fn = vmul(n, -1); front = dot(Bb->p, fn) + hB.x; sn = RB.c2; side = dot(Bb->p, sn); negS = -side + hB.y; posS = side + hB.y; negE = E3; posE = E1; incident(ie, hA, A->p, RA, fn); }
    else { fn = vmul(n, -1); front = dot(Bb->p, fn) + hB.y; sn = RB.c1; side = dot(Bb->p, sn); negS = -side + hB.x; posS = side + hB.x; negE = E2; posE = E4; incident(ie, hA, A->p, RA, fn); }
    CV c1[2], c2[2];
    if (clipLine(c1, ie, vmul(sn, -1), negS, negE) < 2) return 0;
    if (clipLine(c2, c1, sn, posS, posE) < 2) return 0;
    int k = 0;
    for (int i = 0; i < 2; i++) {
        float s = dot(fn, c2[i].v) - front;
        if (s <= 0) {
            out[k].sep = s; out[k].n = n; out[k].pos = vsub(c2[i].v, vmul(fn, s)); out[k].f = c2[i].f;
            if (axis >= 2) { u8 t = out[k].f.e.in1; out[k].f.e.in1 = out[k].f.e.in2; out[k].f.e.in2 = t; t = out[k].f.e.out1; out[k].f.e.out1 = out[k].f.e.out2; out[k].f.e.out2 = t; }
            out[k].Pn = out[k].Pt = 0; k++;
        }
    }
    return k;
}
static void applyP(Body *b1, Body *b2, V2 r1, V2 r2, V2 P) {
    b1->v = vsub(b1->v, vmul(P, b1->invM)); b1->w -= b1->invI * crossVV(r1, P);
    b2->v = vadd(b2->v, vmul(P, b2->invM)); b2->w += b2->invI * crossVV(r2, P);
}
static float physGrav = GRAV;
static void step(float dt) {
    // broad phase: only pairs with a loose body, whose boxes come near each other
    memcpy(arbsOld, arbs, sizeof(Arb) * nArb); nArbOld = nArb; nArb = 0;
    int act[24], na = 0;                                      // (the GBA has no float hardware: only these take part)
    for (int i = 0; i < nB && na < 24; i++) if (B[i].used && (i == 0 || i >= nB - 16)) act[na++] = i;
    for (int ii = 0; ii < na; ii++) { int i = act[ii]; Body *a = &B[i];
        for (int jj = ii + 1; jj < na; jj++) { int j = act[jj]; Body *b = &B[j]; if (a->invM == 0 && b->invM == 0) continue;
            float ra = a->hw + a->hh, rb = b->hw + b->hh;
            if (fabs_(a->p.x - b->p.x) > ra + rb || fabs_(a->p.y - b->p.y) > ra + rb) continue;
            Contact cs[2]; int n = collide(cs, a, b);
            if (!n || nArb >= 96) continue;
            Arb *ar = &arbs[nArb++]; ar->a = i; ar->b = j; ar->n = n; ar->fric = fsqrt(a->fric * b->fric);
            for (int k = 0; k < n; k++) {                    // warm start from last step's matching contact
                ar->c[k] = cs[k];
                for (int o = 0; o < nArbOld; o++) if (arbsOld[o].a == i && arbsOld[o].b == j)
                    for (int q = 0; q < arbsOld[o].n; q++) if (arbsOld[o].c[q].f.key == cs[k].f.key) { ar->c[k].Pn = arbsOld[o].c[q].Pn; ar->c[k].Pt = arbsOld[o].c[q].Pt; }
            }
        } }
    for (int i = 0; i < nB; i++) { Body *b = &B[i]; if (!b->used || b->invM == 0) continue; b->v.y += dt * physGrav; }
    float inv = 1 / dt;
    for (int k = 0; k < nArb; k++) { Arb *ar = &arbs[k]; Body *b1 = &B[ar->a], *b2 = &B[ar->b];
        for (int i = 0; i < ar->n; i++) { Contact *c = &ar->c[i];
            c->r1 = vsub(c->pos, b1->p); c->r2 = vsub(c->pos, b2->p);
            float rn1 = dot(c->r1, c->n), rn2 = dot(c->r2, c->n);
            c->mN = 1 / (b1->invM + b2->invM + b1->invI * (dot(c->r1, c->r1) - rn1 * rn1) + b2->invI * (dot(c->r2, c->r2) - rn2 * rn2));
            V2 t = v2(c->n.y, -c->n.x); float rt1 = dot(c->r1, t), rt2 = dot(c->r2, t);
            c->mT = 1 / (b1->invM + b2->invM + b1->invI * (dot(c->r1, c->r1) - rt1 * rt1) + b2->invI * (dot(c->r2, c->r2) - rt2 * rt2));
            float s = c->sep + 0.5f; c->bias = s < 0 ? -0.2f * inv * s : 0;
            applyP(b1, b2, c->r1, c->r2, vadd(vmul(c->n, c->Pn), vmul(t, c->Pt)));
        } }
    for (int it = 0; it < 5; it++)                                    // 5 solver passes (the DS does 7): quick enough for the GBA
        for (int k = 0; k < nArb; k++) { Arb *ar = &arbs[k]; Body *b1 = &B[ar->a], *b2 = &B[ar->b];
            for (int i = 0; i < ar->n; i++) { Contact *c = &ar->c[i];
                V2 dv = vsub(vadd(b2->v, crossSV(b2->w, c->r2)), vadd(b1->v, crossSV(b1->w, c->r1)));
                float dPn = c->mN * (-dot(dv, c->n) + c->bias), P0 = c->Pn;
                c->Pn = P0 + dPn > 0 ? P0 + dPn : 0; dPn = c->Pn - P0;
                applyP(b1, b2, c->r1, c->r2, vmul(c->n, dPn));
                dv = vsub(vadd(b2->v, crossSV(b2->w, c->r2)), vadd(b1->v, crossSV(b1->w, c->r1)));
                V2 t = v2(c->n.y, -c->n.x);
                float dPt = c->mT * (-dot(dv, t)), mx = ar->fric * c->Pn, T0 = c->Pt, Tn = T0 + dPt;
                c->Pt = Tn < -mx ? -mx : Tn > mx ? mx : Tn; dPt = c->Pt - T0;
                applyP(b1, b2, c->r1, c->r2, vmul(t, dPt));
            } }
    for (int i = 0; i < nB; i++) { Body *b = &B[i]; if (!b->used || b->invM == 0) continue;
        b->v = vmul(b->v, 0.97f); b->w *= 0.97f;                       // the website's air drag (0.03)
        b->p = vadd(b->p, vmul(b->v, dt)); b->a += b->w * dt; }
}

// ── the game ──────────────────────────────────────────────────────────────
enum { ST_PLAY, ST_OVER };
static int placed[MAXB] EWRAM_BSS;
static int state, score, overT, cur = -1, nPlaced, horizontal, coinsWon, newBest, overSel, menuSel;
static float camY, camScale = 1; static V2 target; static int haveTarget;
static int newBody(float x, float y, float w, float h, int isStatic, int col) {
    int i = nB < MAXB ? nB++ : MAXB - 1;
    Body *b = &B[i]; memset(b, 0, sizeof *b);
    b->used = 1; b->p = v2(x, y); b->hw = w / 2; b->hh = h / 2; b->fric = 1.0f; b->col = col;
    setMass(b, isStatic);
    return i;
}
static float baseAngle(void) { return horizontal ? 1.5707963f : 0; }
static float topY(void) { float t = PLATE_Y; for (int i = 0; i < nPlaced; i++) if (B[placed[i]].p.y < t) t = B[placed[i]].p.y; return t; }
static void spawn(void) {
    int col = nPlaced % 3;
    float y = topY() - ML * 1.4f;
    // the website shrinks the box onto the visible marker (94% of its width, 98% of its length)
    float sw = col == 0 ? 17 : col == 1 ? 16 : 13, bw = sw * 0.94f;        // (the DS's marker pictures' widths) if (bw < 11.5f) bw = 11.5f;
    cur = newBody(CW / 2, y, bw, ML * 0.98f, 1, col);
    B[cur].a = baseAngle(); B[cur].held = 0; B[cur].dropped = 0;
    haveTarget = 0;
}
static void startGame(void) {
    nB = 0; nArb = 0; nPlaced = 0; score = 0; camY = 0; camScale = 1; state = ST_PLAY; overT = 0; physGrav = GRAV;
    newBody(CW / 2, PLATE_Y + MW * 0.4f, PLATE_W, MW * 0.8f, 1, -1);     // the plate
    spawn(); camY = VH - PLATE_Y - 34; target = B[cur].p; haveTarget = 0;
    screen = S_STACK;
}
static void gameOver(void) {
    if (state == ST_OVER) return;
    state = ST_OVER; overT = 50;
    // let the top of the tower come down (the rest stays where it was buried)
    int from = nPlaced - 8 < 0 ? 0 : nPlaced - 8;
    for (int i = from; i < nPlaced; i++) setMass(&B[placed[i]], 0);
    if (cur >= 0 && !B[cur].dropped) setMass(&B[cur], 0);
    for (int i = 0; i < 4; i++) sfxPlop();
    newBest = score > sv.arcadeBest[ARC_STACK];
    if (newBest) sv.arcadeBest[ARC_STACK] = score;
    sv.arcadePlays[ARC_STACK]++;
    int c = score / 2; if (c > 20) c = 20;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite();
}
static float clampX(float x) { float h = (horizontal ? ML : MW) * 0.5f; return x < h ? h : x > CW - h ? CW - h : x; }
void updateStack(void) {
    if (state == ST_PLAY && cur >= 0 && B[cur].held && haveTarget) {        // the held marker follows your finger
        Body *b = &B[cur];
        b->p.x += (target.x - b->p.x) * 0.45f; b->p.y += (target.y - b->p.y) * 0.45f;   // follows your finger quickly
        b->a = baseAngle(); b->v = v2(0, 0); b->w = 0;
    }
    physGrav = state == ST_OVER ? GRAV * 2.5f : GRAV;
    step(1.0f / 60);
    if (state == ST_PLAY && cur >= 0 && B[cur].dropped) {
        Body *b = &B[cur];
        if (b->p.y > PLATE_Y + ML * 0.8f) gameOver();
        else {
            float sp = fsqrt(b->v.x * b->v.x + b->v.y * b->v.y) / 21.6f + fabs_(b->w) * 0.5f;
            if (sp < 0.4f) b->settle++; else b->settle = 0;
            if (b->settle > 4) {                                   // settled: next marker straight away
                placed[nPlaced++] = cur; score = nPlaced; sfxPlop();
                if (nPlaced > LOOSE) setMass(&B[placed[nPlaced - LOOSE - 1]], 1);   // buried: lock it in place
                cur = -1; spawn();
            }
        }
    }
    if (state == ST_PLAY)
        for (int i = 0; i < nPlaced; i++) { Body *b = &B[placed[i]];
            if (b->p.y > PLATE_Y + ML * 1.1f || b->p.x < CW / 2 - PLATE_W - ML || b->p.x > CW / 2 + PLATE_W + ML) { gameOver(); break; } }
    float ty = topY(); if (cur >= 0 && B[cur].dropped && B[cur].p.y < ty) ty = B[cur].p.y;
    if (state == ST_OVER) {                                                    // pull back to see the whole fall
        float mn = ty, mx = PLATE_Y;
        for (int i = 0; i < nB; i++) if (B[i].used) { if (B[i].p.y < mn) mn = B[i].p.y; if (B[i].p.y > mx && B[i].p.y < PLATE_Y + 300) mx = B[i].p.y; }
        float Hf = mx - mn + ML * 2.4f; if (Hf < VH * 0.5f) Hf = VH * 0.5f;
        float ts = VH * 0.82f / Hf; ts = ts < 0.35f ? 0.35f : ts > 1 ? 1 : ts;
        camScale += (ts - camScale) * 0.15f; camY += ((VH * 0.5f - (mn + mx) / 2 * camScale) - camY) * 0.15f;
        if (overT > 0 && --overT == 0) screen = S_STACK_OVER;
    } else {
        // keep the top of the tower about two-thirds down the screen, so the marker you're
        // holding (which starts above it) is in view; never past the plate at the bottom
        float t = VH * 0.65f - ty, lo = VH - PLATE_Y - 34; if (t < lo) t = lo;
        camScale += (1 - camScale) * 0.2f; camY += (t - camY) * 0.2f;
    }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputStack(void) {
    if (screen == S_STACK_PAUSE) {
        if (kDown & KEY_START) screen = S_STACK;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_STACK_PAUSE; return; }
    if (state != ST_PLAY || cur < 0 || B[cur].dropped) return;
    Body *b = &B[cur];
    if (kDown & (KEY_L | KEY_R)) { horizontal = !horizontal; b->a = baseAngle(); return; }
    // never let the held marker be pushed down into the tower: keep its underside above the top
    float lowest = PLATE_Y - ML * 0.42f, half = horizontal ? b->hw : b->hh;
    for (int i = 0; i < nPlaced; i++) { Body *p = &B[placed[i]];
        float ext = fabs_(fcos(p->a)) * p->hh + fabs_(fsin(p->a)) * p->hw, top = p->p.y - ext - half - 2;
        if (fabs_(p->p.x - b->p.x) < ext + half + (horizontal ? b->hh : b->hw) && top < lowest) lowest = top; }
    if (kHeld & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)) {          // steer it
        if (!haveTarget) { target = b->p; haveTarget = 1; }
        b->held = 1;
        if (kHeld & KEY_LEFT) target.x -= 4.5f;
        if (kHeld & KEY_RIGHT) target.x += 4.5f;
        if (kHeld & KEY_UP) target.y -= 4.5f;
        if (kHeld & KEY_DOWN) target.y += 4.5f;
        target.x = clampX(target.x); if (target.y > lowest) target.y = lowest;
    }
    if (kDown & KEY_A) {                                                 // let go: it drops
        b->held = 0; b->dropped = 1; setMass(b, 0); b->v = v2(0, 0); b->w = 0;
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
#define SS 0.625f                                        // GBA pixels per DS pixel
#define FW 160                                           // the field column; the panel is the rest
#define PX0 (FW + 1)
#define PW (SW - PX0)
static void sx_(float x, float y, int *ox, int *oy) { *ox = (int)((CW / 2 + (x - CW / 2) * camScale) * SS); *oy = (int)((y * camScale + camY) * SS); }
// tiles: upright markers (32x64) and lying ones (64x32), red / green / blue; palettes 0-2, 3-5
enum { T_SV = 512, T_SH = 608 };
void stackObjLoad(void) {
    const u8 *V[3] = { st_v0, st_v1, st_v2 }, *H[3] = { st_h0, st_h1, st_h2 };
    const u16 *VP[3] = { st_v0_pal, st_v1_pal, st_v2_pal }, *HP[3] = { st_h0_pal, st_h1_pal, st_h2_pal };
    for (int i = 0; i < 3; i++) {
        platObjTiles(T_SV + i * 32, V[i], 1024); memcpy(&objPal[i * 16], VP[i], 32);
        platObjTiles(T_SH + i * 32, H[i], 1024); memcpy(&objPal[(3 + i) * 16], HP[i], 32);
    }
}
// The GBA has 32 turn-and-scale tables for all its sprites: markers at (nearly) the same angle
// share one, and once they run out a marker takes the nearest one.
static int nObj, nMat; static s16 matA[32]; static float matAng[32];
static int matFor(float ang) {
    int q = (int)(ang * 40.7437f + (ang < 0 ? -0.5f : 0.5f));             // 256 steps a turn
    for (int i = 0; i < nMat; i++) if (matA[i] == q) return i;
    if (nMat < 32) {
        int i = nMat++; float a = q / 40.7437f, c = fcos(a) / camScale, s = fsin(a) / camScale;
        matA[i] = (s16)q; matAng[i] = a;
        oam[i * 4].a3 = (u16)(s16)(c * 256); oam[i * 4 + 1].a3 = (u16)(s16)(s * 256);
        oam[i * 4 + 2].a3 = (u16)(s16)(-s * 256); oam[i * 4 + 3].a3 = (u16)(s16)(c * 256);
        return i;
    }
    int best = 0; float bd = 99; for (int i = 0; i < 32; i++) { float d = fabs_(matAng[i] - q / 40.7437f); if (d < bd) { bd = d; best = i; } }
    return best;
}
static void stackMarker(int col, int x, int y, float a) {
    if (nObj >= 128 || x < -64 || x > FW + 64 || y < -64 || y > SH + 64) return;
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    // near upright or near flat: that picture, turned a little in its own box (cheap);
    // anything else: the upright picture turned in a double-size box
    float up = a > 1.5707963f ? a - 3.14159265f : a < -1.5707963f ? a + 3.14159265f : a;
    float fl = a - 1.5707963f; while (fl > 1.5707963f) fl -= 3.14159265f; while (fl < -1.5707963f) fl += 3.14159265f;
    int i = nObj++;
    if (fabs_(up) <= 0.35f) {
        int m = matFor(up);
        oam[i].a0 = (u16)(((y - 32) & 0xFF) | 0x0100 | (2 << 14)); oam[i].a1 = (u16)(((x - 16) & 0x1FF) | (m << 9) | (3 << 14));
        oam[i].a2 = (u16)((T_SV + col * 32) | (col << 12));
    } else if (fabs_(fl) <= 0.35f) {
        int m = matFor(fl);
        oam[i].a0 = (u16)(((y - 16) & 0xFF) | 0x0100 | (1 << 14)); oam[i].a1 = (u16)(((x - 32) & 0x1FF) | (m << 9) | (3 << 14));
        oam[i].a2 = (u16)((T_SH + col * 32) | ((3 + col) << 12));
    } else {
        int m = matFor(up);
        oam[i].a0 = (u16)(((y - 64) & 0xFF) | 0x0300 | (2 << 14)); oam[i].a1 = (u16)(((x - 32) & 0x1FF) | (m << 9) | (3 << 14));
        oam[i].a2 = (u16)((T_SV + col * 32) | (col << 12));
    }
}
static void drawPlate(void) {
    int px, py; sx_(CW / 2, PLATE_Y, &px, &py);
    int rx = (int)(PLATE_W / 2 * camScale * SS), ry = (int)(MW * 0.55f * camScale * SS); if (ry < 2) ry = 2;
    for (int j = -ry; j <= ry; j++) {
        float k = 1 - (float)(j * j) / (ry * ry); int hw = (int)(rx * fsqrt(k > 0 ? k : 0));
        int edge = j == -ry || j == ry;
        rect(px - hw, py + j, hw * 2 + 1, 1, edge ? C_DIMEDGE : C_SILVER);
        if (!edge && hw > 1) { pset(px - hw, py + j, C_DIMEDGE); pset(px + hw, py + j, C_DIMEDGE); }
    }
}
void drawStack(void) {
    char s[24];
    // the window backdrop, scrolling slowly (the website's parallax), then the panel
    int off = ((int)(camY * 0.16f * SS) % STW_H + STW_H) % STW_H;
    for (int y = 0; y < SH; y++) {
        drawDirect();
        int sy = ((y - off) % STW_H + STW_H) % STW_H;
        copy32(&page[y * (SW / 2)], &stw8[sy * STW_W], STW_W / 4);
        fill32(&page[y * (SW / 2) + FW / 2], C_PANEL * 0x01010101u, (SW - FW) / 4);
    }
    rect(FW, 0, 1, SH, C_PANELLINE);
    clipX0 = 0; clipX1 = FW; clipY0 = 0; clipY1 = SH;
    drawPlate();
    hideSprites(); nObj = 0; nMat = 0;
    // the held marker first (on top), then the tower from the top down
    if (cur >= 0 && B[cur].used) { int x, y; sx_(B[cur].p.x, B[cur].p.y, &x, &y); stackMarker(B[cur].col, x, y, B[cur].a); }
    for (int i = nB - 1; i >= 0; i--) { Body *b = &B[i]; if (!b->used || b->col < 0 || i == cur) continue;
        int x, y; sx_(b->p.x, b->p.y, &x, &y); stackMarker(b->col, x, y, b->a); }
    if (state == ST_OVER) textCW(0, FW, 30, "TIMBER!", C_GOLD, 2);
    clipAll();
    // the panel
    clipX0 = PX0;
    sprintf(s, "%d", score); textCW(PX0, PW, 4, s, WHITE, 2);
    textS(PX0 + (PW - textSW("markers high")) / 2, 34, "markers high", GREY);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_STACK]); textS(PX0 + (PW - textSW(s)) / 2, 50, s, LIME);
    if (state == ST_PLAY) {
        textS(PX0 + 4, 76, "1 / 2", GREY); textS(PX0 + 32, 76, horizontal ? "FLAT" : "UPRIGHT", C_LBLUE);
        textS(PX0 + 4, 96, "D-pad: move", GREY); textS(PX0 + 4, 107, "A: drop", GREY);
    }
    sprintf(s, "coins %d", sv.coins); textS(PX0 + 4, 148, s, GOLD);
    clipAll();
    if (screen == S_STACK_PAUSE) {
        hideSprites();
        box(20, 44, 200, 72, C_TOAST, GOLD);
        textCW(20, 200, 50, "PAUSED", YELLOW, 2);
        textS(40, 84, "+  resume", WHITE); textS(40, 98, "-  quit to the arcade", WHITE);
    }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn SB[3];
static void smLayout(void) { SB[0] = (Btn){ 136, 40, 100, 30, "START", 0, 0 }; SB[1] = (Btn){ 136, 80, 100, 26, "BACK", 0, 0 }; }
void drawStackMenu(void) {
    char s[32];
    fillScreen(DARK);
    text(6, 2, "QU33PH STACK", GOLD, 1);
    static const char *L[6] = { "Build the tower: each", "marker you land on it is", "one higher. Topple it", "and it's TIMBER!", "", "D-pad move, A drop, 1/2 turn" };
    for (int i = 0; i < 6; i++) if (L[i][0]) textS(6, 22 + i * 11, L[i], i == 5 ? GREY : WHITE);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_STACK]); textS(6, 100, s, LIME);
    coinCount(6, 140, 0);
    smLayout();
    drawBtns(SB, 2, menuSel);
}
void inputStackMenu(void) {
    smLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(SB, 2, &menuSel, 1);
    if (h == 0) startGame();
    if (h == 1) goScreen(S_ARCADE);
}
void drawStackOver(void) {
    char s[32];
    drawStack();
    hideSprites();
    box(20, 16, 200, 128, C_TOAST, GOLD);
    textC(22, "TIMBER!", C_GOLD, 2);
    sprintf(s, "%d markers high", score); textC(54, s, WHITE, 1);
    if (newBest) sprintf(s, "NEW BEST!  +%d coins", coinsWon); else sprintf(s, "BEST %d   +%d coins", sv.arcadeBest[ARC_STACK], coinsWon);
    textC(70, s, newBest ? LIME : GREY, 1);
    SB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; SB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    drawBtns(SB, 2, overSel);
}
void inputStackOver(void) {
    SB[0] = (Btn){ 30, 92, 180, 24, "PLAY AGAIN", 0, 0 }; SB[1] = (Btn){ 30, 118, 180, 22, "ARCADE", 0, 0 };
    int h = btnInput(SB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) startGame();
    if (h == 1) goScreen(S_ARCADE);
}
