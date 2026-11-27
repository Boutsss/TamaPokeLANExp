// The Battle Tower and gym rematches, as LOGIC: tower.cpp against the real
// Ported from vunguyen2712/TamaPoke with this base's names (Inventory, IT_*).
// Pet, Bag and dex. The screens and the fights are driven through the real
// sketch in tower_ui_test and rematch_ui_test; this one measures the numbers.
//
// Strength is MEASURED, not assumed: the mean base-stat total and the IV of
// generated teams are computed from what towerTeamFor() returns, and the
// rematch teams are compared against the real rosters slot by slot.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "tower.h"
#include "inventory.h"
#include "noart.h"
#include "game_events.h"
#include "moves.h"
#include "casino.h"
#include "avatars.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 9; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
static uint32_t g_ms = 0; uint32_t millis() { return g_ms; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static int evWin = 0, evLoss = 0, evRm = 0;
static int16_t lastWinArg = -1, lastLossArg = -1, lastRmArg = -1;
static bool winArgsInOrder = true;
static void sink(GameEv ev, int16_t arg) {
  if (ev == EV_TOWER_WIN) { evWin++; if (arg != lastWinArg + 1 && !(arg == 1)) winArgsInOrder = false; lastWinArg = arg; }
  if (ev == EV_TOWER_LOSS) { evLoss++; lastLossArg = arg; }
  if (ev == EV_REMATCH_WIN) { evRm++; lastRmArg = arg; }
}

// TrainerMon has a padding byte, so compare fields, never memcmp
static bool sameTeam(const TrainerMon *a, const TrainerMon *b, int n) {
  for (int i = 0; i < n; i++) if (a[i].dex != b[i].dex || a[i].level != b[i].level) return false;
  return true;
}

static bool hasDamagingMove(int16_t dex, uint8_t lvl) {
  Pet f; f.dbgHatchAs(dex, false);
  f.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  f.relearnFromLevel();
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (f.moves[i] && MOVE_TBL[f.moves[i]].cat != MC_STATUS) return true;
  return false;
}

int main() {
  gameEventSubscribe(sink);
  Pet p; p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  if (p.isEgg()) p.dbgHatchAs(4, false);
  bag.begin(); bag.clear();
  towerBegin(); towerClearAll();

  // ---- unlocking
  ck(!towerUnlocked(), "unbound: the tower is locked");
  towerBindPet(&p);
  ck(!towerUnlocked(), "fresh save: locked");
  p.winBadge(0, TRAINER_COUNT - 2, false);
  ck(!towerUnlocked(), "the last Elite 4 member is not enough");
  p.winBadge(3, TRAINER_COUNT - 1, true);
  ck(towerUnlocked(), "a champion beaten on HARD, in Sinnoh, unlocks it");
  {
    Pet q; q.begin();
    ck(towerUnlockedFor(q), "and that survives a reload of the badges");
  }
  {
    Pet e; e.badges = 0; e.badgesHard = 0;
    for (int r = 0; r < GYM_REGIONS - 1; r++) { e.badgesX[r] = 0; e.badgesHardX[r] = 0; }
    ck(!towerUnlockedFor(e), "(a blank creature is locked)");
    e.winBadge(GYM_REGIONS - 1, TRAINER_COUNT - 1, false);
    ck(towerUnlockedFor(e), "a champion on EASY in the last ladder unlocks it too");
  }

  // ---- generated teams
  bool allValid = true, noDup = true, allDrawable = true, allLv50 = true, inBand = true,
       noEarlyLegend = true, determ = true, smartOk = true, ivMono = true, movesOk = true;
  uint8_t lastIv = 0;
  const uint32_t seeds[4] = { 1, 77, 12345, 0xDEADBEEF };
  for (uint16_t fl = 1; fl <= 40; fl++) {
    uint8_t iv = towerIvFor(fl);
    if (iv < lastIv) ivMono = false;
    lastIv = iv;
    if (towerSmartFor(fl) != (fl >= TOWER_SMART_FLOOR)) smartOk = false;
    uint16_t lo, hi; towerBand(fl, lo, hi);
    for (int si = 0; si < 4; si++) {
      TowerTeam t, t2;
      if (!towerTeamFor(fl, seeds[si], t) || t.count != TOWER_TEAM) { allValid = false; continue; }
      towerTeamFor(fl, seeds[si], t2);
      if (!sameTeam(t.mon, t2.mon, TOWER_TEAM)) determ = false;
      if (t.iv != iv) allValid = false;
      for (int a = 0; a < t.count; a++) {
        int16_t d = (int16_t)t.mon[a].dex;
        if (!towerDrawable(d) || !speciesHasArt(d)) allDrawable = false;
        if (t.mon[a].level != TOWER_LEVEL) allLv50 = false;
        if (speciesBst(d) < lo || speciesBst(d) > hi) inBand = false;
        if (fl < TOWER_LEGEND_FLOOR && speciesLegendary(d)) noEarlyLegend = false;
        for (int b = 0; b < a; b++) if (t.mon[b].dex == t.mon[a].dex) noDup = false;
        if (si < 2 && !hasDamagingMove(d, TOWER_LEVEL)) {
          printf("      floor %u: %s has no damaging move at 50\n", fl, DEX_TBL[d].name);
          movesOk = false;
        }
      }
    }
  }
  ck(allValid, "every floor 1-40 fields a full team of 3 at the floor's IV");
  ck(noDup, "no duplicate within a team");
  ck(allDrawable, "every creature can be drawn (art + pack)");
  ck(allLv50, "every foe is level 50");
  ck(inBand, "every foe sits in its floor's base-stat band");
  ck(noEarlyLegend, "no legendary before floor 22");
  ck(determ, "a floor's team is deterministic for its seed (what is shown is what is fought)");
  ck(smartOk, "hard AI exactly from floor 7");
  ck(ivMono && towerIvFor(1) == EASY_IV && towerIvFor(15) == HARD_IV && towerIvFor(99) == HARD_IV,
     "IVs climb EASY_IV (floor 1) -> HARD_IV (floor 15) and stay there");
  ck(movesOk, "every foe has a damaging move at level 50");
  {
    TowerTeam a, b; towerTeamFor(1, 1, a); towerTeamFor(1, 2, b);
    ck(!sameTeam(a.mon, b.mon, TOWER_TEAM), "a different run climbs a different tower");
  }
  // strength, measured over 40 seeds
  {
    auto meanBst = [](uint16_t f0, uint16_t f1) {
      uint32_t sum = 0, n = 0;
      for (uint32_t s = 1; s <= 40; s++)
        for (uint16_t f = f0; f <= f1; f++) {
          TowerTeam t; towerTeamFor(f, s * 2654435761u, t);
          for (int k = 0; k < t.count; k++) { sum += speciesBst((int16_t)t.mon[k].dex); n++; }
        }
      return n ? sum / n : 0;
    };
    uint32_t early = meanBst(1, 3), mid = meanBst(10, 12), late = meanBst(25, 27);
    printf("      mean BST: floors 1-3 = %u, 10-12 = %u, 25-27 = %u\n", early, mid, late);
    ck(early + 60 < mid && mid + 60 < late, "teams get measurably stronger with the floor");
    int legends = 0;
    for (uint32_t s = 1; s <= 40; s++) {
      TowerTeam t; towerTeamFor(30, s, t);
      for (int k = 0; k < t.count; k++) legends += speciesLegendary((int16_t)t.mon[k].dex);
    }
    printf("      legendaries on floor 30 over 40 runs: %d\n", legends);
    ck(legends > 0, "and legendaries do turn up late (the gate is real, not a blanket ban)");
  }
  // only Kanto on the card: still a full team, all from Kanto
  {
    uint16_t keep = gRegionArt;
    gRegionArt = 1;   // Kanto only
    bool ok = true;
    for (uint16_t fl : { 1, 9, 30 }) {
      TowerTeam t;
      if (!towerTeamFor(fl, 5, t) || t.count != TOWER_TEAM) ok = false;
      for (int k = 0; k < t.count; k++) if (t.mon[k].dex > 151) ok = false;
    }
    ck(ok, "with only the Kanto pack, a full team and every creature from Kanto");
    gRegionArt = keep;
  }

  // ---- streak, best, rewards
  ck(towerStreak() == 0 && towerBest() == 0 && !towerRunActive(), "a fresh tower: streak 0, best 0");
  bool rwOnlyAt7 = true;
  for (uint16_t s = 0; s <= 100; s++) { TowerReward r; if (towerRewardFor(s, r) != (s && s % 7 == 0)) rwOnlyAt7 = false; }
  ck(rwOnlyAt7, "a reward exactly at every multiple of 7");
  uint8_t ub0 = bag.count(IT_ULTRABALL);
  // v3.56: any floor may also roll a held item -- on floors 1-10 a C-tier one
  // and nothing else, ever.
  bool noEarly = true;
  for (int i = 1; i <= 6; i++) {
    TowerReward g; towerWin(&g);
    for (uint8_t k = 0; k < g.n; k++) if (itemTier(g.item[k]) != HT_C || g.qty[k] != 1) noEarly = false;
  }
  ck(noEarly && bag.count(IT_ULTRABALL) == ub0, "wins 1-6 pay no balls, and at most a C-tier held item");
  ck(towerStreak() == 6 && towerBest() == 6 && towerRunActive(), "six wins: streak 6, best 6, run active");
  TowerReward g7; towerWin(&g7);
  ck(g7.n == 3 && bag.count(IT_ULTRABALL) == ub0 + 3 && bag.count(IT_SITRUS_BERRY) == 2 &&
     itemTier(g7.item[2]) == HT_C,
     "the 7th win bags 3 ULTRA BALL + 2 SITRUS BERRY + a certain C-tier held item");
  towerBegin();          // a reload: the run is on flash
  {
    Inventory b2; b2.begin();
    ck(towerStreak() == 7 && towerBest() == 7, "the streak and the best survive a reload");
    ck(b2.count(IT_ULTRABALL) == ub0 + 3, "and a reload does not pay the reward again");
  }
  TowerReward g8; towerWin(&g8);
  ck(g8.n <= 1 && bag.count(IT_ULTRABALL) == ub0 + 3, "the 8th win pays no balls (once, not every win after 7)");
  for (int i = 9; i <= 13; i++) towerWin(nullptr);
  TowerReward g14; towerWin(&g14);
  ck(g14.n == 3 && bag.count(IT_LUM_BERRY) >= 2 && bag.count(IT_ULTRABALL) == ub0 + 6 &&
     itemTier(g14.item[2]) >= HT_C && itemTier(g14.item[2]) <= HT_B,
     "the 14th: 3 ULTRA BALL + 2 LUM BERRY + a B- or C-tier held item");

  // ---- the reward tiers (v3.56). Each guard below is phrased against the
  // RULE, and the positive companions prove the tiers are actually reached.
  {
    bool onlyC = true, bAt11 = false, noAS49 = true, aAt50 = false, sAt99 = false, onlyAS = true;
    int b11 = 0, b49 = 0;
    for (int r = 0; r < 100; r++) {
      for (int f = 1; f <= 10; f++) onlyC &= towerTierFor(f, r) == HT_C;
      if (towerTierFor(11, r) == HT_B) { bAt11 = true; b11++; }
      if (towerTierFor(49, r) == HT_B) b49++;
      for (int f = 11; f < 50; f++) noAS49 &= towerTierFor(f, r) <= HT_B;
      aAt50 |= towerTierFor(50, r) == HT_A;
      sAt99 |= towerTierFor(99, r) == HT_S;
      onlyAS &= towerTierFor(100, r) >= HT_A && towerTierFor(250, r) >= HT_A;
    }
    ck(onlyC, "floors 1-10 roll C only");
    ck(bAt11 && noAS49, "B from floor 11, and no A/S below 50");
    ck(b49 > b11, "the B share climbs between 11 and 49");
    ck(aAt50 && sAt99, "A from floor 50, and S by 99");
    ck(onlyAS, "the top floor and endless: A/S only");
    bool own = true;
    for (uint8_t t = HT_C; t <= HT_S; t++)
      for (uint32_t r = 0; r < 600; r += 3) { ItemKey k = towerItemOfTier(t, r); own &= k && itemTier(k) == t && itemIsHeld(k); }
    ck(own, "a tier hands out only its own held items");
  }
  ck(evWin == 14 && lastWinArg == 14 && winArgsInOrder, "EV_TOWER_WIN fired 14 times, arg = the new streak");
  uint32_t seedBefore = towerSeed();
  towerLoss();
  ck(towerStreak() == 0 && towerBest() == 14 && !towerRunActive(), "a loss ends the run; the best is kept");
  ck(evLoss == 1 && lastLossArg == 14, "EV_TOWER_LOSS carries the streak that ended");
  ck(towerSeed() != seedBefore, "and the next run gets new floors");
  towerBegin();
  ck(towerStreak() == 0 && towerBest() == 14, "reset and best both on flash");
  towerWin(nullptr);
  ck(towerStreak() == 1 && towerBest() == 14, "a new run starts from 1 under the old best");
  towerStartRun(4242);
  ck(towerStreak() == 0 && towerSeed() == 4242, "towerStartRun seeds a fresh run");

  // ---- rematches
  ck(rematchLevel(12) == 25 && rematchLevel(50) == 72 && rematchLevel(60) == 85 &&
     rematchLevel(72) == 100 && rematchLevel(90) == 100, "rematch level = lvl*5/4 + 10, capped at 100");
  {
    Pet q; q.badges = 0; q.badgesHard = 0;
    for (int r = 0; r < GYM_REGIONS - 1; r++) { q.badgesX[r] = 0; q.badgesHardX[r] = 0; }
    ck(!rematchUnlockedFor(q, 1), "rematch locked before the champion");
    q.winBadge(1, TRAINER_COUNT - 1, true);
    ck(!rematchUnlockedFor(q, 1), "a HARD champion win does not open the EASY rematch");
    q.winBadge(1, TRAINER_COUNT - 1, false);
    ck(rematchUnlockedFor(q, 1) && !rematchUnlockedFor(q, 0), "the easy champion opens THAT region only");
    ck(rematchOpenFor(q, 1, 0) && !rematchOpenFor(q, 1, 1), "rematches are sequential");
    uint8_t ub = bag.count(IT_ULTRABALL), ob = bag.count(IT_ORAN_BERRY);
    uint16_t b0 = q.badges, bx = q.badgesX[0];
    int ev0 = evRm;
    TowerReward r;
    bool first = rematchWin(1, 0, &r);
    ck(first && rematchWon(1, 0) && rematchMask(1) == 1 && rematchMask(0) == 0,
       "a rematch win lands in ITS region's rematch mask");
    ck(q.badges == b0 && q.badgesX[0] == bx, "and in no badge mask");
    ck(bag.count(IT_ORAN_BERRY) == ob + 1 && bag.count(IT_ULTRABALL) == ub + 2,
       "first win: an ORAN BERRY and 2 ULTRA BALLs");
    ck(evRm == ev0 + 1 && lastRmArg == 0, "EV_REMATCH_WIN with the trainer index");
    ck(rematchOpenFor(q, 1, 1), "and the next rematch opens");
    first = rematchWin(1, 0, &r);
    ck(!first && bag.count(IT_ORAN_BERRY) == ob + 2 && bag.count(IT_ULTRABALL) == ub + 2,
       "a repeat win: the berry only");
    towerBegin();
    ck(rematchWon(1, 0) && !rematchWon(0, 0), "the rematch mask survives a reload");
  }
  // additive storage: an older, shorter blob reads into the front
  {
    Preferences pr; pr.begin("tamapoke", false);
    uint16_t two[2] = { 0x0005, 0x0002 };
    pr.putBytes("rmWin", two, sizeof(two));
    towerBegin();
    ck(rematchMask(0) == 5 && rematchMask(1) == 2 && rematchMask(2) == 0,
       "a shorter rmWin blob (fewer ladders) keeps its meaning");
  }
  // stronger teams, for every trainer of every ladder
  {
    bool full = true, raised = true, stronger = true, drawable = true, themed = true,
         aceLast = true, stable = true;
    for (uint8_t rg = 0; rg < GYM_REGIONS; rg++)
      for (uint8_t i = 0; i < TRAINER_COUNT; i++) {
        const Trainer &t = TRAINER_SETS[rg].list[i];
        TrainerMon m[TRAINER_TEAM_MAX], m2[TRAINER_TEAM_MAX];
        uint8_t n = rematchTeam(rg, i, m);
        rematchTeam(rg, i, m2);
        if (!sameTeam(m, m2, n)) stable = false;
        if (n != TRAINER_TEAM_MAX) { full = false; printf("      %s %s: %u\n", TRAINER_SETS[rg].region, t.name, n); }
        uint32_t ob = 0, nb = 0, ol = 0, nl = 0;
        for (int k = 0; k < t.count; k++) { ob += speciesBst((int16_t)t.team[k].dex); ol += t.team[k].level; }
        for (int k = 0; k < n; k++) { nb += speciesBst((int16_t)m[k].dex); nl += m[k].level; }
        if (nb < ob || nl <= ol || n < t.count) {
          stronger = false;
          printf("      %s %s: levels %u -> %u, BST %u -> %u\n", TRAINER_SETS[rg].region, t.name, ol, nl, ob, nb);
        }
        // the originals are all there, in order, each raised and evolved
        int fillers = n - t.count;
        for (int k = 0; k < t.count; k++) {
          int at = (k == t.count - 1) ? n - 1 : k;   // fillers go before the ace
          if (m[at].level != rematchLevel(t.team[k].level)) raised = false;
          if (speciesBst((int16_t)m[at].dex) < speciesBst((int16_t)t.team[k].dex)) raised = false;
          if (k == t.count - 1 && evolveForLevel((int16_t)t.team[k].dex, m[at].level) != (int16_t)m[at].dex) aceLast = false;
        }
        for (int k = t.count - 1; k < t.count - 1 + fillers; k++) {
          const DexEntry &e = DEX_TBL[m[k].dex];
          if (!towerDrawable((int16_t)m[k].dex) || speciesLegendary((int16_t)m[k].dex)) drawable = false;
          if (e.type1 != t.type && e.type2 != t.type) themed = false;
        }
      }
    ck(full, "every rematch team is filled to six");
    ck(raised, "every original creature returns at its raised level, never weaker");
    ck(aceLast, "the leader's ace still closes the team");
    ck(stronger, "every rematch team: more total levels, no less total base stats, no fewer creatures");
    ck(drawable, "fillers are drawable and never legendary");
    ck(themed, "fillers keep the leader's type");
    ck(stable, "a rematch team is the same every time it is built");
    TrainerMon m[TRAINER_TEAM_MAX];
    uint8_t n = rematchTeam(0, 0, m);
    printf("      KANTO BROCK rematch:");
    for (int k = 0; k < n; k++) printf(" %s L%u", DEX_TBL[m[k].dex].name, m[k].level);
    printf("\n");
  }

  // ---- floor 100: a legendary costume, at random, from those not owned yet.
  // LAST on purpose: it climbs 300 floors, which fills the bag with rewards.
  {
    casinoBegin();
    auto ownedTower = []() { int n = 0; for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) n += casinoOwns(PK_AVATAR, TOWER_AVATARS[k]); return n; };
    auto isTower = [](int16_t av) {
      if (av < AVATAR_BASE_COUNT) return false;
      for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) if (TOWER_AVATARS[k] == av - AVATAR_BASE_COUNT) return true;
      return false;
    };
    ck(TOWER_AVATAR_COUNT == 94 && ownedTower() == 0, "94 legendary costumes, none owned at first");
    bool auraAll = true;
    for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) if (!(AVATARS[AVATAR_BASE_COUNT + TOWER_AVATARS[k]].fx & AVFX_AURA)) auraAll = false;
    int auraOthers = 0;
    for (uint16_t a = 0; a < AVATAR_COUNT; a++) if ((AVATARS[a].fx & AVFX_AURA) && !isTower(a)) auraOthers++;
    ck(auraAll && auraOthers == 0, "every legendary costume glows, and nothing else does");
    uint32_t c = 60000;
    ck(!casinoPrizeSold(PK_AVATAR, TOWER_AVATARS[0]) && !casinoBuy(PK_AVATAR, TOWER_AVATARS[0], c, [](uint32_t){ return true; }),
       "a legendary costume is not sold at the Game Corner");
    ck(!casinoGrantAvatar(0) && !casinoOwns(PK_AVATAR, 0), "and the tower cannot hand out an ordinary prize");

    towerStartRun(77);
    bool early = false;
    for (int f = 1; f < TOWER_AVATAR_FLOOR; f++) { TowerReward g; towerWin(&g); if (g.avatar >= 0) early = true; }
    ck(!early && ownedTower() == 0, "floors 1-99 unlock no costume");
    randomSeed(11);
    TowerReward g100; towerWin(&g100);
    ck(towerStreak() == 100 && isTower(g100.avatar) && casinoOwns(PK_AVATAR, g100.avatar - AVATAR_BASE_COUNT) && ownedTower() == 1,
       "clearing floor 100 unlocks exactly one legendary costume, and says which");
    int as100 = 0;
    for (uint8_t k = 0; k < g100.n; k++) if (itemTier(g100.item[k]) >= HT_A) as100++;
    ck(as100 == TOWER_TOP_ITEMS, "and pays four A/S held items");
    TowerReward g101; towerWin(&g101);
    ck(g101.avatar < 0 && ownedTower() == 1, "floor 101 does not unlock another");
    ck(g101.n == 1 && g101.item[0] == IT_MASTERBALL, "endless: a MASTER BALL a floor");
    for (int f = 102; f < 105; f++) towerWin(nullptr);
    TowerReward g105; towerWin(&g105);
    ck(g105.n == 2 && g105.item[0] == IT_MASTERBALL && itemTier(g105.item[1]) >= HT_A,
       "and every 5th floor an A/S held item too");
    casinoBegin();
    ck(casinoOwns(PK_AVATAR, g100.avatar - AVATAR_BASE_COUNT), "the costume is saved");

    towerLoss();
    for (int f = 1; f < TOWER_AVATAR_FLOOR; f++) towerWin(nullptr);
    TowerReward again; towerWin(&again);
    ck(isTower(again.avatar) && again.avatar != g100.avatar && ownedTower() == 2,
       "the next run to clear 100 unlocks a different one");

    // random among those left, not the first in the list every time
    int firstPicks = 0;
    for (int t = 0; t < 8; t++) {
      randomSeed(100 + t);
      int16_t a = towerGrantAvatar();
      if (a >= 0) {
        // was it the FIRST unowned one in the list? (only lower ones count, and
        // the grant itself has just been recorded, so it is not among them)
        bool lowerFree = false;
        for (uint16_t k = 0; k < TOWER_AVATAR_COUNT && TOWER_AVATARS[k] < a - AVATAR_BASE_COUNT; k++)
          if (!casinoOwns(PK_AVATAR, TOWER_AVATARS[k])) lowerFree = true;
        if (!lowerFree) firstPicks++;
      }
    }
    ck(firstPicks < 8, "the pick is random among the costumes left");

    for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) casinoGrantAvatar(TOWER_AVATARS[k]);
    ck(ownedTower() == TOWER_AVATAR_COUNT && towerGrantAvatar() == -1, "with all 94 owned there is nothing left to give");
    towerLoss();
    for (int f = 1; f < TOWER_AVATAR_FLOOR; f++) towerWin(nullptr);
    TowerReward full; towerWin(&full);
    ck(full.avatar < 0 && towerStreak() == 100, "and floor 100 then still counts, with no costume");
  }

  // ---- the run's team and the checkpoint (v3.56)
  {
    towerLoss();
    ck(!towerRunActive() && towerTeam(nullptr) == 0, "no run, no team");
    PartyMon t[TOWER_TEAM];
    for (int i = 0; i < TOWER_TEAM; i++) { t[i] = PartyMon(); t[i].dex = (uint16_t)(1 + 3 * i); t[i].level = 30; }
    ck(towerSetTeam(t, TOWER_TEAM) && towerRunActive(), "locking a team in is a run, before any win");
    t[0].dex = 150;                                  // the party changes...
    PartyMon got[TOWER_TEAM];
    ck(towerTeam(got) == TOWER_TEAM && got[0].dex == 1, "...and the run's copy does not");
    towerFightBegin();
    towerBegin();                                    // the board went down mid-fight
    ck(towerResumed() && towerTeam(got) == TOWER_TEAM && got[2].dex == 7 && towerStreak() == 0,
       "a restart mid-fight comes back at the same floor, same team, and says so");
    towerFightBegin();
    ck(!towerResumed(), "the next fight clears the note");
    towerSuspend();
    ck(towerResumed() && towerRunActive(), "leaving a fight keeps the run");
    towerWin(nullptr);
    towerBegin();
    ck(!towerResumed() && towerStreak() == 1, "a won floor is no checkpoint to report");
    towerLoss();
    towerBegin();
    ck(towerTeam(nullptr) == 0 && !towerRunActive(), "a loss releases the team, on flash too");
  }

  printf(bad ? "tower_test: %d FAILED\n" : "tower_test: all passed\n", bad);
  return bad ? 1 : 0;
}
