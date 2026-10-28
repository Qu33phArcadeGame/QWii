// save.c — progress kept in the cartridge's battery-backed save memory (SRAM), high scores, records
#include "qu.h"

SaveData sv EWRAM_BSS;
int saveOK;
#define SAVE_MAGIC 0x47335551   // "QU3G"
#include <stddef.h>
#define SAVE_VERSION 3
#define SAVE_V1_SIZE offsetof(SaveData, arcadeBest)   // version 1 saves end where the arcade fields begin
#define SAVE_V2_SIZE offsetof(SaveData, jumpBest)     // version 2 saves end where Jump's fields begin
// Flash carts and emulators look for this text to know the game saves to SRAM.
const char saveTypeTag[] __attribute__((aligned(4), used)) = "SRAM_V113";

static void defaults(void) {
    memset(&sv, 0, sizeof sv);
    sv.magic = SAVE_MAGIC; sv.version = SAVE_VERSION;
    sv.musicOn = 1; sv.sfxOn = 1; sv.twoRounds = 5; sv.timer1p = 30; sv.timer2p = 15; sv.orient = 0;
}
// a simple checksum after the data, so a blank or scrambled save memory starts fresh
static u32 sumN(const void *s, unsigned n) { const u8 *p = s; u32 h = 2166136261u; for (unsigned i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u; return h; }
static u32 sum(const SaveData *s) { return sumN(s, sizeof *s); }
static SaveData lastSaved EWRAM_BSS; static int haveSaved;
void saveInit(void) {
    (void)*(volatile const char *)saveTypeTag;       // (keeps the tag in the cartridge)
    defaults();
    SaveData t; u32 chk;
    sramRead(&t, 0, sizeof t); sramRead(&chk, sizeof t, 4);
    int ok = t.magic == SAVE_MAGIC && t.version == SAVE_VERSION && chk == sum(&t);
    // an older save: keep all of it; whatever's been added since starts at 0
    static const unsigned OLD_SIZE[2] = { SAVE_V2_SIZE, SAVE_V1_SIZE }; static const u32 OLD_VER[2] = { 2, 1 };
    for (int o = 0; o < 2 && !ok; o++) {
        memset(&t, 0, sizeof t);
        sramRead(&t, 0, OLD_SIZE[o]); sramRead(&chk, OLD_SIZE[o], 4);
        if (t.magic == SAVE_MAGIC && t.version == OLD_VER[o] && chk == sumN(&t, OLD_SIZE[o])) { t.version = SAVE_VERSION; ok = 1; }
    }
    if (ok) {
        sv = t;
        for (int i = 0; i < 10; i++) { sv.high1p[i].name[8] = 0; sv.highOly[i].name[8] = 0; }
        sv.name[8] = 0;
        if (t.version == SAVE_VERSION) { lastSaved = sv; haveSaved = 1; }
    }
    saveOK = 1;
    saveWrite();
}
// only write when something actually changed since the last write
void saveWrite(void) {
    if (!saveOK || (haveSaved && !memcmp(&lastSaved, &sv, sizeof sv))) return;
    u32 chk = sum(&sv);
    sramWrite(0, &sv, sizeof sv); sramWrite(sizeof sv, &chk, 4);
    lastSaved = sv; haveSaved = 1;
}
void resetHighScores(void) { memset(sv.high1p, 0, sizeof sv.high1p); memset(sv.highOly, 0, sizeof sv.highOly); saveWrite(); }
void resetEverything(void) { defaults(); saveWrite(); }

// ── the career record ─────────────────────────────────────────────────────
void addCoins(int n) { sv.coins += n; if (n > 0) sv.coinsEarned += n; }
void spendCoins(int n) { sv.coins -= n; sv.coinsSpent += n; }
void trackGameEnd(int s2) { (void)s2; sv.games++; }
void trackQu33ph(int sd) { (void)sd; sv.qu33phs++; }
void trackMega(void) { sv.megas++; }
void trackPeef(void) { sv.peefs++; }

// ── Top 10 tables ─────────────────────────────────────────────────────────
int highQualifies(HighEntry *l, int s2) { return s2 > 0 && (!l[9].name[0] || s2 > l[9].score2); }
void highInsert(HighEntry *l, const char *name, int s2) {
    int i = 9;
    while (i > 0 && (!l[i - 1].name[0] || s2 > l[i - 1].score2)) { l[i] = l[i - 1]; i--; }
    strncpy(l[i].name, name, 8); l[i].name[8] = 0; l[i].score2 = s2;
    saveWrite();
}
