#pragma once
#include <Arduino.h>
#include "items.h"

class Pet;

// Pokedex rewards -- the only way the held items and the SHINY CHARM enter the
// game. Ported from vunguyen2712/TamaPoke's items.cpp, adapted to this base:
//
//   - every DEX_GIFT_EVERY registrations  -> one battle item, cycling berries
//     first (dexGiftItem())
//   - a region's OBTAINABLE dex complete  -> SHINY CHARM (once, ever)
//
// OBTAINABLE asks speciesShowable() -- the same single answer the egg pool and
// every evolution use -- so a region whose pack is missing has nothing
// obtainable and can never count as complete. Never keep a second copy of that
// rule here (CLAUDE.md, trap 1).
//
// The OVAL CHARM is deliberately NOT granted: it only means something to a
// daycare, and this branch has none yet. Handing out an item that does nothing
// would read as a bug.
//
// Persisted in two U8 keys, "dxgf" (gifts paid) and "dxrw" (flag bits). Both are
// in SAVE_FIELDS. The counter is only advanced after the bag write has landed,
// so a failed write is retried at the next registration rather than lost; a
// power cut between the two pays a gift twice, which is the harmless direction.
#define DEX_GIFT_EVERY 25
enum : uint8_t { DXRW_SHINY = 1 << 0 };

void dexRewardsBegin(Pet *p);          // subscribe + catch up on an old save; once, from setup()
void dexRewardsCheck(const Pet &p);    // grants whatever is due; idempotent
bool dexObtainable(int16_t dex);
uint16_t regionObtainableCount(uint8_t r);
bool regionDexComplete(const Pet &p, uint8_t r);
uint8_t dexRewardFlags();
uint8_t dexGiftsGranted();
ItemKey dexGiftItem(uint8_t n, uint8_t *qty);  // the n-th gift (0-based)
void dexRewardsReset();                // tests: forget RAM state (NVS untouched)
