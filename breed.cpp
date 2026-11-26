#include "breed.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "dex.h"
#include "game_events.h"
#include <Preferences.h>
#include <string.h>
#include <stdlib.h>

// See breed.h for the rules and for why the hand-over is ordered as it is.

// ---------------------------------------------------------------- the rule

// The species dex evolves FROM, or 0 for a first form. EEVEE's extra branches
// are not in evolvesTo (a single field) but in EEVEE_EVOS.
static int16_t prevOf(int16_t dex) {
  for (uint8_t i = 0; i < EEVEE_EVO_COUNT; i++)
    if (EEVEE_EVOS[i] == dex) return DEX_EEVEE;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].evolvesTo == (uint16_t)dex) return d;
  return 0;
}

int16_t breedBaseOf(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  int16_t cur = dex;
  for (int guard = 0; guard < 6; guard++) {   // no line is longer than 3
    int16_t p = prevOf(cur);
    if (p < 1) break;
    cur = p;
  }
  return cur;
}

// Any member of the line, walked from its base: PHIONE is legendary while
// MANAPHY (its "evolution" in this table) is not, and either must refuse.
bool breedLineLegendary(int16_t dex) {
  int16_t cur = breedBaseOf(dex);
  for (int guard = 0; cur >= 1 && cur <= DEX_COUNT && guard < 6; guard++) {
    if (DEX_TBL[cur].rarity == R_LEGENDARIO) return true;
    cur = DEX_TBL[cur].evolvesTo;
  }
  return dex >= 1 && dex <= DEX_COUNT && DEX_TBL[dex].rarity == R_LEGENDARIO;
}

static bool shareType(int16_t a, int16_t b) {
  const DexEntry &x = DEX_TBL[a], &y = DEX_TBL[b];
  uint8_t tx[2] = { x.type1, x.type2 }, ty[2] = { y.type1, y.type2 };
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++)
      if (tx[i] != T_NONE && tx[i] == ty[j]) return true;
  return false;
}

// What the ordinary egg pool would accept: a hatchable rarity, and
// speciesShowable() -- the single answer the egg pool and every evolution ask.
// Never a second copy of that rule (CLAUDE.md, trap 1).
bool breedEggable(int16_t d) {
  if (d < 1 || d > DEX_COUNT) return false;
  uint8_t r = DEX_TBL[d].rarity;
  if (r != R_COMUN && r != R_RARO) return false;
  return speciesShowable(d);
}

static bool validMon(const PartyMon &m) { return m.dex >= 1 && m.dex <= DEX_COUNT; }

BreedVerdict breedCheck(const PartyMon &a, const PartyMon &b) {
  if (!validMon(a) || !validMon(b)) return BV_EMPTY;
  if (&a == &b) return BV_SAME;
  bool da = a.dex == BREED_DITTO, db = b.dex == BREED_DITTO;
  if (da && db) return BV_TWO_DITTO;
  if (breedLineLegendary(a.dex) || breedLineLegendary(b.dex)) return BV_LEGEND;
  if (!da && !db && !shareType(a.dex, b.dex)) return BV_NO_TYPE;
  int16_t egg = breedBaseOf(da ? b.dex : a.dex);
  if (!breedEggable(egg)) return BV_NO_EGG;
  return BV_OK;
}

int16_t breedEggSpecies(const PartyMon &a, const PartyMon &b) {
  if (breedCheck(a, b) != BV_OK) return 0;
  return breedBaseOf(a.dex == BREED_DITTO ? b.dex : a.dex);
}

void breedInherit(const PartyMon &a, const PartyMon &b, bool ovalCharm, uint8_t out[4]) {
  const uint8_t pa[4] = { a.ivAtk, a.ivDef, a.ivSpe, a.ivHp };
  const uint8_t pb[4] = { b.ivAtk, b.ivDef, b.ivSpe, b.ivHp };
  if (ovalCharm) {   // all four, the better parent each time
    for (int i = 0; i < 4; i++) out[i] = pa[i] > pb[i] ? pa[i] : pb[i];
  } else {           // BREED_INHERIT of the four, each from a random parent
    uint8_t order[4] = { 0, 1, 2, 3 };
    for (int k = 3; k > 0; k--) {
      int j = random(k + 1);
      uint8_t t = order[k]; order[k] = order[j]; order[j] = t;
    }
    for (int i = 0; i < 4; i++) out[i] = BREED_IV_ROLL;
    for (int k = 0; k < BREED_INHERIT; k++) {
      uint8_t s = order[k];
      out[s] = random(2) ? pa[s] : pb[s];
    }
  }
  // A parent's IV is a stored byte: never trust it past the range a hatch gives.
  for (int i = 0; i < 4; i++)
    if (out[i] != BREED_IV_ROLL && out[i] > 31) out[i] = 31;
}

// ---------------------------------------------------------------- the state

static BredEgg gQueued;           // made at the daycare, waiting for newEgg()
static BredEgg gLive;             // taken by newEgg(): the egg on screen now
static uint32_t gLastDay = 0;
static bool gBredEver = false;    // day 0 (no clock) still allows a first breed
static bool gOpened = false;      // never write NVS unless begun
static Preferences gPrefs;

// "dcare" layout, little-endian, APPEND-ONLY (vunguyen2712's v1, unchanged, so
// the two forks read each other's daycare):
//  [0] version 1
//  [1..2] queued dex  [3..6] queued IVs  [7] queued shiny  [8..9] pa  [10..11] pb
//  [12..13] live dex  [14..17] live IVs
//  [18..21] last breed day  [22] bred ever
#define DC_LEN 23

static void put16(uint8_t *p, int16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((uint16_t)v >> 8); }
static int16_t get16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }

static bool saveState() {
  if (!gOpened) return false;
  uint8_t b[DC_LEN] = { 0 };
  b[0] = 1;
  put16(b + 1, gQueued.dex);
  memcpy(b + 3, gQueued.iv, 4);
  b[7] = gQueued.shiny;
  put16(b + 8, gQueued.pa);
  put16(b + 10, gQueued.pb);
  put16(b + 12, gLive.dex);
  memcpy(b + 14, gLive.iv, 4);
  for (int i = 0; i < 4; i++) b[18 + i] = (uint8_t)(gLastDay >> (8 * i));
  b[22] = gBredEver ? 1 : 0;
  if (gPrefs.putBytes("dcare", b, sizeof(b)) != sizeof(b)) {
    Serial.println("daycare: NVS write failed");
    return false;
  }
  return true;
}

static bool sane(BredEgg &e) {
  if (e.dex < 1 || e.dex > DEX_COUNT) { e = BredEgg(); return false; }
  for (int i = 0; i < 4; i++)
    if (e.iv[i] != BREED_IV_ROLL && e.iv[i] > 31) e.iv[i] = 31;
  if (e.pa < 0 || e.pa > DEX_COUNT) e.pa = 0;
  if (e.pb < 0 || e.pb > DEX_COUNT) e.pb = 0;
  e.shiny = e.shiny ? 1 : 0;
  return true;
}

static void load() {
  gQueued = BredEgg();
  gLive = BredEgg();
  gLastDay = 0;
  gBredEver = false;
  uint8_t b[DC_LEN] = { 0 };
  size_t have = gPrefs.getBytesLength("dcare");
  if (!have) return;
  if (have <= sizeof(b)) gPrefs.getBytes("dcare", b, have);
  else {   // a longer, newer layout: keep the prefix we understand
    uint8_t *tmp = (uint8_t *)malloc(have);
    if (!tmp) return;
    if (gPrefs.getBytes("dcare", tmp, have) == have) memcpy(b, tmp, sizeof(b));
    free(tmp);
  }
  if (b[0] < 1) return;
  gQueued.dex = get16(b + 1);
  memcpy(gQueued.iv, b + 3, 4);
  gQueued.shiny = b[7];
  gQueued.pa = get16(b + 8);
  gQueued.pb = get16(b + 10);
  sane(gQueued);
  gLive.dex = get16(b + 12);
  memcpy(gLive.iv, b + 14, 4);
  sane(gLive);
  gLastDay = (uint32_t)b[18] | ((uint32_t)b[19] << 8) | ((uint32_t)b[20] << 16) | ((uint32_t)b[21] << 24);
  gBredEver = b[22] != 0;
}

void breedBegin(const Pet *p) {
  gOpened = gPrefs.begin("tamapoke", false);
  if (!gOpened) { Serial.println("daycare: NVS would not open"); return; }
  load();
  // Finish a hand-over a power cut interrupted between newEgg()'s checkpoint
  // and breedCommitTaken(): the queued egg is already the live, uncracked egg.
  if (p && breedHasQueued() && p->isEgg() && p->eggProgress() == 0 &&
      p->eggPeek() == gQueued.dex)
    breedCommitTaken();
}

void breedReset() {
  gQueued = BredEgg();
  gLive = BredEgg();
  gLastDay = 0;
  gBredEver = false;
  gOpened = false;
}

const PartyMon *breedBanked(uint16_t idx) {
  if (idx < PARTY_SLOTS) return &party.slots[idx];
  if (idx < BREED_BANK) return &party.box[idx - PARTY_SLOTS];
  return nullptr;
}

BreedVerdict breedCheckIdx(uint16_t ia, uint16_t ib) {
  const PartyMon *a = breedBanked(ia), *b = breedBanked(ib);
  if (!a || !b) return BV_EMPTY;
  return breedCheck(*a, *b);
}

bool breedPossible() {
  if (breedHasQueued()) return false;
  // Only the occupied ones: with a 1200-slot box, every PAIR of positions is
  // ~727,000 checks, almost all of them on empty slots.
  static uint16_t occ[BREED_BANK];
  uint16_t n = 0;
  for (uint16_t i = 0; i < BREED_BANK; i++) {
    const PartyMon *m = breedBanked(i);
    if (m && !m->empty()) occ[n++] = i;
  }
  for (uint16_t a = 0; a < n; a++)
    for (uint16_t b = a + 1; b < n; b++)
      if (breedCheckIdx(occ[a], occ[b]) == BV_OK) return true;
  return false;
}

bool breedOnCooldown(uint32_t day) { return gBredEver && day == gLastDay; }

bool breedHasQueued() { return gQueued.dex >= 1; }
const BredEgg &breedQueued() { return gQueued; }
const BredEgg &breedLive() { return gLive; }
bool breedQueuedHatchable() { return breedHasQueued() && breedEggable(gQueued.dex); }

BreedResult breedMake(uint16_t ia, uint16_t ib, uint32_t day) {
  if (!gOpened) return BR_NOT_READY;
  if (breedCheckIdx(ia, ib) != BV_OK) return BR_BAD_PAIR;
  if (breedHasQueued()) return BR_QUEUE_FULL;
  if (breedOnCooldown(day)) return BR_COOLDOWN;
  const PartyMon &a = *breedBanked(ia), &b = *breedBanked(ib);
  BredEgg e;
  e.dex = breedEggSpecies(a, b);
  breedInherit(a, b, bag.has(IT_OVAL_CHARM), e.iv);
  e.shiny = (a.shiny && b.shiny && random(BREED_SHINY_BONUS) == 0) ? 1 : 0;
  e.pa = a.dex;
  e.pb = b.dex;
  const BredEgg oldQ = gQueued;
  const uint32_t oldDay = gLastDay;
  const bool oldEver = gBredEver;
  gQueued = e;
  gLastDay = day;
  gBredEver = true;
  if (!saveState()) {   // RAM never claims an egg that flash does not hold
    gQueued = oldQ; gLastDay = oldDay; gBredEver = oldEver;
    return BR_SAVE_FAILED;
  }
  gameEvent(EV_BREED, e.dex);
  return BR_OK;
}

bool breedCanSwapIn(const Pet &p) {
  return gOpened && breedQueuedHatchable() && p.isEgg() && p.eggProgress() == 0 &&
         p.ceremony == CER_NONE && !p.awaitingStarter();
}

// Same order as newEgg(): the egg's checkpoint first, the queue after.
bool breedSwapIn(Pet &p) {
  if (!breedCanSwapIn(p)) return false;
  if (!p.eggSwapTo(gQueued.dex, gQueued.shiny != 0)) return false;
  breedCommitTaken();
  return true;
}

bool breedPeekQueued(int16_t &dex, bool &shiny) {
  // A species whose pack has left the card waits in the queue instead of
  // hatching as a number; the daycare screen says why.
  if (!gOpened || !breedQueuedHatchable()) return false;
  dex = gQueued.dex;
  shiny = gQueued.shiny != 0;
  return true;
}

void breedCommitTaken() {
  if (!gOpened || !breedHasQueued()) return;
  gLive = gQueued;
  gQueued = BredEgg();
  saveState();
}

void breedForgetLive() {
  if (!gOpened || gLive.dex < 1) return;
  gLive = BredEgg();
  saveState();
}

bool breedTakeLive(int16_t dex, uint8_t out[4]) {
  if (!gOpened || gLive.dex < 1 || gLive.dex != dex) return false;
  memcpy(out, gLive.iv, 4);
  gLive = BredEgg();
  saveState();
  return true;
}
