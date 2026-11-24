#pragma once
#include <Arduino.h>

// Shiny chains on wild encounters. Ported from vunguyen2712/TamaPoke (v3.14),
// re-built on this base's EXPLORE roll.
//
// GAME LOGIC ONLY: links into every headless test next to pet.cpp.
//
// THE RULES (README § "Shiny chains" quotes the same numbers):
//   - Beating OR catching a wild creature links it: the same species again adds
//     one (to CHAIN_MAX), another species restarts the chain on that one.
//   - A whole game day with no link breaks it: alive while today <= day + 1.
//   - Legendaries never start or extend a chain, so none can be farmed.
//   - While a chain is alive, an encounter in the chained species' region is
//     that species half the time (chainBoosts()).
//   - Its shiny odds are multiplied by 1 + length, at most CHAIN_SHINY_CAP --
//     the feature fork's multiplier, applied to this base's flat 1/4096: the
//     best a chain reaches is 1/256, or 1/128 with the SHINY CHARM.
//
// Persisted under "wchain" (in SAVE_FIELDS), append-only bytes:
//   [0] version 1  [1..2] dex  [3] len  [4] best  [5..8] day of the last link
#define CHAIN_MAX 99
#define CHAIN_SHINY_CAP 16

class Pet;

// ---- the rule (pure)
uint32_t chainShinyMult(uint8_t len);            // 1 + len, capped at CHAIN_SHINY_CAP
bool chainAlive(uint32_t lastDay, uint32_t today);
bool chainEligible(int16_t dex);                 // a species that may chain at all

// ---- the state
uint8_t chainLen(uint32_t today);                // 0 once broken
int16_t chainDex(uint32_t today);                // 0 once broken
uint8_t chainBest();
// The chained species, if it should be THIS encounter (one coin flip, passed in
// so the test can pin both sides): alive, in `region`, and showable here.
int16_t chainPick(uint8_t region, uint32_t today, bool coin);
// A wild win or catch. Returns the new length.
uint8_t chainLink(int16_t dex, uint32_t today);

void chainBegin(const Pet *p);   // load from NVS, listen for EV_WILD_WIN / EV_WILD_CATCH
void chainReset();               // tests: forget RAM state (NVS untouched)
