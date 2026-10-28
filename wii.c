// wii.c — the Wii: stands in for gba.c. The games still draw the GBA's 240x160 screen (a page of
// 256-colour pixels plus GBA-style sprites in oam[]), so this file builds that picture in software
// each frame (the bitmap, the sprites with their rotation, flips, see-through blending and the
// match's sprite window), and shows it on the TV through the GPU, scaled to fill the screen at
// 3:2. It also plays the two sound channels (music + effects), keeps the save on the SD card, and
// reads the Wiimotes (and Nunchuk, Classic Controller, GameCube pads) — a swing of the Wiimote
// is a throw.
//
// HOST_TEST builds the same file on a PC with no Wii libraries: frames go to .ppm pictures and the
// buttons come from a script (the test harness).
#include "qu.h"
#include <math.h>

#ifndef HOST_TEST
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <fat.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

u16 *page;
u16 bgPal[256], objPal[256];
Obj oam[128];
volatile u32 vbCount;
int motionSwing; float motionPower;

static u16 pages[2][SW * SH / 2] __attribute__((aligned(32)));
static u8 objVram[0x8000] __attribute__((aligned(32)));    // sprite pictures (the GBA's 0x06010000 area)
static int shown, gameView;

void copy32(void *dst, const void *src, int words) { if (words > 0) memmove(dst, src, (size_t)words * 4); }
void fill32(void *dst, u32 v, int words) { u32 *d = dst; while (words-- > 0) *d++ = v; }
void platGameView(int w) { gameView = w; }
void platObjTiles(int tile, const void *src, int bytes) {
    int off = tile * 32;
    if (off < 0 || off >= (int)sizeof objVram) return;
    if (off + bytes > (int)sizeof objVram) bytes = (int)sizeof objVram - off;
    memcpy(objVram + off, src, (size_t)bytes);
}

// ── building the picture ──────────────────────────────────────────────────
// The sprite layer: each pixel's colour (15-bit), and flags: 1 = a sprite is here, 2 = see-through,
// priority in bits 2-3.
static u16 objCol[SW * SH];
static u8 objFlag[SW * SH];
static const u8 OBJ_W[3][4] = { { 8, 16, 32, 64 }, { 16, 32, 32, 64 }, { 8, 8, 16, 32 } };
static const u8 OBJ_H[3][4] = { { 8, 16, 32, 64 }, { 8, 8, 16, 32 }, { 16, 32, 32, 64 } };

static void drawSprites(void) {
    memset(objFlag, 0, sizeof objFlag);
    int xLim = gameView ? gameView : SW;                     // the match: sprites only over the field
    for (int i = 127; i >= 0; i--) {                         // lower numbers end up on top
        u16 a0 = oam[i].a0, a1 = oam[i].a1, a2 = oam[i].a2;
        int rot = a0 & 0x100, dbl = a0 & 0x200;
        if (!rot && dbl) continue;                           // hidden
        int gm = (a0 >> 10) & 3, shape = a0 >> 14, size = a1 >> 14;
        if (gm >= 2 || shape == 3) continue;
        int w = OBJ_W[shape][size], h = OBJ_H[shape][size];
        int bw = (rot && dbl) ? w * 2 : w, bh = (rot && dbl) ? h * 2 : h;
        int y = a0 & 0xFF, x = a1 & 0x1FF;
        if (y + bh > 256) y -= 256;
        if (x + bw > 512) x -= 512;
        if (y >= SH || x >= xLim || y + bh <= 0 || x + bw <= 0) continue;
        int bpp8 = a0 & 0x2000, tile = a2 & 0x3FF, pal = (a2 >> 12) * 16, prio = (a2 >> 10) & 3;
        u8 fl = (u8)(1 | (gm == 1 ? 2 : 0) | (prio << 2));
        int pa = 256, pb = 0, pc = 0, pd = 256, hf = 0, vf = 0;
        if (rot) {
            int p = (a1 >> 9) & 31;
            pa = (s16)oam[p * 4].a3; pb = (s16)oam[p * 4 + 1].a3; pc = (s16)oam[p * 4 + 2].a3; pd = (s16)oam[p * 4 + 3].a3;
        } else { hf = a1 & 0x1000; vf = a1 & 0x2000; }
        int tw = w >> 3, step = bpp8 ? 2 : 1;
        int sy0 = y < 0 ? -y : 0, sy1 = y + bh > SH ? SH - y : bh;
        int sx0 = x < 0 ? -x : 0, sx1 = x + bw > xLim ? xLim - x : bw;
        for (int sy = sy0; sy < sy1; sy++) {
            int py = y + sy, row = py * SW;
            for (int sx = sx0; sx < sx1; sx++) {
                int tx, ty;
                if (rot) {
                    int dx = sx - bw / 2, dy = sy - bh / 2;
                    tx = ((pa * dx + pb * dy) >> 8) + w / 2; ty = ((pc * dx + pd * dy) >> 8) + h / 2;
                    if ((unsigned)tx >= (unsigned)w || (unsigned)ty >= (unsigned)h) continue;
                } else { tx = hf ? w - 1 - sx : sx; ty = vf ? h - 1 - sy : sy; }
                int tn = (tile + ((ty >> 3) * tw + (tx >> 3)) * step) & 0x3FF;
                int ci; u16 col;
                if (bpp8) { ci = objVram[tn * 32 + (ty & 7) * 8 + (tx & 7)]; if (!ci) continue; col = objPal[ci]; }
                else {
                    int b = objVram[tn * 32 + (ty & 7) * 4 + ((tx & 7) >> 1)];
                    ci = (tx & 1) ? b >> 4 : b & 15; if (!ci) continue; col = objPal[pal + ci];
                }
                int o = row + x + sx;
                if (!(objFlag[o] & 1) || prio <= ((objFlag[o] >> 2) & 3)) { objCol[o] = col; objFlag[o] = fl; }
            }
        }
    }
}

// The finished picture, 15-bit colour, one pixel at (x, y) handed to put565 (the TV's texture or a
// test picture). The BG is mode 4: the shown page, colour 0 = the backdrop (palette 0). A sprite
// pixel over the BG wins unless its priority puts it behind; see-through ones mix 7/16 sprite with
// 9/16 of the picture under them (the GBA's settings), but not over the bare backdrop.
static inline u16 mix(u16 a, u16 b) {
    int r = ((a & 31) * 7 + (b & 31) * 9) >> 4, g = (((a >> 5) & 31) * 7 + ((b >> 5) & 31) * 9) >> 4,
        bl = (((a >> 10) & 31) * 7 + ((b >> 10) & 31) * 9) >> 4;
    return (u16)(r | (g << 5) | (bl << 10));
}
static void composeRow(int y, u16 *out /* SW 15-bit colours */) {
    const u8 *pg = (const u8 *)pages[shown] + y * SW;      // (bytes in memory order: left pixel first)
    for (int x = 0; x < SW; x++) {
        int idx = pg[x], o = y * SW + x;
        u16 c = bgPal[idx];
        u8 f = objFlag[o];
        if (f & 1) {
            int behind = ((f >> 2) & 3) > 0 && idx;       // (the BG is priority 0)
            if (!behind) c = (f & 2) && idx ? mix(objCol[o], c) : objCol[o];
        }
        out[x] = c & 0x7FFF;
    }
}

#ifndef HOST_TEST
// ══ THE WII ══════════════════════════════════════════════════════════════
static GXRModeObj *rmode;
static void *xfb[2]; static int fb;
static void *gpFifo;
#define TEX_W (SW * 2)                                    // the picture doubled (each pixel 2x2), then smoothed
#define TEX_H (SH * 2)                                    // by the GPU: sharp, with even pixels at any size
static u16 tex[TEX_W * TEX_H] __attribute__((aligned(32)));
static u16 to565[32768];
static GXTexObj texObj;
static float qx0, qy0, qx1, qy1;                          // where the picture goes on the TV
static int pal50;
static u64 t0;
static volatile int quitReq;

static void retrace(u32 n) { (void)n; if (!pal50) vbCount++; }
static void onReset(u32 irq, void *ctx) { (void)irq; (void)ctx; quitReq = 1; }
static void onPower(void) { quitReq = 2; }
static void onPadPower(s32 chan) { (void)chan; quitReq = 2; }

static void videoInit(void) {
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    VIDEO_Configure(rmode);
    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetPostRetraceCallback(retrace);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    pal50 = VIDEO_GetCurrentTvMode() == VI_PAL;            // 50 a second: the game counts time instead

    gpFifo = memalign(32, 256 * 1024); memset(gpFifo, 0, 256 * 1024);
    GX_Init(gpFifo, 256 * 1024);
    GX_SetCopyClear((GXColor){ 0, 0, 0, 0xFF }, GX_MAX_Z24);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    f32 ys = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbH = GX_SetDispCopyYScale(ys);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbH);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, (rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);
    GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetClipMode(GX_CLIP_DISABLE);
    GX_CopyDisp(xfb[0], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    Mtx44 proj; Mtx view;
    guOrtho(proj, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
    guMtxIdentity(view);
    GX_LoadPosMtxImm(view, GX_PNMTX0);
    GX_InvalidateTexAll();

    // the picture at 3:2, as big as fits, with a little room for TVs that crop the edges
    float W = rmode->fbWidth, H = rmode->efbHeight;
    float aspect = CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? 16.0f / 9.0f : 4.0f / 3.0f;
    float par = aspect * H / W;                            // the width of one pixel on the TV, over its height
    float qw = 1.5f * W / aspect, qh = H;
    if (qw > W) { qw = W; qh = qw * par / 1.5f; }
    qw *= 0.94f; qh *= 0.94f;
    qx0 = (W - qw) / 2; qx1 = qx0 + qw; qy0 = (H - qh) / 2; qy1 = qy0 + qh;

    for (int c = 0; c < 32768; c++) {
        int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        to565[c] = (u16)((r << 11) | (((g << 1) | (g >> 4)) << 5) | b);
    }
}

// the texture is in the GPU's 4x4 tiles: each tile 16 pixels in a row, tiles left to right
static inline void texPut(int x, int y, u16 c) { tex[((y >> 2) * (TEX_W / 4) + (x >> 2)) * 16 + ((y & 3) << 2) + (x & 3)] = c; }
static void present(void) {
    static u16 row[SW];
    for (int y = 0; y < SH; y++) {
        composeRow(y, row);
        for (int x = 0; x < SW; x++) {
            u16 c = to565[row[x]];
            texPut(2 * x, 2 * y, c); texPut(2 * x + 1, 2 * y, c); texPut(2 * x, 2 * y + 1, c); texPut(2 * x + 1, 2 * y + 1, c);
        }
    }
    DCFlushRange(tex, sizeof tex);
    GX_InvalidateTexAll();
    GX_InitTexObj(&texObj, tex, TEX_W, TEX_H, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(qx0, qy0, 0); GX_TexCoord2f32(0, 0);
    GX_Position3f32(qx1, qy0, 0); GX_TexCoord2f32(1, 0);
    GX_Position3f32(qx1, qy1, 0); GX_TexCoord2f32(1, 1);
    GX_Position3f32(qx0, qy1, 0); GX_TexCoord2f32(0, 1);
    GX_End();
    GX_DrawDone();
    fb ^= 1;
    GX_CopyDisp(xfb[fb], GX_TRUE);
    GX_DrawDone();
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (pal50) vbCount = (u32)(ticks_to_microsecs(gettime() - t0) * 60ULL / 1000000ULL);
}

// ── sound: ASND voice 0 = the music (10000 a second, loops), voice 1 = the effects (16000) ──
// ASND wants sounds in 32-byte-aligned memory, so each one is copied once, the first time it plays.
typedef struct { const s8 *src; s8 *buf; int len; } SndCopy;
static SndCopy sndCopies[64]; static int nCopies;
static s8 *sndBuf(const s8 *d, int len, int *outLen) {
    for (int i = 0; i < nCopies; i++) if (sndCopies[i].src == d && sndCopies[i].len >= len) { *outLen = (len + 31) & ~31; return sndCopies[i].buf; }
    int n = (len + 31) & ~31;
    s8 *b = memalign(32, n);
    if (!b) return NULL;
    memcpy(b, d, len); memset(b + len, 0, n - len);
    DCFlushRange(b, n);
    if (nCopies < 64) sndCopies[nCopies++] = (SndCopy){ d, b, len };
    *outLen = n; return b;
}
static int musMuted; static const s8 *musCur;
#define MUS_VOL 200
void sndMusic(const s8 *d, int len) {
    int n; s8 *b = sndBuf(d, len, &n); if (!b) return;
    ASND_StopVoice(0);
    ASND_SetInfiniteVoice(0, VOICE_MONO_8BIT, 10000, 0, b, n, musMuted ? 0 : MUS_VOL, musMuted ? 0 : MUS_VOL);
    musCur = d;
}
void sndMusicStop(void) { ASND_StopVoice(0); musCur = NULL; }
void sndMusicMute(int mute) { musMuted = mute; if (musCur) ASND_ChangeVolumeVoice(0, mute ? 0 : MUS_VOL, mute ? 0 : MUS_VOL); }
void sndFx(const s8 *d, int len) {
    int n; s8 *b = sndBuf(d, len, &n); if (!b) return;
    ASND_StopVoice(1);
    ASND_SetVoice(1, VOICE_MONO_8BIT, 16000, 0, b, n, 255, 255, NULL);
}

// ── save: the GBA's save memory, kept as a file on the SD card (or a USB drive) ──
static u8 sram[0x8000]; static int sramUsed, sramDirty, haveFat;
static char savePath[64];
static void saveLoad(void) {
    memset(sram, 0xFF, sizeof sram);                       // (blank save memory reads as all ones)
    haveFat = fatInitDefault();
    if (!haveFat) return;
    const char *dev[2] = { "sd:", "usb:" };
    for (int i = 0; i < 2; i++) {
        char dir[32]; sprintf(dir, "%s/apps", dev[i]);
        struct stat st; if (stat(dir, &st) != 0) continue;
        sprintf(dir, "%s/apps/qu33ph", dev[i]); mkdir(dir, 0777);
        sprintf(savePath, "%s/apps/qu33ph/qu33ph.sav", dev[i]);
        FILE *f = fopen(savePath, "rb");
        if (f) { sramUsed = (int)fread(sram, 1, sizeof sram, f); fclose(f); }
        return;
    }
    haveFat = 0;
}
static void saveFlush(void) {
    if (!sramDirty || !haveFat || !savePath[0]) return;
    sramDirty = 0;
    FILE *f = fopen(savePath, "wb");
    if (f) { fwrite(sram, 1, (size_t)sramUsed, f); fclose(f); }
}
void sramRead(void *dst, int off, int n) { if (off >= 0 && off + n <= (int)sizeof sram) memcpy(dst, sram + off, (size_t)n); }
void sramWrite(int off, const void *src, int n) {
    if (off < 0 || off + n > (int)sizeof sram) return;
    memcpy(sram + off, src, (size_t)n);
    if (off + n > sramUsed) sramUsed = off + n;
    sramDirty = 1;                                         // written to the card once this frame is shown
}

// ── leaving: HOME, RESET or POWER (the save is written first) ─────────────
static void quit(void) {
    saveFlush();
    ASND_End();
    WPAD_Shutdown();
    VIDEO_SetBlack(true); VIDEO_Flush(); VIDEO_WaitVSync();
    if (quitReq == 2) SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    exit(0);                                               // back to the Homebrew Channel
}

void platInit(void) {
    videoInit();
    t0 = gettime();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetPowerButtonCallback(onPadPower);
    PAD_Init();
    SYS_SetResetCallback(onReset);
    SYS_SetPowerCallback(onPower);
    ASND_Init();
    ASND_Pause(0);
    saveLoad();
    for (int i = 0; i < 128; i++) { oam[i].a0 = 0x0200; oam[i].a1 = oam[i].a2 = oam[i].a3 = 0; }
    shown = 0; page = pages[1];
}
void platFlip(void) { platShow(1); }
void platShow(int swap) {
    shown ^= swap;
    drawSprites();
    present();
    page = pages[shown ^ 1];
    saveFlush();
    if (quitReq) quit();
}

// ── the controllers ───────────────────────────────────────────────────────
// Wiimote held upright, pointing at the TV: D-pad, A, B (trigger), + = START, - = SELECT,
// 1 = L, 2 = R, HOME quits. Nunchuk: the stick is the D-pad, C = L, Z = R. Classic Controller
// and GameCube pads work as you'd expect. Every Wiimote counts (pass yours in 2 PLAYER, or use two).
//
// A swing: the Wiimote's acceleration jumps past 2 g; the hardest moment of the swing sets the
// power (about 2 g = softest, 4.3 g or more = full), and it counts once the flick has settled.
typedef struct { int armed, t, cool; float peak; } Swing;
static Swing sw[4];
static int swingCheck(int ch, const WPADData *wd) {
    Swing *s = &sw[ch];
    float x = wd->gforce.x, y = wd->gforce.y, z = wd->gforce.z, g = sqrtf(x * x + y * y + z * z);
    if (s->cool > 0) s->cool--;
    if (!s->armed) {
        if (g > 2.0f && !s->cool) { s->armed = 1; s->peak = g; s->t = 0; }
        return 0;
    }
    if (g > s->peak) s->peak = g;
    if (++s->t >= 3 && (g < 1.5f || s->t > 14)) {
        s->armed = 0; s->cool = 24;
        float p = (s->peak - 1.9f) / (4.3f - 1.9f);
        motionPower = p < 0.05f ? 0.05f : p > 1 ? 1 : p;
        return 1;
    }
    return 0;
}
static u16 stickDir(float ang, float mag) {
    if (mag < 0.45f) return 0;
    u16 k = 0;
    if (ang > 292.5f || ang < 67.5f) k |= KEY_UP;
    if (ang > 22.5f && ang < 157.5f) k |= KEY_RIGHT;
    if (ang > 112.5f && ang < 247.5f) k |= KEY_DOWN;
    if (ang > 202.5f && ang < 337.5f) k |= KEY_LEFT;
    return k;
}
u16 platKeys(void) {
    WPAD_ScanPads(); PAD_ScanPads();
    u16 k = 0; motionSwing = 0;
    for (int ch = 0; ch < 4; ch++) {
        WPADData *wd = WPAD_Data(ch);
        if (!wd || wd->err != WPAD_ERR_NONE) continue;
        u32 b = wd->btns_h;
        if (b & WPAD_BUTTON_UP) k |= KEY_UP;
        if (b & WPAD_BUTTON_DOWN) k |= KEY_DOWN;
        if (b & WPAD_BUTTON_LEFT) k |= KEY_LEFT;
        if (b & WPAD_BUTTON_RIGHT) k |= KEY_RIGHT;
        if (b & WPAD_BUTTON_A) k |= KEY_A;
        if (b & WPAD_BUTTON_B) k |= KEY_B;
        if (b & WPAD_BUTTON_PLUS) k |= KEY_START;
        if (b & WPAD_BUTTON_MINUS) k |= KEY_SELECT;
        if (b & WPAD_BUTTON_1) k |= KEY_L;
        if (b & WPAD_BUTTON_2) k |= KEY_R;
        if (wd->btns_d & WPAD_BUTTON_HOME) quitReq = 1;
        if (wd->exp.type == WPAD_EXP_NUNCHUK) {
            if (b & WPAD_NUNCHUK_BUTTON_C) k |= KEY_L;
            if (b & WPAD_NUNCHUK_BUTTON_Z) k |= KEY_R;
            k |= stickDir(wd->exp.nunchuk.js.ang, wd->exp.nunchuk.js.mag);
        } else if (wd->exp.type == WPAD_EXP_CLASSIC) {
            if (b & WPAD_CLASSIC_BUTTON_UP) k |= KEY_UP;
            if (b & WPAD_CLASSIC_BUTTON_DOWN) k |= KEY_DOWN;
            if (b & WPAD_CLASSIC_BUTTON_LEFT) k |= KEY_LEFT;
            if (b & WPAD_CLASSIC_BUTTON_RIGHT) k |= KEY_RIGHT;
            if (b & WPAD_CLASSIC_BUTTON_A) k |= KEY_A;
            if (b & WPAD_CLASSIC_BUTTON_B) k |= KEY_B;
            if (b & WPAD_CLASSIC_BUTTON_PLUS) k |= KEY_START;
            if (b & WPAD_CLASSIC_BUTTON_MINUS) k |= KEY_SELECT;
            if (b & (WPAD_CLASSIC_BUTTON_FULL_L | WPAD_CLASSIC_BUTTON_ZL)) k |= KEY_L;
            if (b & (WPAD_CLASSIC_BUTTON_FULL_R | WPAD_CLASSIC_BUTTON_ZR)) k |= KEY_R;
            if (wd->btns_d & WPAD_CLASSIC_BUTTON_HOME) quitReq = 1;
            k |= stickDir(wd->exp.classic.ljs.ang, wd->exp.classic.ljs.mag);
        }
        if (swingCheck(ch, wd)) motionSwing = 1;
    }
    for (int p = 0; p < 4; p++) {
        u16 b = PAD_ButtonsHeld(p);
        if (b & PAD_BUTTON_UP) k |= KEY_UP;
        if (b & PAD_BUTTON_DOWN) k |= KEY_DOWN;
        if (b & PAD_BUTTON_LEFT) k |= KEY_LEFT;
        if (b & PAD_BUTTON_RIGHT) k |= KEY_RIGHT;
        if (b & PAD_BUTTON_A) k |= KEY_A;
        if (b & PAD_BUTTON_B) k |= KEY_B;
        if (b & PAD_BUTTON_START) k |= KEY_START;
        if (b & (PAD_BUTTON_Y | PAD_TRIGGER_Z)) k |= KEY_SELECT;
        if (b & PAD_TRIGGER_L) k |= KEY_L;
        if (b & PAD_TRIGGER_R) k |= KEY_R;
        int sx = PAD_StickX(p), sy = PAD_StickY(p);
        if (sx < -48) k |= KEY_LEFT;
        if (sx > 48) k |= KEY_RIGHT;
        if (sy > 48) k |= KEY_UP;
        if (sy < -48) k |= KEY_DOWN;
    }
    return k;
}

#else
// ══ THE PC TEST HARNESS ═════════════════════════════════════════════════
// QTEST_SCRIPT: "frame:keys[:swingpower];..." keys as a GBA key mask (decimal), held from that frame on.
// QTEST_SHOTS: "frame,frame,..." pictures to write (shot_<frame>.ppm). QTEST_FRAMES: frames to run.
static int frameNo, nextEv; static u16 heldK;
static struct { int f; u16 k; float sw; } ev[256]; static int nEv;
static int shots[64], nShots, maxFrames = 600;
static u8 sramT[0x8000];
void sndMusic(const s8 *d, int len) { (void)d; (void)len; }
void sndMusicStop(void) {}
void sndMusicMute(int m) { (void)m; }
void sndFx(const s8 *d, int len) { (void)d; (void)len; }
void sramRead(void *dst, int off, int n) { memcpy(dst, sramT + off, (size_t)n); }
void sramWrite(int off, const void *src, int n) { memcpy(sramT + off, src, (size_t)n); }
void platInit(void) {
    memset(sramT, 0xFF, sizeof sramT);
    const char *s = getenv("QTEST_SCRIPT");
    while (s && *s && nEv < 256) {
        int f, k, n = 0; float p = -1;
        if (sscanf(s, "%d:%d%n", &f, &k, &n) < 2) break;
        s += n; if (*s == ':') { int m = 0; sscanf(s + 1, "%f%n", &p, &m); s += 1 + m; }
        ev[nEv].f = f; ev[nEv].k = (u16)k; ev[nEv].sw = p; nEv++;
        if (*s == ';') s++;
    }
    s = getenv("QTEST_SHOTS");
    while (s && *s && nShots < 64) { shots[nShots++] = atoi(s); while (*s && *s != ',') s++; if (*s) s++; }
    if (getenv("QTEST_FRAMES")) maxFrames = atoi(getenv("QTEST_FRAMES"));
    for (int i = 0; i < 128; i++) { oam[i].a0 = 0x0200; oam[i].a1 = oam[i].a2 = oam[i].a3 = 0; }
    shown = 0; page = pages[1];
}
void platFlip(void) { platShow(1); }
void platShow(int swap) {
    shown ^= swap;
    drawSprites();
    for (int i = 0; i < nShots; i++) if (shots[i] == frameNo) {
        char name[32]; sprintf(name, "shot_%05d.ppm", frameNo);
        FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", SW, SH);
        u16 row[SW];
        for (int y = 0; y < SH; y++) { composeRow(y, row); for (int x = 0; x < SW; x++) {
            u8 rgb[3] = { (u8)((row[x] & 31) * 255 / 31), (u8)(((row[x] >> 5) & 31) * 255 / 31), (u8)(((row[x] >> 10) & 31) * 255 / 31) };
            fwrite(rgb, 1, 3, f); } }
        fclose(f);
    }
    page = pages[shown ^ 1];
    vbCount++; frameNo++;
    if (frameNo >= maxFrames) { printf("ran %d frames, screen %d\n", frameNo, screen); exit(0); }
}
u16 platKeys(void) {
    motionSwing = 0;
    static int went;
    if (!went && getenv("QTEST_GO")) { went = 1; goScreen(atoi(getenv("QTEST_GO"))); }
    while (nextEv < nEv && ev[nextEv].f <= frameNo) {
        heldK = ev[nextEv].k;
        if (ev[nextEv].sw >= 0) { motionSwing = 1; motionPower = ev[nextEv].sw; }
        nextEv++;
    }
    return heldK;
}
#endif
