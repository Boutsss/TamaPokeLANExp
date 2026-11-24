#include "chain.h"
#include <Preferences.h>
#include "pet.h"
#include "dex.h"
#include "game_events.h"

static Preferences cPrefs;
static bool cOpen = false;
static const Pet *cPet = nullptr;
static int16_t cDex = 0;
static uint8_t cLen = 0, cBest = 0;
static uint32_t cDay = 0;

uint32_t chainShinyMult(uint8_t len) {
  uint32_t m = 1u + len;
  return m > CHAIN_SHINY_CAP ? CHAIN_SHINY_CAP : m;
}

bool chainAlive(uint32_t lastDay, uint32_t today) { return today <= lastDay + 1; }

bool chainEligible(int16_t dex) {
  return dex >= 1 && dex <= DEX_COUNT && DEX_TBL[dex].rarity != R_LEGENDARIO;
}

uint8_t chainLen(uint32_t today) { return cDex && chainAlive(cDay, today) ? cLen : 0; }
int16_t chainDex(uint32_t today) { return chainLen(today) ? cDex : 0; }
uint8_t chainBest() { return cBest; }

int16_t chainPick(uint8_t region, uint32_t today, bool coin) {
  int16_t d = chainDex(today);
  if (!d || !coin || !speciesShowable(d)) return 0;
  if (region < REGION_COUNT && region != REGION_ALL &&
      (d < (int16_t)REGIONS[region].lo || d > (int16_t)REGIONS[region].hi)) return 0;
  return d;
}

static void save() {
  if (!cOpen) return;
  uint8_t b[9] = { 1, (uint8_t)(cDex & 0xFF), (uint8_t)((uint16_t)cDex >> 8), cLen, cBest,
                   (uint8_t)cDay, (uint8_t)(cDay >> 8), (uint8_t)(cDay >> 16), (uint8_t)(cDay >> 24) };
  if (cPrefs.putBytes("wchain", b, sizeof(b)) != sizeof(b)) Serial.println("chain: NVS write failed");
}

uint8_t chainLink(int16_t dex, uint32_t today) {
  if (!chainEligible(dex)) return chainLen(today);
  if (chainLen(today) && cDex == dex) {
    if (cLen < CHAIN_MAX) cLen++;
  } else {
    cDex = dex;
    cLen = 1;
  }
  cDay = today;
  if (cLen > cBest) cBest = cLen;
  save();
  gameEvent(EV_CHAIN, cLen);
  return cLen;
}

static void sink(GameEv ev, int16_t arg) {
  if (!cPet || (ev != EV_WILD_WIN && ev != EV_WILD_CATCH)) return;
  chainLink(arg, cPet->currentDay());
}

void chainBegin(const Pet *p) {
  cPet = p;
  cOpen = cPrefs.begin("tamapoke", false);
  cDex = 0; cLen = cBest = 0; cDay = 0;
  if (cOpen) {
    uint8_t b[9] = { 0 };
    size_t n = cPrefs.getBytesLength("wchain");
    if (n >= sizeof(b) && cPrefs.getBytes("wchain", b, sizeof(b)) == sizeof(b) && b[0] >= 1) {
      cDex = (int16_t)(b[1] | (b[2] << 8));
      cLen = b[3]; cBest = b[4];
      cDay = (uint32_t)b[5] | ((uint32_t)b[6] << 8) | ((uint32_t)b[7] << 16) | ((uint32_t)b[8] << 24);
      if (!chainEligible(cDex) || !cLen) { cDex = 0; cLen = 0; }
      if (cLen > CHAIN_MAX) cLen = CHAIN_MAX;
      if (cBest < cLen) cBest = cLen;
    }
  }
  gameEventSubscribe(sink);
}

void chainReset() {
  gameEventUnsubscribe(sink);
  cOpen = false; cPet = nullptr; cDex = 0; cLen = cBest = 0; cDay = 0;
}
