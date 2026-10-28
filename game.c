// game.c — the Qu33ph match: the website's rules & physics (unchanged from the DS), drawn for
// the GBA's one screen: the field on the left with a camera that follows the throw, the
// markers as hardware sprites, and a side panel with the time, score and a map of the table.
#include "qu.h"

// ── the website's world (420 x 900) and the realistic field's layout ──────
#define VX0          36.0f
#define PX           0.8f              // GBA pixels per world unit (the field is 11/16 of the DS's)
#define LEFT_EDGE    77.7f
#define RIGHT_WALL   211.9f
#define FIELD_CX     144.8f
#define LAUNCH_Y     877.5f
#define END_LINE_Y   99.0f
#define START_LINE_Y 820.0f
#define REDDOT_X     217.9f
#define REDDOT_Y     159.8f
#define TOUCH_DIST   45.0f

typedef struct { float x, y, vx, vy, curve, rot, spin, life; int stopped, fallen, armed, deducted, megaTriggered, col; } Marker;
static Marker mk[3];
static int current;
int orient;
int score2[2], player, p2Round, roundFrames, totalFrames, suddenDeath, matchOver, frameCount;
static float chairX = FIELD_CX, chairY = 117.0f, chairR = 35.0f;

// The camera: at rest it shows the launch end (where you aim). It follows a marker up the
// table, holds on where it landed for a moment, then glides back for the next throw.
// SELECT held looks up the table at the markers already down.
#define CAM_REST (FIELD_H - SH)
static float camY = CAM_REST;
static int camOff;                      // this frame's camera row (whole pixels)
static int lastThrown = -1, landHoldT;
static const char *ORIENT_SHORT[3] = { "VERT", "ANGLE", "FLAT" };

// ── popups, banner ────────────────────────────────────────────────────────
typedef struct { float x, y; int life; char text[24]; int col; } Popup;
static Popup pops[6];
static void popup(float x, float y, const char *t, int c) {
    for (int i = 0; i < 6; i++) if (pops[i].life <= 0) { pops[i].x = x; pops[i].y = y; pops[i].life = 60; pops[i].col = c; strncpy(pops[i].text, t, 23); pops[i].text[23] = 0; return; }
}
static char banner[2][24]; static int bannerT, bannerCol;
static void showBanner(const char *a, const char *b, int c) { strncpy(banner[0], a, 23); banner[0][23] = 0; strncpy(banner[1], b ? b : "", 23); banner[1][23] = 0; bannerT = 75; bannerCol = c; }

// ── sound ─────────────────────────────────────────────────────────────────
static void play(const s8 *d, int len) { if (sv.sfxOn) sndFx(d, len); }
void sfxThrow(int o) {
    int r = rand();
    if (o == 0) { const s8 *s[4] = { snd_vertical1, snd_vertical2, snd_vertical3, snd_vertical4 };
                  const int l[4] = { SND_VERTICAL1_LEN, SND_VERTICAL2_LEN, SND_VERTICAL3_LEN, SND_VERTICAL4_LEN }; play(s[r % 4], l[r % 4]); }
    else if (o == 1) { if (r & 1) play(snd_angled1, SND_ANGLED1_LEN); else play(snd_angled2, SND_ANGLED2_LEN); }
    else { if (r & 1) play(snd_horizontal1, SND_HORIZONTAL1_LEN); else play(snd_horizontal2, SND_HORIZONTAL2_LEN); }
}
void sfxPeef(int o) {
    if (o == 0) play(snd_verticalpeef1, SND_VERTICALPEEF1_LEN);
    else if (o == 1) play(snd_angledpeef1, SND_ANGLEDPEEF1_LEN);
    else play(snd_horizontalpeef1, SND_HORIZONTALPEEF1_LEN);
}
void sfxPlop(void) { play(snd_plop, SND_PLOP_LEN); }
static int musicOnNow, musicTrack = MUS_MAIN;
void musicStart(void) {
    if (musicOnNow || !sv.musicOn) return;
    if (musicTrack == MUS_MINI) sndMusic(ms_music, MS_MUSIC_LEN); else if (musicTrack == MUS_BALL) sndMusic(bs_music, BS_MUSIC_LEN); else if (musicTrack == MUS_FIDGET) sndMusic(fs_music, FS_MUSIC_LEN); else if (musicTrack == MUS_BOWL) sndMusic(bw_music, BW_MUSIC_LEN); else if (musicTrack == MUS_STACK) sndMusic(st_music, ST_MUSIC_LEN); else if (musicTrack == MUS_FLIP) sndMusic(fl_music, FL_MUSIC_LEN); else if (musicTrack == MUS_DOZER) sndMusic(dz_music, DZ_MUSIC_LEN); else if (musicTrack == MUS_PIN) sndMusic(pb_music, PB_MUSIC_LEN); else sndMusic(snd_music, SND_MUSIC_LEN);
    musicOnNow = 1;
}
void musicSet(int t) { if (t != musicTrack) { musicStop(); musicTrack = t; } musicStart(); }
void musicStop(void) { sndMusicStop(); musicOnNow = 0; }
void musicToggle(void) { sv.musicOn = !sv.musicOn; if (sv.musicOn) { sndMusicMute(0); musicStart(); } else sndMusicMute(1); }

// ── maths ─────────────────────────────────────────────────────────────────
static float dist(float ax, float ay, float bx, float by) { return fsqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }
static int touching(Marker *a, Marker *b) { return dist(a->x, a->y, b->x, b->y) < TOUCH_DIST; }
static int wsx(float wx) { return (int)((wx - VX0) * PX); }
static int wsy(float wy) { return (int)(wy * PX) - camOff; }

// ── a turn ────────────────────────────────────────────────────────────────
static void resetSet(void) { current = 0; memset(mk, 0, sizeof mk); }
static int setHoldT;
void startMatch(void) {
    resetSet(); setHoldT = 0; roundFrames = 0; suddenDeath = 0; matchOver = 0;
    int rt = (mode == M_TWO) ? sv.timer2p : sv.timer1p;
    totalFrames = (mode == M_TWO) ? rt * 60 : (rt * 3 / 2) * 60;     // 1P & Olympics: sudden death for the last third
    chairX = FIELD_CX; chairY = 117.0f;
    camY = CAM_REST; lastThrown = -1; landHoldT = 0; bannerT = 0;
    for (int i = 0; i < 6; i++) pops[i].life = 0;
}
static void addScore(int d) { score2[player] += d; }
static void throwMarker(float dx, float dy) {
    if (screen != S_PLAY || current >= 3) return;
    Marker *m = &mk[current];
    memset(m, 0, sizeof *m);
    m->x = FIELD_CX; m->y = LAUNCH_Y;
    m->vx = dx * 0.2f; m->vy = dy * 0.2f; m->curve = dx * 0.0003f;
    m->spin = orient == 0 ? 0.08f : orient == 1 ? 0.12f : 0.18f;
    m->col = current; lastThrown = current; landHoldT = 0;
    // Land in the orientation you picked: the marker still spins the website's amount in flight
    // (spin x 22.85 over its one second), but it starts turned so it comes to rest VERTICAL /
    // ANGLED / FLAT. Each picture's marker lies at its own angle (red & green diagonal, blue level).
    {
        float target = orient == 0 ? 1.5708f : orient == 1 ? 0.7854f : 0.0f;
        float axis = m->col == 2 ? -0.14f : 0.7854f;
        m->rot = target - axis - m->spin * 22.85f;
    }
    current++;
    sfxThrow(orient);
}
static void triggerMega(Marker *m) {
    m->x = REDDOT_X; m->y = REDDOT_Y; m->vx = m->vy = 0; m->stopped = 1;
    addScore(20);
    showBanner("MEGA QU33PH", "+10", YELLOW);
    sfxPlop(); trackMega();
}
static int calcRoundScore(void) {
    Marker *g = &mk[0], *r = &mk[1], *b = &mk[2];
    if (!(g->armed && r->armed && b->armed)) return 0;
    int gr = touching(g, r), rb = touching(r, b), gb = touching(g, b);
    if (suddenDeath) {
        if (gr && rb && gb) return 20;
        if (rb || gb) return 10;
        if (gr) return 6;
        return 0;
    }
    if (gr && rb && gb) return 6;       // QU33PH = 3
    if (rb || gb) return 4;             // blue touching = 2
    if (gr) return 1;                   // green-red = 0.5
    return 0;
}
static void tryAdvanceRound(void) {
    if (setHoldT > 0) { if (--setHoldT == 0) { resetSet(); lastThrown = -1; } return; }   // then the camera glides back
    if (current < 3) return;
    for (int i = 0; i < 3; i++) if (!mk[i].stopped && !mk[i].fallen) return;
    Marker *g = &mk[0], *r = &mk[1], *b = &mk[2];
    if (!b->fallen && suddenDeath && b->y > chairY) {
        addScore(-4); popup(LEFT_EDGE + 4, chairY + 60, "PEEF -2", RED); sfxPeef(orient); trackPeef();
    }
    int rs = calcRoundScore();
    addScore(rs);
    char t[16], s[10]; scoreStr(s, rs); sprintf(t, "+%s", s);
    if (!g->fallen && !r->fallen && !b->fallen && touching(g, r) && touching(r, b) && touching(g, b)) {
        showBanner("QU33PH!", t, YELLOW); sfxPlop(); trackQu33ph(suddenDeath);
    } else showBanner(t, 0, WHITE);
    setHoldT = 45;                                     // keep the markers (and their touch glow) up for 3/4 s, then clear
}

static float aimAng = -1.5708f, power; static int charging, chargeT, ph;
void matchUpdate(void) {
    roundFrames++;
    int rt = (mode == M_TWO) ? sv.timer2p : sv.timer1p;
    if (mode != M_TWO && roundFrames > rt * 60 && !suddenDeath) {
        suddenDeath = 1; chairX = FIELD_CX; chairY = END_LINE_Y + 90;
        showBanner("SUDDEN DEATH", "hit the chair!", RED);
    }
    if (roundFrames > totalFrames) { matchOver = 1; return; }
    for (int i = 0; i < current; i++) {
        Marker *m = &mk[i];
        if (m->stopped || m->fallen) continue;
        m->life += 1.0f / 60.0f;
        if (m->life >= 1.0f) { m->stopped = 1; m->vx = m->vy = 0; sfxPlop(); }
        m->vx += m->curve;
        m->x += m->vx; m->y += m->vy;
        if (!m->armed && m->y < START_LINE_Y) m->armed = 1;
        m->vx *= 0.94f; m->vy *= 0.94f;
        m->rot += m->spin; m->spin *= 0.96f;
        if (suddenDeath && !m->megaTriggered && dist(m->x, m->y, chairX, chairY) < chairR) {
            if (frand() < 0.25f) triggerMega(m);
            else {                                   // the website's gentle chair bounce
                float dx = m->x - chairX, dy = m->y - chairY, d = dist(m->x, m->y, chairX, chairY); if (d < 0.01f) d = 1;
                float nx = dx / d, ny = dy / d;
                m->x = chairX + nx * chairR * 1.5f; m->y = chairY + ny * chairR * 1.5f;
                float relv = m->vx * nx + m->vy * ny;
                if (relv < 0) { m->vx -= 1.6f * relv * nx; m->vy -= 1.6f * relv * ny; }
                float side = (m->x < chairX) ? -1 : 1;
                m->vx += -ny * side * 1.8f; m->vy += nx * side * 1.8f;
                if (m->vy > -1) m->vy = -1 - frand() * 1.2f;
                if (m->vx < -3) m->vx = -3 + frand() * 1.5f;
                m->spin += (nx > 0 ? 1 : -1) * 1.1f;
            }
            m->megaTriggered = 1;
        }
        if (m->x < LEFT_EDGE) {
            if (!m->deducted) {
                int ded = suddenDeath ? 5 : 2;
                addScore(-2 * ded); m->deducted = 1;
                char t[12]; sprintf(t, "PEEF -%d", ded);
                popup(LEFT_EDGE + 4, m->y < END_LINE_Y + 40 ? END_LINE_Y + 40 : m->y, t, RED);
                sfxPeef(orient); trackPeef();
            }
            m->fallen = 1;
        }
        if (m->x > RIGHT_WALL) { m->x = RIGHT_WALL; m->vx *= -0.4f; }
        if (m->y < END_LINE_Y) { m->y = END_LINE_Y; m->vy *= -0.2f; }
        if (!m->stopped && fabsf_(m->vx) < 0.05f && fabsf_(m->vy) < 0.05f) { m->stopped = 1; sfxPlop(); }
        if ((m->stopped || m->fallen) && i == lastThrown) landHoldT = 60;   // look at where it landed for a second
    }
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) { pops[i].life--; pops[i].y -= 0.8f; }
    if (bannerT > 0) bannerT--;
    tryAdvanceRound();
    // the camera: follow the marker moving furthest up the table; once it stops, hold on it for
    // a second (or while the set is scored), then come back to the launch end for the next throw
    float target = CAM_REST; int lead = -1;
    for (int i = 0; i < current; i++) if (!mk[i].stopped && !mk[i].fallen && (lead < 0 || mk[i].y < mk[lead].y)) lead = i;
    if (landHoldT > 0) landHoldT--;
    if (lead < 0 && lastThrown >= 0 && lastThrown < current && !mk[lastThrown].fallen && (landHoldT > 0 || setHoldT > 0)) lead = lastThrown;
    if (lead >= 0) target = mk[lead].y * PX - SH * 0.55f;
    else if (kHeld & KEY_SELECT) {                     // SELECT: look up the table at the furthest marker down
        float top = 1e9f;                              // (none yet: the chair in sudden death, else the far end)
        for (int i = 0; i < current; i++) if (!mk[i].fallen && mk[i].y < top) top = mk[i].y;
        if (top > 1e8f) top = suddenDeath ? chairY : END_LINE_Y + 60;
        target = top * PX - SH * 0.5f;
    }
    if (target < 0) target = 0;
    if (target > CAM_REST) target = CAM_REST;
    camY += (target - camY) * (target < camY ? 0.16f : 0.10f);
    if (fabsf_(target - camY) < 0.5f) camY = target;
}

// ── aiming with buttons ───────────────────────────────────────────────────
// D-pad left/right aims; hold A to charge (the power swings up and down), let go to throw;
// B cancels. A full charge just reaches the back wall. L/R change how the marker lands.
void matchInput(int down, int held, int up) {
    if (down & KEY_L) orient = (orient + 2) % 3;
    if (down & KEY_R) orient = (orient + 1) % 3;
    if (held & KEY_LEFT)  { aimAng -= 0.03f; chargeT = 90; }
    if (held & KEY_RIGHT) { aimAng += 0.03f; chargeT = 90; }
    if (held & (KEY_LEFT | KEY_RIGHT | KEY_A)) landHoldT = 0;     // aiming: bring the camera home now
    if (aimAng < -2.6f) aimAng = -2.6f;
    if (aimAng > -0.55f) aimAng = -0.55f;
    if (down & KEY_A) { charging = 1; power = 0; ph = 0; }
    if (charging) {
        ph++; float p = (ph % 30) / 15.0f; power = p < 1 ? p : 2 - p;    // full in a quarter of a second
        chargeT = 90;
        if (down & KEY_B) charging = 0;
        else if (up & KEY_A) { charging = 0; if (motionTap) power = motionPower; float pw = 70 + power * 180; throwMarker(fcos(aimAng) * pw, fsin(aimAng) * pw); }
    }
    if (chargeT > 0 && !charging) chargeT--;
}

// ── sprites ───────────────────────────────────────────────────────────────
// Sprite pictures (4 bits a pixel, 64x64 = 64 tiles each), in the sprite area the bitmap modes leave:
// green, red, blue, chair, then the three gold halos. Palettes: 0 green, 1 red, 2 blue, 3 chair,
// 4 the halo's gold (pulses).
enum { T_GREEN = 512, T_RED = 576, T_BLUE = 640, T_CHAIR = 704, T_GLOW = 768 };
static const int MK_TILE[3] = { T_GREEN, T_RED, T_BLUE };
static u8 panelBg[64 * SH] EWRAM_BSS __attribute__((aligned(4)));   // the panel's fixed picture (the table map)
#define PANEL_X VIEW_W
#define MAP_X (PANEL_X + 38)             // the table map's left edge on screen
// The match (and PlinQu33ph) and each arcade game share the sprite memory: whichever screen is
// up has its pictures loaded.
void objSet(int set) {
    static int cur = -1;
    if (set == cur) return;
    cur = set;
    if (set == OBJ_MINI) { miniObjLoad(); return; }
    if (set == OBJ_BALL) { ballObjLoad(); return; }
    if (set == OBJ_FIDGET) { fidgetObjLoad(); return; }
    if (set == OBJ_BOWL) { bowlObjLoad(); return; }
    if (set == OBJ_STACK) { stackObjLoad(); return; }
    if (set == OBJ_FLIP) { flipObjLoad(); return; }
    if (set == OBJ_DOZER) { dozerObjLoad(); return; }
    if (set == OBJ_PIN) { pinObjLoad(); return; }
    if (set == OBJ_JUMP) { jumpObjLoad(); return; }
    if (set == OBJ_JUMPMENU) { jumpMenuObjLoad(); return; }
    platObjTiles(T_GREEN, obj_mk_green, 2048); platObjTiles(T_RED, obj_mk_red, 2048);
    platObjTiles(T_BLUE, obj_mk_blue, 2048); platObjTiles(T_CHAIR, obj_chair, 2048);
    platObjTiles(T_GLOW, glow_mk_green, 2048); platObjTiles(T_GLOW + 64, glow_mk_red, 2048); platObjTiles(T_GLOW + 128, glow_mk_blue, 2048);
    memcpy(&objPal[0], obj_mk_green_pal, 32); memcpy(&objPal[16], obj_mk_red_pal, 32);
    memcpy(&objPal[32], obj_mk_blue_pal, 32); memcpy(&objPal[48], obj_chair_pal, 32);
    casinoInit();
}
void gameInit(void) {
    // the panel: dark, with the whole table drawn small down its right-hand side
    memset(panelBg, C_PANEL, sizeof panelBg);
    for (int y = 0; y < SH; y++) {
        panelBg[y * 64] = C_PANELLINE;
        for (int x = 0; x < MM_W; x++) panelBg[y * 64 + (MAP_X - PANEL_X) + x] = mini8[y * MM_W + x];
        panelBg[y * 64 + (MAP_X - PANEL_X) - 1] = C_PANELLINE;
    }
}
void hideSprites(void) { for (int i = 0; i < 128; i++) { oam[i].a0 = 0x0200; oam[i].a1 = 0; oam[i].a2 = 0; } }
static int nObj;
static void affine(int m, float ang) {
    int c = (int)(fcos(ang) * 256), s = (int)(fsin(ang) * 256);
    oam[m * 4].a3 = (u16)c; oam[m * 4 + 1].a3 = (u16)s; oam[m * 4 + 2].a3 = (u16)-s; oam[m * 4 + 3].a3 = (u16)c;
}
// a turned 64x64 sprite centred on (cx, cy): double-size, so the turned picture is never cut off
static void spriteRot(int tile, int pal, int cx, int cy, float ang, int seeThrough) {
    if (nObj >= 32 || cx < -64 || cx >= VIEW_W + 64 || cy < -64 || cy >= SH + 64) return;
    int i = nObj++;
    affine(i, ang);
    oam[i].a0 = (u16)(((cy - 64) & 0xFF) | 0x0300 | (seeThrough ? 0x0400 : 0));      // affine, double size
    oam[i].a1 = (u16)(((cx - 64) & 0x1FF) | (i << 9) | 0xC000);                       // matrix i, 64x64
    oam[i].a2 = (u16)(tile | (pal << 12));
}
static const int MCOL[3] = { C_MGREEN, C_MRED, C_MBLUE };

// ── the panel ─────────────────────────────────────────────────────────────
static int mapX(float wx) { return MAP_X + (int)(((wx - VX0) * PX - MM_X0) * MM_W / 108.0f); }
static int mapY(float wy) { return (int)(wy * PX * MM_H / (float)FIELD_H); }
static void drawPanel(void) {
    char s[24], a[12];
    clipX0 = PANEL_X; clipX1 = SW; clipY0 = 0; clipY1 = SH;
    int left = (totalFrames - roundFrames + 59) / 60; if (left < 0) left = 0;
    // the words and numbers: redrawn only when one of them changed (they're the same most frames)
    int bar = 30 * (totalFrames - roundFrames) / (totalFrames ? totalFrames : 1);
    u32 sig = sigMix(sigMix(sigMix(sigMix(sigMix(sigMix(left, suddenDeath), bar), mode), score2[0]), score2[1]), player);
    sig = sigMix(sigMix(sigMix(sigMix(sig, p2Round), olyNation), current), orient);
    if (panelNeeds(sig)) {
    for (int y = 0; y < SH; y++) copy32(&page[y * (SW / 2) + VIEW_W / 2], &panelBg[y * 64], 9);
    textS(PANEL_X + 4, 3, suddenDeath ? "SUDDEN" : "TIME", suddenDeath ? RED : GREY);
    sprintf(s, "%d", left); text(PANEL_X + 4, 13, s, suddenDeath ? RED : WHITE, 1);
    int bh = 4;  // the time bar under it
    rect(PANEL_X + 4, 29, 30, bh, C_PANELLINE);
    rect(PANEL_X + 4, 29, bar, bh, suddenDeath ? RED : LIME);
    clipX1 = MAP_X - 2;                               // panel text never runs into the map
    if (mode != M_TWO) {
        textS(PANEL_X + 4, 38, "SCORE", GREY);
        scoreStr(a, score2[0]); text(PANEL_X + 4, 48, a, YELLOW, 1);
        if (mode == M_OLYMPICS) drawFlag(PANEL_X + 4, 70, 30, 20, olyNation);
    } else {                                         // whose turn it is shows in yellow
        for (int p = 0; p < 2; p++) {
            int y = 38 + p * 26, on = player == p;
            textS(PANEL_X + 4, y, p ? "P2" : "P1", on ? YELLOW : GREY);
            if (on) textS(PANEL_X + 18, y, "<", YELLOW);
            scoreStr(a, score2[p]); text(PANEL_X + 4, y + 9, a, on ? YELLOW : WHITE, 1);
        }
        sprintf(s, "RND %d/%d", p2Round, sv.twoRounds); textS(PANEL_X + 3, 90, s, GREY);
    }
    // markers left this set: three little sticks in their colours
    for (int i = 0; i < 3; i++) if (i >= current) rect(PANEL_X + 6 + i * 10, 100, 5, 16, MCOL[i]); else rect(PANEL_X + 6 + i * 10, 112, 5, 4, C_PANELLINE);
    textS(PANEL_X + 4, 126, "1 / 2", GREY);
    textS(PANEL_X + 4, 138, ORIENT_SHORT[orient], WHITE);
    }
    // the map: where the view is, the markers, the chair in sudden death, and the aim
    clipX0 = MAP_X; clipX1 = MAP_X + MM_W;
    int vy0 = camOff * MM_H / FIELD_H, vy1 = (camOff + SH) * MM_H / FIELD_H;
    rect(MAP_X, vy0, MM_W, 1, WHITE); rect(MAP_X, vy1 - 1, MM_W, 1, WHITE);
    rect(MAP_X, vy0, 1, vy1 - vy0, WHITE); rect(MAP_X + MM_W - 1, vy0, 1, vy1 - vy0, WHITE);
    if (suddenDeath) rect(mapX(chairX) - 1, mapY(chairY) - 1, 3, 3, RED);
    int moving = 0; for (int i = 0; i < current; i++) if (!mk[i].stopped && !mk[i].fallen) moving = 1;
    if (!moving && current < 3) {                      // the aim, as a dotted line up the table
        float ux = fcos(aimAng), uy = fsin(aimAng);
        for (int k = 4; k < 160; k += 6) { float wx = FIELD_CX + ux * k * 4.5f, wy = LAUNCH_Y + uy * k * 4.5f; if (wy < END_LINE_Y) break; pset(mapX(wx), mapY(wy), YELLOW); }
    }
    for (int i = 0; i < current; i++) { Marker *m = &mk[i]; if (m->fallen && m->x < LEFT_EDGE - 40) continue;
        int x = mapX(m->x), y = mapY(m->y); rect(x - 2, y - 1, 4, 3, C_BLACK); rect(x - 1, y - 1, 3, 3, MCOL[m->col]); }
    clipAll();
}

// ── drawing ───────────────────────────────────────────────────────────────
void matchDraw(void) {
    camOff = (int)camY;
    // the field: one row of the photo per line, then the panel's fixed picture beside it
    drawDirect();
    for (int y = 0; y < SH; y++) {
        copy32(&page[y * (SW / 2)], &field8[(camOff + y) * FIELD_W], FIELD_W / 4);
        copy32(&page[y * (SW / 2) + VIEW_W / 2 + 18], &panelBg[y * 64 + 36], 7);   // (the map's strip: it changes every frame)
    }
    clipX0 = 0; clipX1 = VIEW_W; clipY0 = 0; clipY1 = SH;
    // the power marker grows out of the launch spot
    if (current < 3 && (charging || chargeT > 0)) {
        float len = charging ? (40 + power * 150) * PX : 40 * PX;
        powerMarker(wsx(FIELD_CX), wsy(LAUNCH_Y), fcos(aimAng), fsin(aimAng), len, charging ? (power < 0.04f ? 0.04f : power) : 0);
    }
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) text(wsx(pops[i].x), wsy(pops[i].y), pops[i].text, pops[i].col, 1);
    if (bannerT > 0) { textCW(0, VIEW_W, 30, banner[0], bannerCol, 2); if (banner[1][0]) textCW(0, VIEW_W, 62, banner[1], WHITE, 2); }
    clipAll();
    drawPanel();

    // sprites: markers first (on top), then their halos, then the chair
    hideSprites(); nObj = 0;
    int haloN = 0; int halo[3], haloX[3], haloY[3]; float haloA[3];
    for (int i = 0; i < current; i++) {
        Marker *m = &mk[i];
        if (m->fallen && m->x < LEFT_EDGE - 40) continue;
        int cx = wsx(m->x), cy = wsy(m->y);
        int touch = 0;
        if (!m->fallen) for (int j = 0; j < current; j++) if (j != i && !mk[j].fallen && touching(m, &mk[j])) touch = 1;
        spriteRot(MK_TILE[m->col], m->col, cx, cy, m->rot, 0);
        if (touch) { halo[haloN] = m->col; haloX[haloN] = cx; haloY[haloN] = cy; haloA[haloN] = m->rot; haloN++; }
    }
    // markers still waiting this set sit on the table above the launch spot, as on the website
    // (see-through while you charge, so the power marker shows beneath them)
    for (int i = current; i < 3; i++) {
        float a = orient == 0 ? 1.0472f : orient == 1 ? 1.5708f : 0.0f;
        spriteRot(MK_TILE[i], i, wsx((LEFT_EDGE + RIGHT_WALL) / 2), wsy(LAUNCH_Y - 30 - i * 46), a, charging || chargeT > 0);
    }
    for (int k = 0; k < haloN; k++) spriteRot(T_GLOW + halo[k] * 64, 4, haloX[k], haloY[k], haloA[k], 0);
    // touching: a pulsing gold glow, so you can see the touch that scores
    { int pz = (frameCount >> 2) & 7; pz = pz < 4 ? pz : 7 - pz;
      objPal[64 + 2] = RGB15(31, 22 + pz * 2, 4 + pz * 3); objPal[64 + 1] = RGB15(18, 12 + pz, 2 + pz); }
    if (suddenDeath && nObj < 32) {
        int cx = wsx(chairX), cy = wsy(chairY);
        if (cx > -32 && cx < VIEW_W + 32 && cy > -32 && cy < SH + 32) {
            int i = nObj++;
            oam[i].a0 = (u16)(((cy - 32) & 0xFF));
            oam[i].a1 = (u16)(((cx - 32) & 0x1FF) | 0xC000);
            oam[i].a2 = (u16)(T_CHAIR | (3 << 12));
        }
    }
}
