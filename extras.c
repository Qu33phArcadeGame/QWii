// extras.c — Qu33ph Olympics (the DS version's rules, laid out for one screen)
#include "qu.h"

const char *NATION[NATION_COUNT] = { "Ireland", "Israel", "USA", "South Africa", "England", "Scotland", "Norway", "Japan", "Germany",
                                     "Brazil", "Australia", "Canada", "France", "Netherlands", "Sweden", "Russia", "Iran", "China" };
const char *NATION_CODE[NATION_COUNT] = { "IRL", "ISR", "USA", "RSA", "ENG", "SCO", "NOR", "JPN", "GER", "BRA", "AUS", "CAN", "FRA", "NED", "SWE", "RUS", "IRN", "CHN" };
#define CODE NATION_CODE
int olyNation, olyFinished;
static int olyN[8];                                  // the 8 nations; the player is olyN[7]
static int qf1[2], qf2[2], qf3[2], qf4[2], sf1[2], sf2[2], fin[2];   // scores (doubled), -1 = not played
int olySpecial;                                       // playing the Special Olympics (a second chance after losing)
static int olyRound, olyOut, olyMedal, olyTotal, olySel, olyCoins;
static const int REWARD[4] = { 0, 5, 10, 15 };      // win QF = 5, SF = 10, FINAL = 15 coins
static const int SPECIAL_REWARD[4] = { 0, 1, 2, 3 };  // the Special Olympics pays less (as the website)
static void olyReward(int round) { int r = (olySpecial ? SPECIAL_REWARD : REWARD)[round]; addCoins(r); olyCoins += r; }

// Rival scores: the website's formula (85-130), scaled to what's reachable here (as on the DS) —
// a flawless game (a QU33PH every set) scores about 40-45. Raise OLY_SCALE for a harder Olympics.
#define OLY_SCALE 30          // percent of the website's rival scores
static int simScore(void) { int b = 85 + rand() % 45; if (rand() % 10 < 7) b = 90 + rand() % 30; return b * 2 * OLY_SCALE / 100; }
static int winQ(int *m, int a, int b) { return m[0] > m[1] ? a : b; }
static void freshBracket(void) {
    int *qs[3] = { qf1, qf2, qf3 };
    for (int k = 0; k < 3; k++) { qs[k][0] = simScore(); do qs[k][1] = simScore(); while (qs[k][1] == qs[k][0]); }
    qf4[0] = qf4[1] = sf1[0] = sf1[1] = sf2[0] = sf2[1] = fin[0] = fin[1] = -1;
    olyRound = 1; olyOut = 0; olyMedal = 0; olyTotal = 0; olyFinished = 0; olyCoins = 0;
}
void olyNew(void) {
    olySpecial = 0;
    int pool[NATION_COUNT], n = 0;
    for (int i = 0; i < NATION_COUNT; i++) if (i != olyNation) pool[n++] = i;
    for (int i = n - 1; i > 0; i--) { int j = rand() % (i + 1), t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    for (int i = 0; i < 7; i++) olyN[i] = pool[i];
    olyN[7] = olyNation;
    freshBracket();
}
void olyAfterMatch(int p) {
    int opp; do opp = simScore(); while (opp == p);
    olyTotal += p;
    if (olyRound == 1) {
        qf4[0] = opp; qf4[1] = p;                    // olyN[6] vs player
        sf1[0] = simScore(); do sf1[1] = simScore(); while (sf1[1] == sf1[0]);
        if (p <= opp) { olyOut = 1; olyFinished = 1; }
        else { olyReward(1); olyRound = 2; }
    } else if (olyRound == 2) {
        sf2[0] = opp; sf2[1] = p;                    // QF3 winner vs player
        if (p <= opp) { olyOut = 1; olyMedal = 1; olyFinished = 1; }
        else { olyReward(2); olyRound = 3; }
    } else {
        fin[0] = opp; fin[1] = p;                    // SF1 winner vs player
        olyMedal = p > opp ? 3 : 2;
        if (olyMedal == 3) olyReward(3);
        olyFinished = 1;
    }
    if (olyFinished && !olySpecial) {                // (Special Olympics medals don't count toward the real ones)
        if (olyMedal == 3) sv.gold++; else if (olyMedal == 2) sv.silver++; else if (olyMedal == 1) sv.bronze++;
    }
    saveWrite();
}
int olyTournamentTotal(void) { return olyTotal; }
// The website's SPECIAL OLYMPICS: if you don't win gold you can enter a second bracket made of
// the nations knocked out of the main one (topped up with others).
static void olySpecialNew(void) {
    int pool[NATION_COUNT], n = 0, used[NATION_COUNT] = { 0 };
    #define LOSER(m, a, b) do { if ((m)[0] >= 0) { int l = (m)[0] > (m)[1] ? (b) : (a); if (l != olyNation && !used[l]) { used[l] = 1; pool[n++] = l; } } } while (0)
    int w1 = winQ(qf1, olyN[0], olyN[1]), w2 = winQ(qf2, olyN[2], olyN[3]), w3 = winQ(qf3, olyN[4], olyN[5]);
    LOSER(qf1, olyN[0], olyN[1]); LOSER(qf2, olyN[2], olyN[3]); LOSER(qf3, olyN[4], olyN[5]); LOSER(qf4, olyN[6], olyN[7]);
    LOSER(sf1, w1, w2); LOSER(sf2, w3, olyNation);
    #undef LOSER
    int extra[NATION_COUNT], ne = 0;
    for (int i = 0; i < NATION_COUNT; i++) if (i != olyNation && !used[i]) extra[ne++] = i;
    for (int i = ne - 1; i > 0; i--) { int j = rand() % (i + 1), t = extra[i]; extra[i] = extra[j]; extra[j] = t; }
    for (int i = 0; n < 7 && i < ne; i++) pool[n++] = extra[i];
    for (int i = n - 1; i > 0; i--) { int j = rand() % (i + 1), t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    for (int i = 0; i < 7; i++) olyN[i] = pool[i];
    olyN[7] = olyNation;
    freshBracket(); olySpecial = 1;
}

// ── nation select: D-pad to pick, A to confirm ────────────────────────────
static Btn natB[NATION_COUNT] EWRAM_BSS;
static void natLayout(void) {
    for (int i = 0; i < NATION_COUNT; i++) {
        natB[i].x = 3 + (i % 3) * 79; natB[i].y = 31 + (i / 3) * 21; natB[i].w = 76; natB[i].h = 20;
        natB[i].label[0] = 0; natB[i].col = 0; natB[i].dim = 0;
    }
}
void drawOlySelect(void) {
    natLayout();
    fillScreen(DARK);
    drawFlag(4, 3, 36, 24, olySel);
    textS(46, 2, "QU33PH OLYMPICS - choose your nation", GOLD);
    text(46, 13, NATION[olySel], WHITE, 1);
    drawBtns(natB, NATION_COUNT, olySel);
    for (int i = 0; i < NATION_COUNT; i++) {
        drawFlag(natB[i].x + 11, natB[i].y + 4, 18, 12, i);
        text(natB[i].x + 34, natB[i].y + 3, CODE[i], i == olySel ? YELLOW : WHITE, 1);
    }
}
void inputOlySelect(void) {
    natLayout();
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    int hit = btnInput(natB, NATION_COUNT, &olySel, 3);
    if (hit >= 0) { olyNation = olySel; olyNew(); goScreen(S_OLY_BRACKET); }
}

// ── the bracket ───────────────────────────────────────────────────────────
static void matchRow(int y, int a, int b, int *m, const char *lbl) {
    char s[12];
    textS(6, y + 2, lbl, GREY);
    drawFlag(34, y + 1, 18, 12, a); text(56, y, CODE[a], a == olyNation ? YELLOW : WHITE, 1);
    if (m[0] >= 0) { scoreStr(s, m[0]); text(89, y, s, m[0] > m[1] ? LIME : GREY, 1); }
    textS(126, y + 2, "v", GREY);
    drawFlag(135, y + 1, 18, 12, b); text(157, y, CODE[b], b == olyNation ? YELLOW : WHITE, 1);
    if (m[1] >= 0) { scoreStr(s, m[1]); text(193, y, s, m[1] > m[0] ? LIME : GREY, 1); }
}
static Btn olyBtn[2]; static int olyBtnSel, olyNB;
static void olyButtons(void) {
    if (!olyFinished) {
        const char *r = olyRound == 1 ? "PLAY QUARTER-FINAL" : olyRound == 2 ? "PLAY SEMI-FINAL" : "PLAY THE FINAL";
        olyBtn[0] = (Btn){ 30, 128, 180, 26, "", 0, 0 }; strcpy(olyBtn[0].label, r); olyNB = 1;
    } else if (olyMedal != 3 && !olySpecial) {   // didn't win gold: a second chance in the Special Olympics
        olyBtn[0] = (Btn){ 4, 134, 136, 24, "SPECIAL OLYMPICS", 0, 0 };
        olyBtn[1] = (Btn){ 146, 134, 90, 24, "CONTINUE", 0, 0 }; olyNB = 2;
    } else { olyBtn[0] = (Btn){ 50, 134, 140, 24, "CONTINUE", 0, 0 }; olyNB = 1; }
    if (olyBtnSel >= olyNB) olyBtnSel = 0;
}
void drawOlyBracket(void) {
    fillScreen(DARK);
    textS(4, 1, olySpecial ? "SPECIAL OLYMPICS BRACKET" : "OLYMPIC BRACKET", olySpecial ? C_FO : GOLD);
    textS(SW - 4 - textSW("B quit"), 1, "B quit", GREY);
    matchRow(13, olyN[0], olyN[1], qf1, "QF1"); matchRow(27, olyN[2], olyN[3], qf2, "QF2");
    matchRow(41, olyN[4], olyN[5], qf3, "QF3"); matchRow(55, olyN[6], olyN[7], qf4, "QF4");
    int w1 = winQ(qf1, olyN[0], olyN[1]), w2 = winQ(qf2, olyN[2], olyN[3]), w3 = winQ(qf3, olyN[4], olyN[5]);
    int s1w = sf1[0] >= 0 ? winQ(sf1, w1, w2) : -1;
    if (sf1[0] >= 0) matchRow(71, w1, w2, sf1, "SF1");
    if (olyRound >= 2 || sf2[0] >= 0) matchRow(85, w3, olyNation, sf2, "SF2");
    if (fin[0] >= 0 || olyRound == 3) matchRow(101, s1w >= 0 ? s1w : w1, olyNation, fin, "FIN");
    olyButtons();
    { char s[32]; sprintf(s, "coins won: %d", olyCoins); textS(SW - 4 - textSW(s), 118, s, GOLD); }
    if (olyFinished) {
        const char *m = olyMedal == 3 ? "GOLD MEDAL!" : olyMedal == 2 ? "SILVER MEDAL" : olyMedal == 1 ? "BRONZE MEDAL" : "KNOCKED OUT";
        int c = olyMedal == 3 ? GOLD : olyMedal == 2 ? C_SILVER : olyMedal == 1 ? C_BRONZE : RED;
        textCW(0, 170, 117, m, c, 1);
    }
    drawBtns(olyBtn, olyNB, olyBtnSel);
}
void inputOlyBracket(void) {
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    olyButtons();
    int h = btnInput(olyBtn, olyNB, &olyBtnSel, 1);
    if (h < 0) return;
    if (!olyFinished) startOlympicMatch();
    else if (olyNB == 2 && h == 0) { trackGameEnd(0); saveWrite(); olySpecialNew(); olyBtnSel = 0; goScreen(S_OLY_BRACKET); }   // on to the Special Olympics
    else olympicsDone();
}
