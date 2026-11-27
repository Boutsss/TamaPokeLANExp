#include "stars.h"
#include "pet.h"
#include "tower.h"
#include "trainers.h"
#include "dex_rewards.h"
#include "game_events.h"
#include "nvsinfo.h"
#include <Preferences.h>

// ONE blob under "stars", append-only like every other record here:
// [u8 version][u8 earned bits][u16 lifetime LAN wins][u16 lifetime shiny
// catches (v3.57)]. A shorter (older) blob leaves the newer fields at their
// defaults -- so shiny catches count from the first build that counts them.
#define STARS_KEY "stars"
#define STARS_VER 2

static Preferences s_prefs;
static bool s_opened = false;
static const Pet *s_pet = nullptr;
static uint8_t s_earned = 0;
static uint16_t s_lanWins = 0;
static uint16_t s_shiny = 0;

static void starsSave() {
  if (!s_opened) return;
  uint8_t b[6] = { STARS_VER, s_earned, (uint8_t)(s_lanWins & 0xFF), (uint8_t)(s_lanWins >> 8),
                   (uint8_t)(s_shiny & 0xFF), (uint8_t)(s_shiny >> 8) };
  if (s_prefs.putBytes(STARS_KEY, b, sizeof(b)) != sizeof(b)) logKeyFailure("stars", STARS_KEY);
}

static void latch(uint8_t star) {
  if (star >= STAR_COUNT || (s_earned >> star) & 1) return;
  s_earned |= (uint8_t)(1u << star);
  starsSave();
}

// Every region's ladder -- its gyms, Elite 4 and champion -- beaten on `hard`.
static bool everyLadder(bool hard) {
  const uint16_t full = (uint16_t)((1u << TRAINER_COUNT) - 1);
  for (uint8_t rg = 0; rg < GYM_REGIONS; rg++)
    if ((s_pet->badgeMask(rg, hard) & full) != full) return false;
  return true;
}

// Every region's OBTAINABLE dex -- the Shiny Charm's rule, regionDexComplete():
// what can hatch, or evolve from something that can, with its pack on the card.
// A region whose pack is missing is never complete, so neither is the dex.
static bool everyDex() {
  for (uint8_t r = 0; r < REGION_COUNT; r++) {
    if (r == REGION_ALL) continue;
    if (!regionDexComplete(*s_pet, r)) return false;
  }
  return true;
}

void starsUpdate() {
  if (!s_pet) return;
  if (towerBest() >= STAR_TOWER_FLOORS) latch(STAR_TOWER);
  if (s_lanWins >= STAR_LAN_WINS) latch(STAR_LAN);
  if (s_pet->coins >= STAR_COINS_HELD) latch(STAR_COINS);
  if (s_shiny >= STAR_SHINY_CATCHES) latch(STAR_SHINY);
  if (everyLadder(false)) latch(STAR_LADDER_EASY);
  if (everyLadder(true)) latch(STAR_LADDER_HARD);
  if (everyDex()) latch(STAR_DEX);
}

static void starsOnEvent(GameEv ev, int16_t) {
  if (ev == EV_SHINY_CATCH) {
    if (s_shiny < 0xFFFF) s_shiny++;
    starsSave();
    if (s_shiny >= STAR_SHINY_CATCHES) latch(STAR_SHINY);
    return;
  }
  if (ev != EV_LAN_WIN) return;
  if (s_lanWins < 0xFFFF) s_lanWins++;
  starsSave();
  if (s_lanWins >= STAR_LAN_WINS) latch(STAR_LAN);
}

void starsBegin(const Pet *p) {
  s_pet = p;
  if (!s_opened) s_opened = s_prefs.begin("tamapoke", false);
  if (!s_opened) { Serial.println("stars: NVS would not open"); return; }
  gameEventSubscribe(starsOnEvent);
  s_earned = 0; s_lanWins = 0; s_shiny = 0;
  uint8_t b[8] = { 0 };
  size_t n = s_prefs.getBytesLength(STARS_KEY);
  if (n >= 2 && n <= sizeof(b) && s_prefs.getBytes(STARS_KEY, b, n) == n) {
    s_earned = b[1] & (uint8_t)((1u << STAR_COUNT) - 1);
    if (n >= 4) s_lanWins = (uint16_t)(b[2] | (b[3] << 8));
    if (n >= 6) s_shiny = (uint16_t)(b[4] | (b[5] << 8));
  } else if (p) {
    // First boot with stars: the LAN wins already on record count. rivals[]
    // keeps only the last RIVAL_CAP opponents, so this is a floor, not a total.
    uint32_t w = 0;
    for (int i = 0; i < RIVAL_CAP; i++) w += p->rivals[i].wins;
    s_lanWins = w > 0xFFFF ? 0xFFFF : (uint16_t)w;
    starsSave();
  }
  starsUpdate();
}

uint8_t starsEarned() { return s_earned; }
bool starHas(uint8_t i) { return i < STAR_COUNT && ((s_earned >> i) & 1); }
uint16_t starsLanWins() { return s_lanWins; }
uint16_t starsShinyCatches() { return s_shiny; }

void starsRecheck() {
  s_earned = 0;
  starsSave();
  starsUpdate();
}

void starsClearAll() {
  s_earned = 0; s_lanWins = 0; s_shiny = 0;
  if (s_opened) s_prefs.remove(STARS_KEY);
}
