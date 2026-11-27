#include "tower.h"
#include <Preferences.h>
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "game_events.h"
#include "moves.h"
#include "casino.h"
#include "avatar_names.h"

// ---------- state ----------
// Everything here is player-wide, like the badges: it outlives every creature.
// Keys (see save.cpp): twStrk, twBest, twSeed, rmWin. rmWin is one uint16_t
// mask per ladder, stored as bytes so a later ladder grows the blob ADDITIVELY
// (a shorter stored blob reads into the front, exactly like badgX).
static Preferences s_prefs;
static bool s_opened = false;          // nobody begin()'d us: never write
static const Pet *s_pet = nullptr;
static uint16_t s_streak = 0, s_best = 0;
static uint32_t s_seed = 1;
static uint16_t s_rm[GYM_REGIONS] = { 0 };
static PartyMon s_team[TOWER_TEAM];
static uint8_t s_teamN = 0;
static bool s_resumed = false;          // a checkpoint was found at boot, or a fight was left

// "twTeam": [u8 version 1][u8 count][u8 record size][record x count]. The size
// travels with it so a later, longer PartyMon still reads the prefix it knows
// -- the same rule as the party's migration.
#define TW_TEAM_VER 1
static void teamSave() {
  if (!s_opened) return;
  if (!s_teamN) { s_prefs.remove("twTeam"); return; }
  uint8_t buf[3 + sizeof(PartyMon) * TOWER_TEAM];
  buf[0] = TW_TEAM_VER; buf[1] = s_teamN; buf[2] = (uint8_t)sizeof(PartyMon);
  memcpy(buf + 3, s_team, sizeof(PartyMon) * s_teamN);
  size_t len = 3 + sizeof(PartyMon) * s_teamN;
  if (s_prefs.putBytes("twTeam", buf, len) != len) Serial.println("tower: team write failed");
}
static void teamLoad() {
  s_teamN = 0;
  for (auto &m : s_team) m = PartyMon();
  size_t have = s_prefs.getBytesLength("twTeam");
  if (have < 3 || have > 3 + 255 * TOWER_TEAM) return;
  static uint8_t buf[3 + 255 * TOWER_TEAM];
  s_prefs.getBytes("twTeam", buf, have);
  uint8_t n = buf[1], sz = buf[2];
  if (buf[0] != TW_TEAM_VER || !n || n > TOWER_TEAM || !sz || have < 3 + (size_t)sz * n) return;
  for (uint8_t i = 0; i < n; i++) {
    PartyMon m;
    memcpy(&m, buf + 3 + (size_t)sz * i, sz < sizeof(PartyMon) ? sz : sizeof(PartyMon));
    if (m.empty() || m.dex < 1 || m.dex > DEX_COUNT) continue;   // never trust a stored record
    s_team[s_teamN++] = m;
  }
}

bool towerBindPet(const Pet *p) { s_pet = p; return p != nullptr; }

static void towerSave() {
  if (!s_opened) return;
  bool ok = s_prefs.putUShort("twStrk", s_streak) == 2;
  ok = s_prefs.putUShort("twBest", s_best) == 2 && ok;
  ok = s_prefs.putUInt("twSeed", s_seed) == 4 && ok;
  ok = s_prefs.putBytes("rmWin", s_rm, sizeof(s_rm)) == sizeof(s_rm) && ok;
  if (!ok) Serial.println("tower: NVS write failed");
}

void towerBegin() {
  if (!s_opened) s_opened = s_prefs.begin("tamapoke", false);
  if (!s_opened) { Serial.println("tower: NVS would not open"); return; }
  s_streak = s_prefs.getUShort("twStrk", 0);
  s_best = s_prefs.getUShort("twBest", 0);
  s_seed = s_prefs.getUInt("twSeed", 1);
  teamLoad();
  // a fight was under way when the board went down: the run waits at that floor
  if (s_prefs.getUChar("twMid", 0)) { s_resumed = true; s_prefs.putUChar("twMid", 0); }
  memset(s_rm, 0, sizeof(s_rm));
  size_t have = s_prefs.getBytesLength("rmWin");
  if (have) s_prefs.getBytes("rmWin", s_rm, have < sizeof(s_rm) ? have : sizeof(s_rm));
  // never trust a stored value
  if (s_streak > TOWER_STREAK_MAX) s_streak = TOWER_STREAK_MAX;
  if (s_best < s_streak) s_best = s_streak;
  if (!s_seed) s_seed = 1;
  for (uint8_t r = 0; r < GYM_REGIONS; r++) s_rm[r] &= (uint16_t)((1u << TRAINER_COUNT) - 1);
}

void towerClearAll() {
  s_streak = s_best = 0;
  s_seed = 1;
  memset(s_rm, 0, sizeof(s_rm));
  s_teamN = 0;
  s_resumed = false;
  towerSave();
  teamSave();
  if (s_opened) s_prefs.putUChar("twMid", 0);
}

uint8_t towerTeam(PartyMon *out) {
  if (out) for (uint8_t i = 0; i < s_teamN; i++) out[i] = s_team[i];
  return s_teamN;
}
bool towerSetTeam(const PartyMon *m, uint8_t n) {
  if (n > TOWER_TEAM) n = TOWER_TEAM;
  s_teamN = 0;
  for (uint8_t i = 0; i < n; i++) if (!m[i].empty()) s_team[s_teamN++] = m[i];
  teamSave();
  return s_teamN > 0;
}
void towerFightBegin() {
  s_resumed = false;
  if (s_opened) s_prefs.putUChar("twMid", 1);
}
static void fightEnd() { if (s_opened) s_prefs.putUChar("twMid", 0); }
void towerSuspend() { fightEnd(); s_resumed = true; }
bool towerResumed() { return s_resumed && towerRunActive(); }

// ---------- unlocking ----------
bool towerUnlockedFor(const Pet &p) {
  for (uint8_t r = 0; r < GYM_REGIONS; r++)
    if (p.hasBadge(r, TRAINER_COUNT - 1, false) || p.hasBadge(r, TRAINER_COUNT - 1, true))
      return true;
  return false;
}
bool towerUnlocked() { return s_pet && towerUnlockedFor(*s_pet); }

uint16_t towerStreak() { return s_streak; }
uint16_t towerBest() { return s_best; }
bool towerRunActive() { return s_streak > 0 || s_teamN > 0; }
uint32_t towerSeed() { return s_seed; }

void towerStartRun(uint32_t seed) {
  s_streak = 0;
  s_seed = seed ? seed : 1;
  towerSave();
}

// ---------- the species pool ----------
static uint32_t xs(uint32_t &s) {   // xorshift32: the floors must not depend on random()
  s ^= s << 13; s ^= s >> 17; s ^= s << 5;
  return s;
}

// Pre-evolutions, built once. DexEntry only points forward, and EEVEE's
// branches are not in the table at all (EEVEE_EVOS).
static int16_t s_pre[DEX_COUNT + 1];
static bool s_preBuilt = false;
static void buildPre() {
  if (s_preBuilt) return;
  for (int d = 0; d <= DEX_COUNT; d++) s_pre[d] = 0;
  for (int d = 1; d <= DEX_COUNT; d++) {
    int16_t to = (int16_t)DEX_TBL[d].evolvesTo;
    if (to >= 1 && to <= DEX_COUNT && !s_pre[to]) s_pre[to] = (int16_t)d;
  }
  for (int i = 0; i < EEVEE_EVO_COUNT; i++)
    if (EEVEE_EVOS[i] >= 1 && EEVEE_EVOS[i] <= DEX_COUNT) s_pre[EEVEE_EVOS[i]] = DEX_EEVEE;
  s_preBuilt = true;
}

// THE single "can this player be shown this species" answer (pet.cpp), never a
// second copy of it (CLAUDE.md, trap 1).
bool towerDrawable(int16_t dex) { return speciesShowable(dex); }

// The Ultra Beasts are R_RARO in the data -- they are catchable in their games
// -- but they are legendary-class (BST 570) and read as such: a STAKATAKA in
// Brock's rematch is the bug this exists for. Gen 7 only; no other region has
// a class like it.
static bool ultraBeast(int16_t d) { return (d >= 793 && d <= 799) || (d >= 803 && d <= 806); }

bool speciesLegendary(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return false;
  if (ultraBeast(dex)) return true;
  buildPre();
  int16_t root = dex;
  for (int g = 0; g < 6 && s_pre[root]; g++) root = s_pre[root];
  return DEX_TBL[root].rarity == R_LEGENDARIO || DEX_TBL[dex].rarity == R_LEGENDARIO;
}

uint16_t speciesBst(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  const DexEntry &d = DEX_TBL[dex];
  return (uint16_t)d.bHp + d.bAtk + d.bDef + d.bSpe + d.bSpA + d.bSpD;
}

int16_t evolveForLevel(int16_t dex, uint8_t lvl) {
  for (int g = 0; g < 4 && dex >= 1 && dex <= DEX_COUNT; g++) {
    const DexEntry &d = DEX_TBL[dex];
    int16_t to = (int16_t)d.evolvesTo;
    if (to < 1 || to > DEX_COUNT || !d.evolveLevel || d.evolveLevel > lvl) break;
    if (!towerDrawable(to)) break;      // never evolve into something undrawable
    dex = to;
  }
  return dex;
}

// Whether this species, built the way the battle builds a foe (a scratch Pet
// relearning at `lvl`), has at least one damaging move. PYUKUMUKU at 50 does
// not, and a foe that cannot hurt you is a free floor. Checked only on picks,
// not across the whole pool: building a Pet is the expensive part.
static bool canAttackAt(int16_t dex, uint8_t lvl) {
  Pet f;                       // never begin()'d, so it can never save
  f.dbgHatchAs(dex, false);
  f.ageMinutes = (uint32_t)(lvl ? lvl - 1 : 0) * MINUTES_PER_LEVEL;
  f.relearnFromLevel();
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (f.moves[i] && f.moves[i] < MOVE_COUNT && MOVE_TBL[f.moves[i]].cat != MC_STATUS) return true;
  return false;
}

// ---------- tower floors ----------
void towerBand(uint16_t floor, uint16_t &lo, uint16_t &hi) {
  if (floor < 1) floor = 1;
  uint32_t c = TOWER_BST_START + (uint32_t)TOWER_BST_STEP * (floor - 1);
  if (c > TOWER_BST_TOP) c = TOWER_BST_TOP;
  lo = (uint16_t)(c - TOWER_BST_HALF);
  hi = (uint16_t)(c + TOWER_BST_HALF);
}

uint8_t towerIvFor(uint16_t floor) {
  if (floor < 1) floor = 1;
  uint32_t iv = EASY_IV + (uint32_t)(HARD_IV - EASY_IV) * (floor - 1) / TOWER_IV_FLOORS;
  return iv > HARD_IV ? HARD_IV : (uint8_t)iv;
}

bool towerSmartFor(uint16_t floor) { return floor >= TOWER_SMART_FLOOR; }

// Trainer classes, not people: proper nouns, English in every language like the
// species. Every reward floor is the tycoon's.
const char *towerFoeName(uint16_t floor) {
  static const char *const N[] = { "ACE TRAINER", "VETERAN", "EXPERT", "COOLTRAINER", "GENTLEMAN", "BLACK BELT" };
  if (floor && floor % TOWER_REWARD_EVERY == 0) return "TYCOON";
  return N[floor % (sizeof(N) / sizeof(N[0]))];
}

static bool towerEligible(int16_t d, uint16_t lo, uint16_t hi, bool legends) {
  if (!towerDrawable(d)) return false;
  if (!legends && speciesLegendary(d)) return false;
  uint16_t b = speciesBst(d);
  return b >= lo && b <= hi;
}

bool towerTeamFor(uint16_t floor, uint32_t seed, TowerTeam &out) {
  if (floor < 1) floor = 1;
  out.count = 0;
  out.iv = towerIvFor(floor);
  out.smart = towerSmartFor(floor);
  bool legends = floor >= TOWER_LEGEND_FLOOR;
  uint16_t lo, hi;
  towerBand(floor, lo, hi);
  // Widen the band until it can field a whole team: a card with only one small
  // pack must still get a tower, just a less varied one.
  uint16_t n = 0;
  for (int w = 0; w < 12; w++) {
    n = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (towerEligible(d, lo, hi, legends)) n++;
    if (n >= TOWER_TEAM) break;
    lo = lo > 30 ? lo - 30 : 0;
    hi = hi + 30;
  }
  if (!n) return false;
  uint32_t s = seed ^ (0x9E3779B1u * floor);
  if (!s) s = 0x1234567u;
  for (int i = 0; i < 4; i++) xs(s);
  // Bounded: a pool of non-attackers must not hang the screen. Past the bound
  // a pick is taken as it comes rather than leave the floor empty.
  for (int tries = 0; out.count < TOWER_TEAM && out.count < n && tries < 200; tries++) {
    uint16_t pick = (uint16_t)(xs(s) % n), seen = 0;
    int16_t got = 0;
    for (int16_t d = 1; d <= DEX_COUNT && !got; d++)
      if (towerEligible(d, lo, hi, legends) && seen++ == pick) got = d;
    bool dup = false;
    for (uint8_t k = 0; k < out.count; k++) dup |= out.mon[k].dex == (uint16_t)got;
    if (!got || dup) continue;     // n >= count + 1 here, so a fresh one exists
    if (tries < 150 && !canAttackAt(got, TOWER_LEVEL)) continue;
    out.mon[out.count].dex = (uint16_t)got;
    out.mon[out.count].level = TOWER_LEVEL;
    out.count++;
  }
  return out.count > 0;
}

// ---------- rewards ----------
static void rwAdd(TowerReward &r, uint8_t item, uint8_t q) {
  if (r.n < TOWER_REWARD_MAX) { r.item[r.n] = item; r.qty[r.n] = q; r.n++; }
}

// The FIXED part of a floor's pay, deterministic so a test can pin it. Every
// 7th win up to the top: 3 ULTRA BALLs and 2 berries (SITRUS, then LUM
// alternately). Past the top floor: a MASTER BALL every floor instead. The
// held items are rolled on top of this by towerWin().
bool towerRewardFor(uint16_t streak, TowerReward &r) {
  r.n = 0;
  r.avatar = -1;
  if (!streak) return false;
  if (streak > TOWER_TOP_FLOOR) { rwAdd(r, IT_MASTERBALL, 1); return true; }
  if (streak % TOWER_REWARD_EVERY) return false;
  uint16_t k = streak / TOWER_REWARD_EVERY;
  rwAdd(r, IT_ULTRABALL, 3);
  rwAdd(r, (k % 2) ? IT_SITRUS_BERRY : IT_LUM_BERRY, 2);
  return true;
}

uint8_t towerTierFor(uint16_t f, uint8_t roll) {
  roll %= 100;
  if (f < TOWER_TIER_B_FLOOR) return HT_C;
  if (f < TOWER_TIER_A_FLOOR) {
    uint32_t b = 20 + (uint32_t)(f - TOWER_TIER_B_FLOOR) * 40 / (TOWER_TIER_A_FLOOR - 1 - TOWER_TIER_B_FLOOR);
    return roll < b ? HT_B : HT_C;
  }
  if (f >= TOWER_TOP_FLOOR) return roll < 30 ? HT_S : HT_A;   // the top and past it: A/S only
  uint32_t span = TOWER_TOP_FLOOR - 1 - TOWER_TIER_A_FLOOR;
  uint32_t s = 3 + (uint32_t)(f - TOWER_TIER_A_FLOOR) * 12 / span;
  uint32_t a = 10 + (uint32_t)(f - TOWER_TIER_A_FLOOR) * 30 / span;
  if (roll < s) return HT_S;
  if (roll < s + a) return HT_A;
  if (roll < s + a + 45) return HT_B;
  return HT_C;
}

static uint8_t tierWeight(ItemKey k) {
  uint8_t fx = heldFx(k);
  return (fx == HX_PLATE || fx == HX_GEM) ? 1 : 4;
}
ItemKey towerItemOfTier(uint8_t tier, uint32_t roll) {
  uint32_t total = 0;
  for (uint8_t k = 1; k < ITEM_COUNT; k++) if (itemTier(k) == tier) total += tierWeight(k);
  if (!total) return ITEM_NONE;
  roll %= total;
  for (uint8_t k = 1; k < ITEM_COUNT; k++) {
    if (itemTier(k) != tier) continue;
    uint8_t w = tierWeight(k);
    if (roll < w) return k;
    roll -= w;
  }
  return ITEM_NONE;
}
// One held item off `floor`'s table into the reward.
static void rwHeld(TowerReward &r, uint16_t floor) {
  ItemKey k = towerItemOfTier(towerTierFor(floor, (uint8_t)random(100)), (uint32_t)random(0x7FFFFFFF));
  if (k) rwAdd(r, k, 1);
}

static void bagReward(const TowerReward &r) {
  for (uint8_t i = 0; i < r.n; i++) bag.add(r.item[i], r.qty[i]);
}

int16_t towerGrantAvatar() {
  uint16_t left = 0;
  for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) if (!casinoOwns(PK_AVATAR, TOWER_AVATARS[k])) left++;
  if (!left) return -1;
  uint16_t pick = (uint16_t)random(left);
  for (uint16_t k = 0; k < TOWER_AVATAR_COUNT; k++) {
    uint16_t i = TOWER_AVATARS[k];
    if (casinoOwns(PK_AVATAR, i)) continue;
    if (pick-- == 0) return casinoGrantAvatar(i) ? (int16_t)(AVATAR_BASE_COUNT + i) : -1;
  }
  return -1;
}

uint16_t towerWin(TowerReward *got) {
  if (s_streak < TOWER_STREAK_MAX) s_streak++;
  if (s_streak > s_best) s_best = s_streak;
  // Saved BEFORE the reward goes in: a reset between the two loses a reward
  // rather than granting it twice on the next win.
  towerSave();
  fightEnd();
  TowerReward r;
  towerRewardFor(s_streak, r);
  if (s_streak == TOWER_TOP_FLOOR) {
    for (uint8_t i = 0; i < TOWER_TOP_ITEMS; i++) rwHeld(r, s_streak);
  } else if (s_streak > TOWER_TOP_FLOOR) {
    if (s_streak % TOWER_ENDLESS_EVERY == 0) rwHeld(r, s_streak);
  } else if (s_streak % TOWER_REWARD_EVERY == 0 || random(100) < TOWER_HELD_PCT) {
    rwHeld(r, s_streak);
  }
  bool any = r.n > 0;
  if (any) bagReward(r);
  if (got) { if (any) *got = r; else got->n = 0; }
  // After the save, like the items: a reset in between loses the costume
  // rather than handing out two.
  int16_t av = s_streak == TOWER_AVATAR_FLOOR ? towerGrantAvatar() : -1;
  if (got) got->avatar = av;
  gameEvent(EV_TOWER_WIN, (int16_t)s_streak);
  return s_streak;
}

void towerLoss() {
  fightEnd();
  s_teamN = 0;          // the team was this run's; the next run picks again
  s_resumed = false;
  teamSave();
  gameEvent(EV_TOWER_LOSS, (int16_t)s_streak);
  // The next run climbs a different tower. Rolled here, not on START, so the
  // floor-1 team the screen shows before START is the one you then fight.
  s_seed += 0x6D2B79F5u + s_streak;
  xs(s_seed);
  if (!s_seed) s_seed = 1;
  s_streak = 0;
  towerSave();
}

// ---------- gym rematches ----------
uint8_t rematchLevel(uint8_t lvl) {
  uint16_t v = (uint16_t)lvl * 5 / 4 + 10;
  return v > MAX_TRAINER_LEVEL ? MAX_TRAINER_LEVEL : (uint8_t)v;
}

bool rematchUnlockedFor(const Pet &p, uint8_t region) {
  return region < GYM_REGIONS && p.hasBadge(region, TRAINER_COUNT - 1, false);
}
bool rematchUnlocked(uint8_t region) { return s_pet && rematchUnlockedFor(*s_pet, region); }

uint16_t rematchMask(uint8_t region) { return region < GYM_REGIONS ? s_rm[region] : 0; }
bool rematchWon(uint8_t region, uint8_t idx) {
  return idx < TRAINER_COUNT && ((rematchMask(region) >> idx) & 1);
}
bool rematchOpenFor(const Pet &p, uint8_t region, uint8_t idx) {
  if (idx >= TRAINER_COUNT || !rematchUnlockedFor(p, region)) return false;
  return idx == 0 || rematchWon(region, idx - 1);
}

uint8_t rematchTeam(uint8_t region, uint8_t idx, TrainerMon *out) {
  if (region >= GYM_REGIONS || idx >= TRAINER_COUNT) return 0;
  const Trainer &t = TRAINER_SETS[region].list[idx];
  uint8_t n = 0, top = 0, lowest = MAX_TRAINER_LEVEL;
  for (uint8_t k = 0; k < t.count && n < TRAINER_TEAM_MAX; k++) {
    uint8_t lv = rematchLevel(t.team[k].level);
    out[n].dex = (uint16_t)evolveForLevel((int16_t)t.team[k].dex, lv);
    out[n].level = lv;
    if (lv > top) top = lv;
    if (lv < lowest) lowest = lv;
    n++;
  }
  if (!n) return 0;
  // Fill to six with the leader's own type. Candidates are what that species
  // would be at the fill level (so no level-60 CHARMANDER), drawable, never
  // legendary, not already on the team. The six strongest are the shortlist
  // and a per-trainer seed picks among them, so the team is the same every time
  // you look at it and fight it. The ace stays last: fillers go before it.
  uint8_t fillLv = lowest;
  uint32_t s = 0xA5A5u + region * 977u + idx * 131u;
  while (n < TRAINER_TEAM_MAX) {
    int16_t best[6] = { 0 };
    uint16_t bst[6] = { 0 };
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const DexEntry &e = DEX_TBL[d];
      if (e.type1 != t.type && e.type2 != t.type) continue;
      if (!towerDrawable(d) || speciesLegendary(d)) continue;
      if (evolveForLevel(d, fillLv) != d) continue;       // would have evolved
      // and it must be reachable by this level: a stage whose own evolve
      // level is above fillLv is too young to be that form yet
      buildPre();
      int16_t pre = s_pre[d];
      if (pre && DEX_TBL[pre].evolveLevel > fillLv) continue;
      bool dup = false;
      for (uint8_t k = 0; k < n; k++) dup |= out[k].dex == (uint16_t)d;
      if (dup) continue;
      uint16_t b = speciesBst(d);
      if (b <= bst[5]) continue;               // would not make the shortlist anyway
      if (!canAttackAt(d, fillLv)) continue;   // only now: it builds a Pet
      for (int j = 0; j < 6; j++) {
        if (b > bst[j]) {
          for (int m = 5; m > j; m--) { best[m] = best[m - 1]; bst[m] = bst[m - 1]; }
          best[j] = d; bst[j] = b;
          break;
        }
      }
    }
    int have = 0;
    while (have < 6 && best[have]) have++;
    if (!have) break;                    // nothing of that type can be drawn
    int16_t pick = best[xs(s) % have];
    uint8_t at = n - 1;                  // before the ace
    for (uint8_t k = n; k > at; k--) out[k] = out[k - 1];
    out[at].dex = (uint16_t)pick;
    out[at].level = fillLv;
    n++;
  }
  (void)top;
  return n;
}

uint8_t rematchTop(uint8_t region, uint8_t idx, uint8_t *count) {
  TrainerMon tm[TRAINER_TEAM_MAX];
  uint8_t n = rematchTeam(region, idx, tm), top = 0;
  for (uint8_t k = 0; k < n; k++) if (tm[k].level > top) top = tm[k].level;
  if (count) *count = n;
  return top;
}

// Every rematch win: a berry (ORAN from a gym leader, SITRUS from the Elite 4,
// LUM from the champion). The FIRST rematch win of each trainer adds: 2 ULTRA
// BALLs from a gym leader, 3 from the Elite 4, LEFTOVERS from the champion.
bool rematchRewardFor(uint8_t idx, bool first, TowerReward &r) {
  r.n = 0;
  r.avatar = -1;
  if (idx >= TRAINER_COUNT) return false;
  bool champ = idx == TRAINER_COUNT - 1, e4 = idx >= TRAINER_GYMS && !champ;
  rwAdd(r, champ ? IT_LUM_BERRY : e4 ? IT_SITRUS_BERRY : IT_ORAN_BERRY, 1);
  if (first) {
    if (champ) rwAdd(r, IT_LEFTOVERS, 1);
    else rwAdd(r, IT_ULTRABALL, e4 ? 3 : 2);
  }
  return true;
}

bool rematchWin(uint8_t region, uint8_t idx, TowerReward *got) {
  if (region >= GYM_REGIONS || idx >= TRAINER_COUNT) { if (got) got->n = 0; return false; }
  bool first = !rematchWon(region, idx);
  s_rm[region] |= (uint16_t)(1u << idx);
  towerSave();
  TowerReward r;
  rematchRewardFor(idx, first, r);
  bagReward(r);
  if (got) *got = r;
  gameEvent(EV_REMATCH_WIN, idx);
  return first;
}
