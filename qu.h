// qu.h — shared by every Qu33ph source file (GBA and Wii)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
#include "gfx.h"
#include "snd.h"

// Hot loops run from the GBA's fast internal RAM as full 32-bit ARM code (the cartridge is slow).
// IWRAM_CODE goes on the definition, IWRAM_FN on the prototype other files see.
#if defined(GBA_SIM) || defined(QU_WII)
#define IWRAM_CODE
#define IWRAM_FN
#define IWRAM_LOCAL
#define EWRAM_BSS
#else
#define IWRAM_CODE __attribute__((section(".iwram"), long_call, target("arm"), noinline))
#define IWRAM_LOCAL __attribute__((section(".iwram"), target("arm"), noinline))   // fast RAM, called only from fast RAM
#define IWRAM_FN   __attribute__((long_call))
#define EWRAM_BSS  __attribute__((section(".sbss")))
#endif

#define SW 240
#define SH 160
#define VIEW_W 176                 // the match's field column; the panel fills x 176-239

// ── buttons on the GBA ────────────────────────────────────────────────────
enum { KEY_A = 1, KEY_B = 2, KEY_SELECT = 4, KEY_START = 8, KEY_RIGHT = 16, KEY_LEFT = 32, KEY_UP = 64,
       KEY_DOWN = 128, KEY_R = 256, KEY_L = 512 };

// ── colours: the screen is 256 colours. 0-63 are the menus' fixed colours (below);
//    64-255 hold the field photo during a match and the logo on the menus.
enum { C_BLACK, C_WHITE, C_DARK, C_YELLOW, C_GOLD, C_GREY, C_RED, C_LIME, C_SEL, C_DIMEDGE, C_SILVER,
       C_BRONZE, C_PANEL, C_PANELLINE, C_TOAST, C_SHINE, C_GRAD0 /* 16-47: the buttons' gradient */,
       C_FR = 48, C_FB, C_FG, C_FY, C_FO, C_FL, C_MGREEN, C_MRED, C_MBLUE, C_POWER, C_NIB, C_PWDARK,
       C_PWOFF, C_CYAN, C_LBLUE, C_DIMWHITE };
#define WHITE  C_WHITE
#define BLACK  C_BLACK
#define YELLOW C_YELLOW
#define GOLD   C_GOLD
#define GREY   C_GREY
#define RED    C_RED
#define LIME   C_LIME
#define DARK   C_DARK
#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

// Two 8-bit pixels in one 16-bit word of the page: the left one is first in memory. The GBA is
// little-endian (left = low byte); the Wii is big-endian (left = high byte).
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define PX2(l, r)   ((u16)(((l) << 8) | (r)))
#define PXL(w, c)   ((u16)(((w) & 0x00FF) | ((c) << 8)))     // word w with its left pixel set to c
#define PXR(w, c)   ((u16)(((w) & 0xFF00) | (c)))            // ... its right pixel
#else
#define PX2(l, r)   ((u16)((l) | ((r) << 8)))
#define PXL(w, c)   ((u16)(((w) & 0xFF00) | (c)))
#define PXR(w, c)   ((u16)(((w) & 0x00FF) | ((c) << 8)))
#endif

// ── the Wiimote's swing (Wii only; on the GBA these are always 0) ─────────
#ifdef QU_WII
extern int motionSwing;            // set by platKeys() on the frame a swing ends
extern float motionPower;          // how hard the swing was, 0-1
extern int motionTap;              // this frame's A let-go came from a swing (main.c): use motionPower
#else
#define motionSwing 0
#define motionPower 0.0f
#define motionTap 0
#endif

// ── the platform (gba.c on the GBA, wii.c on the Wii, sim.c in the PC test harness) ─
extern u16 *page;                  // the hidden page being drawn: 240x160, one byte a pixel, written 16 bits at a time
extern u16 bgPal[256], objPal[256];
typedef struct { u16 a0, a1, a2, a3; } Obj;
extern Obj oam[128];               // a copy of the sprite table, sent to the hardware in the blank
extern volatile u32 vbCount;       // screen refreshes so far (60 a second)
void platInit(void);
void platShow(int swap);           // the same, but swap 0 keeps the page on screen (nothing new was drawn)
void platFlip(void);
void drawBegin(void);              // start listing this frame's drawing (see draw.c)
void drawDirect(void);             // about to write straight into the page: draw what's listed, stop listing
void drawEnd(void);
IWRAM_FN void drawImg_(const u8 *img, int w, int h, int x, int y);    // drawImg at once, never listed (direct screens only)
IWRAM_FN void hspan(int y, int x0, int x1, int c);                     // a clipped row, drawn at once (direct screens only)
IWRAM_FN void hstripe(int y, int x0, int x1, int w, int c0, int c1);  // a row of diagonal stripes (direct screens only)
u32 sigMix(u32 h, int v);          // fold a number into a fingerprint
int panelNeeds(u32 sig);           // 1 = draw the side panel this frame (its fingerprint changed on this page)
int panelLine(int id, u32 sig, int x, int y, int w, int h, int bg);   // 1 = redraw this panel line (its box is cleared)
int toastOn(void);                // draw the list if the screen changed, then show it               // wait for the blank, show the finished page, send sprites + palettes
void platObjTiles(int tile, const void *src, int bytes);   // sprite pictures (tile = 32-byte units)
u16  platKeys(void);
void platGameView(int w);          // sprites only over the left w columns (0: anywhere)
void copy32(void *dst, const void *src, int words);
void fill32(void *dst, u32 v, int words);
// sound: channel A plays the music (10000 samples a second), channel B the effects (16000)
void sndMusic(const s8 *d, int len);
void sndMusicStop(void);
void sndMusicMute(int mute);
void sndFx(const s8 *d, int len);
// save memory (battery-backed SRAM on the cartridge)
IWRAM_FN void sramRead(void *dst, int off, int n);
IWRAM_FN void sramWrite(int off, const void *src, int n);

// ── screens ───────────────────────────────────────────────────────────────
enum { S_TITLE, S_PLAY, S_PAUSE, S_HANDOFF, S_RESULTS, S_HIGHS, S_SETTINGS, S_OLY_SELECT, S_OLY_BRACKET, S_NAME, S_SLOT, S_PLINKO,
       S_ARCADE, S_MINI_MENU, S_MINI, S_MINI_PAUSE, S_MINI_OVER, S_BALL_MENU, S_BALL, S_BALL_PAUSE, S_BALL_OVER,
       S_FIDGET_MENU, S_FIDGET, S_FIDGET_PAUSE, S_FIDGET_OVER, S_BOWL_MENU, S_BOWL, S_BOWL_PAUSE, S_BOWL_OVER,
       S_STACK_MENU, S_STACK, S_STACK_PAUSE, S_STACK_OVER, S_FLIP_MENU, S_FLIP_LEVELS, S_FLIP, S_FLIP_PAUSE, S_FLIP_OVER,
       S_DOZER_MENU, S_DOZER, S_DOZER_PAUSE, S_DOZER_OVER, S_PIN_MENU, S_PIN, S_PIN_PAUSE, S_PIN_OVER,
       S_JUMP_MENU, S_JUMP_CHARS, S_JUMP, S_JUMP_PAUSE, S_JUMP_OVER };
extern int screen;
enum { M_SINGLE = 1, M_TWO = 2, M_OLYMPICS = 3 };
extern int mode;

// ── save data ─────────────────────────────────────────────────────────────
typedef struct { char name[9]; int score2; } HighEntry;          // scores are doubled (half points exist)
typedef struct {
    u32 magic, version;
    HighEntry high1p[10], highOly[10];
    int games, qu33phs, megas, peefs, forfeits;
    int gold, silver, bronze;
    int musicOn, sfxOn, twoRounds, timer1p, timer2p, orient;
    char name[9];
    int coins, coinsEarned, coinsSpent, slotSpins, slotWins, slotLost;   // (took 6 of the spare slots: older saves read 0)
    int ballBest[3];               // Qu33ph-Ball's best on each machine (took 3 spare slots)
    u32 flipTiers[3];              // Flip: each level's grade, 2 bits a level (took 6 more)
    int flipDone, flipBest, flipBestP;
    u32 spare[1];                  // room to grow
    // ── added in save version 2 (the arcade). Version 1 saves load untouched; these start at 0.
    int arcadeBest[9], arcadePlays[9];   // one slot per arcade game (ARC_*)
    // ── added in save version 3 (Jump). Older saves load untouched; these start at 0.
    int jumpBest[4], jumpChar;           // Jump: each mode's best, your runner
} SaveData;
enum { ARC_MINI, ARC_BALL, ARC_FIDGET, ARC_BOWLING, ARC_STACK, ARC_FLIP, ARC_DOZER, ARC_JUMP, ARC_PINBALL, ARC_COUNT };
extern SaveData sv;
extern int saveOK;
void saveInit(void);
void saveWrite(void);
void resetHighScores(void);
void resetEverything(void);
void trackGameEnd(int score2);
void addCoins(int n);
void spendCoins(int n);
void trackQu33ph(int suddenDeath);
void trackMega(void);
void trackPeef(void);
int  highQualifies(HighEntry *list, int score2);
void highInsert(HighEntry *list, const char *name, int score2);

// ── drawing (draw.c) ──────────────────────────────────────────────────────
extern int clipX0, clipX1, clipY0, clipY1;      // where drawing may land (the match keeps to its field)
void clipAll(void);
void pset(int x, int y, int c);
void rect(int x, int y, int w, int h, int c);
void fillScreen(int c);
void box(int x, int y, int w, int h, int fill, int edge);
int  textW(const char *t, int sc);
void text(int x, int y, const char *t, int col, int sc);
void textC(int y, const char *t, int col, int sc);                 // centred on the screen
void textCW(int x0, int w, int y, const char *t, int col, int sc); // centred in [x0, x0 + w)
int  textSW(const char *t);
void textS(int x, int y, const char *t, int col);         // small font, no outline
void scoreStr(char *o, int doubled);
void drawFlag(int x, int y, int w, int h, int nation);
void drawLogo(int x, int y);
void powerMarker(int x0, int y0, float ux, float uy, float len, float power);
void uiPalette(void);             // the fixed colours 0-63
enum { PAL_MENU, PAL_FIELD, PAL_SLOT, PAL_MINI, PAL_FIDGET, PAL_BOWL, PAL_STACK, PAL_FLIP, PAL_DOZER, PAL_PIN, PAL_JUMP, PAL_BALL /* +machine: keep last */ };
void scenePalette(int which);      // 64-255: the menus (logo, coin), the field, or the slot machine
void drawImg(const u8 *img, int w, int h, int x, int y);   // a picture, colour 0 see-through
void stipple(int x, int y, int w, int h, int c);            // a checkerboard of colour c (dims what's under it)
void coinCount(int x, int y, int slotPal);              // the coin and your balance
float fsqrt(float v); float fatan2r(float y, float x); float fabsf_(float v); float fsin(float a); float fcos(float a); float frand(void);

// ── buttons: drawn + navigated the same way on every screen ───────────────
typedef struct { int x, y, w, h; char label[28]; int col; int dim; } Btn;
void drawBtns(Btn *b, int n, int sel);
int  btnInput(Btn *b, int n, int *sel, int cols);   // returns pressed index or -1

// ── sound (sound in game.c) ───────────────────────────────────────────────
void sfxThrow(int orient); void sfxPeef(int orient); void sfxPlop(void);
void musicStart(void); void musicStop(void); void musicToggle(void);
enum { MUS_MAIN, MUS_MINI, MUS_BALL, MUS_FIDGET, MUS_BOWL, MUS_STACK, MUS_FLIP, MUS_DOZER, MUS_PIN };
void musicSet(int track);          // switch tracks (restarts only if it changes)
enum { OBJ_MAIN, OBJ_MINI, OBJ_BALL, OBJ_FIDGET, OBJ_BOWL, OBJ_STACK, OBJ_FLIP, OBJ_DOZER, OBJ_PIN, OBJ_JUMP, OBJ_JUMPMENU };
void objSet(int set);              // the sprite pictures this screen needs (loaded when it changes)

// ── the match (game.c) ────────────────────────────────────────────────────
extern int score2[2], player, p2Round, frameCount, roundFrames, totalFrames, suddenDeath;
extern int orient;
void gameInit(void);               // sprite pictures, the panel
void startMatch(void);
void matchUpdate(void);
void matchDraw(void);
void matchInput(int down, int held, int up);
void hideSprites(void);
extern int matchOver;

// ── Olympics (extras.c) ───────────────────────────────────────────────────
#define NATION_COUNT 18
extern const char *NATION[NATION_COUNT];
extern const char *NATION_CODE[NATION_COUNT];
extern int olyNation;
void olyNew(void);
void olyAfterMatch(int playerScore2);
void drawOlySelect(void); void inputOlySelect(void);
void drawOlyBracket(void); void inputOlyBracket(void);
extern int olyFinished;

// ── the slot machine and PlinQu33ph (casino.c) ─────────────────────────────
void drawSlot(void); void inputSlot(void); void updateSlot(void);
void drawPlinko(void); void inputPlinko(void); void updatePlinko(void); void plinkoEnter(void);
void casinoInit(void);

// ── the arcade and Mini Qu33ph (mini.c) ─────────────────────────────────────
void drawArcade(void); void inputArcade(void);
void drawMiniMenu(void); void inputMiniMenu(void);
void drawMini(void); void inputMini(void); void updateMini(void);
void drawMiniOver(void); void inputMiniOver(void);
void miniObjLoad(void);
void drawBallMenu(void); void inputBallMenu(void);
void drawBall(void); void inputBall(void); void updateBall(void);
void drawBallOver(void); void inputBallOver(void);
void ballObjLoad(void); int ballMachine(void);
void drawFidgetMenu(void); void inputFidgetMenu(void);
void drawFidget(void); void inputFidget(void); void updateFidget(void);
void drawFidgetOver(void); void inputFidgetOver(void);
void fidgetObjLoad(void);
void drawBowlMenu(void); void inputBowlMenu(void);
void drawBowl(void); void inputBowl(void); void updateBowl(void);
void drawBowlOver(void); void inputBowlOver(void);
void bowlObjLoad(void);
void drawStackMenu(void); void inputStackMenu(void);
void drawStack(void); void inputStack(void); void updateStack(void);
void drawStackOver(void); void inputStackOver(void);
void stackObjLoad(void);
void drawFlipMenu(void); void inputFlipMenu(void); void drawFlipLevels(void); void inputFlipLevels(void);
void drawFlip(void); void inputFlip(void); void updateFlip(void);
void drawFlipOver(void); void inputFlipOver(void);
void flipObjLoad(void);
void drawDozerMenu(void); void inputDozerMenu(void);
void drawDozer(void); void inputDozer(void); void updateDozer(void);
void drawDozerOver(void); void inputDozerOver(void);
void dozerObjLoad(void);
void drawPinMenu(void); void inputPinMenu(void);
void drawPin(void); void inputPin(void); void updatePin(void);
void drawPinOver(void); void inputPinOver(void);
void pinObjLoad(void);
void drawJumpMenu(void); void inputJumpMenu(void); void drawJumpChars(void); void inputJumpChars(void);
void drawJump(void); void inputJump(void); void updateJump(void);
void drawJumpOver(void); void inputJumpOver(void);
void jumpObjLoad(void); void jumpMenuObjLoad(void); void jumpInit(void);
extern u16 jump_pal[192];

// ── toast: short message (new high score) ─────────────────────────────────
void toast(const char *a, const char *b);
void drawToast(void);

void nameEntry(int returnScreen, HighEntry *list, int score2);

// this frame's input (set once per frame in main.c)
extern int kDown, kHeld, kUp;

void goScreen(int s);
void startOlympicMatch(void);
void olympicsDone(void);
int  olyTournamentTotal(void);
