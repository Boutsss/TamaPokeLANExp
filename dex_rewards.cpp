#include "dex_rewards.h"
#include <Preferences.h>
#include <string.h>
#include "pet.h"
#include "dex.h"
#include "inventory.h"
#include "game_events.h"

static Preferences rwPrefs;
static bool rwOpen = false;
static uint8_t rwFlags = 0, rwGifts = 0;
static Pet *rwPet = nullptr;

ItemKey dexGiftItem(uint8_t n, uint8_t *qty) {
  static const uint8_t GIFT[][2] = {
    { IT_ORAN_BERRY, 3 }, { IT_SITRUS_BERRY, 2 }, { IT_LUM_BERRY, 2 },
    { IT_QUICK_CLAW, 1 }, { IT_FOCUS_SASH, 1 },   { IT_LEFTOVERS, 1 },
  };
  const uint8_t k = sizeof(GIFT) / sizeof(GIFT[0]);
  if (qty) *qty = GIFT[n % k][1];
  return GIFT[n % k][0];
}

// One bit per species: can this card's player actually register it? A
// fixpoint, because an evolution can point BACKWARDS in the dex (HAPPINY 440 ->
// CHANSEY 113) and a chain can be several links long.
#define OBT_BYTES ((DEX_COUNT + 8) / 8)
static void obtainableBits(uint8_t *bits) {
  memset(bits, 0, OBT_BYTES);
  auto set = [&](int16_t d) -> bool {
    if (!speciesShowable(d)) return false;
    uint8_t &b = bits[d >> 3];
    uint8_t m = (uint8_t)(1 << (d & 7));
    if (b & m) return false;
    b |= m;
    return true;
  };
  auto has = [&](int16_t d) { return (bits[d >> 3] >> (d & 7)) & 1; };
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].rarity != R_EVO) set(d);   // hatches from an egg
  for (bool grew = true; grew;) {
    grew = false;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      if (!has(d)) continue;
      if (DEX_TBL[d].evolvesTo) grew |= set((int16_t)DEX_TBL[d].evolvesTo);
      if (d == DEX_EEVEE)
        for (uint8_t i = 0; i < EEVEE_EVO_COUNT; i++) grew |= set(EEVEE_EVOS[i]);
    }
  }
}

bool dexObtainable(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return false;
  uint8_t bits[OBT_BYTES];
  obtainableBits(bits);
  return (bits[dex >> 3] >> (dex & 7)) & 1;
}

uint16_t regionObtainableCount(uint8_t r) {
  if (r >= REGION_COUNT || r == REGION_ALL) return 0;
  uint8_t bits[OBT_BYTES];
  obtainableBits(bits);
  uint16_t n = 0;
  for (int16_t d = REGIONS[r].lo; d <= (int16_t)REGIONS[r].hi && d <= DEX_COUNT; d++)
    if ((bits[d >> 3] >> (d & 7)) & 1) n++;
  return n;
}

static bool regionCompleteWith(const Pet &p, uint8_t r, const uint8_t *bits) {
  if (r >= REGION_COUNT || r == REGION_ALL) return false;
  uint16_t need = 0;
  for (int16_t d = REGIONS[r].lo; d <= (int16_t)REGIONS[r].hi && d <= DEX_COUNT; d++) {
    if (!((bits[d >> 3] >> (d & 7)) & 1)) continue;
    need++;
    if (!p.isRegistered(d)) return false;
  }
  return need > 0;     // nothing obtainable (pack missing) is never "complete"
}

bool regionDexComplete(const Pet &p, uint8_t r) {
  uint8_t bits[OBT_BYTES];
  obtainableBits(bits);
  return regionCompleteWith(p, r, bits);
}

uint8_t dexRewardFlags() { return rwFlags; }
uint8_t dexGiftsGranted() { return rwGifts; }

static bool rwSave() {
  if (!rwOpen) return false;
  bool ok = rwPrefs.putUChar("dxrw", rwFlags) == 1;
  ok = rwPrefs.putUChar("dxgf", rwGifts) == 1 && ok;
  if (!ok) Serial.println("dexrw: NVS write failed");
  return ok;
}

// A grant "landed" if the bag took it, or if the bag could never take it (the
// stack is already full, or a key item is already owned) -- in which case
// retrying forever would stall every later reward behind it.
static bool grant(ItemKey it, uint8_t qty) {
  if (bag.add(it, qty)) return true;
  if (itemIsKey(it)) return bag.has(it);
  return bag.count(it) >= BAG_STACK_MAX;
}

void dexRewardsCheck(const Pet &p) {
  if (!rwOpen) return;
  uint16_t n = p.registeredCount();
  bool changed = false;
  while ((uint16_t)(rwGifts + 1) * DEX_GIFT_EVERY <= n && rwGifts < 255) {
    uint8_t qty = 1;
    ItemKey it = dexGiftItem(rwGifts, &qty);
    if (!grant(it, qty)) break;     // write failed: try again next registration
    rwGifts++;
    changed = true;
  }
  if (!(rwFlags & DXRW_OVAL) && n >= DEX_OVAL_AT) {
    if (grant(IT_OVAL_CHARM, 1)) { rwFlags |= DXRW_OVAL; changed = true; }
  }
  if (!(rwFlags & DXRW_SHINY)) {
    uint8_t bits[OBT_BYTES];
    obtainableBits(bits);
    for (uint8_t r = 0; r < REGION_COUNT; r++) {
      if (!regionCompleteWith(p, r, bits)) continue;
      if (grant(IT_SHINY_CHARM, 1)) { rwFlags |= DXRW_SHINY; changed = true; }
      break;
    }
  }
  if (changed) rwSave();
}

static void dexRewardSink(GameEv ev, int16_t) {
  if (ev == EV_DEX_REGISTER && rwPet) dexRewardsCheck(*rwPet);
}

void dexRewardsBegin(Pet *p) {
  rwOpen = rwPrefs.begin("tamapoke", false);
  if (!rwOpen) { Serial.println("dexrw: NVS would not open"); return; }
  rwFlags = rwPrefs.getUChar("dxrw", 0) & (DXRW_SHINY | DXRW_OVAL);
  rwGifts = rwPrefs.getUChar("dxgf", 0);
  rwPet = p;
  gameEventSubscribe(dexRewardSink);
  if (p) dexRewardsCheck(*p);     // an existing save may already have earned some
}

void dexRewardsReset() {
  gameEventUnsubscribe(dexRewardSink);
  rwFlags = 0; rwGifts = 0; rwPet = nullptr; rwOpen = false;
}
