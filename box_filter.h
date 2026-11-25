#pragma once
#include <Arduino.h>
#include "party.h"
#include "types.h"

// Finding a creature in the box: a FILTER (which ones) and a SORT (in what
// order). GAME LOGIC ONLY -- no gfx -- so the tests ask it directly, and the
// box screen and its test both go through boxView().
//
// Filters: everything, favourites, shinies, those holding an item, or any one
// of the 18 types (a dual type matches either). Sorts: the box's own slot
// order, or best-first by level, IV total, each battle stat, dex number, or
// A-Z by name (the nickname if it has one).

enum : uint8_t { BF_ALL = 0, BF_FAV, BF_SHINY, BF_HELD, BF_TYPE0 };   // BF_TYPE0 + t for type t
// SEARCH: the name (nickname or species) contains the query, typed on the
// keyboard. After the types, so their indices did not move.
#define BF_SEARCH (BF_TYPE0 + TYPE_COUNT)
#define BF_COUNT (BF_SEARCH + 1)
#define BF_QUERY_LEN 12
void boxSearchSet(const char *q);       // the SEARCH filter's query ("" matches everything)
const char *boxSearchQuery();
uint32_t boxSearchGen();                // bumped by every boxSearchSet(), for caches
enum : uint8_t { BS_SLOT = 0, BS_LEVEL, BS_IV, BS_ATK, BS_DEF, BS_SPE, BS_HP, BS_DEX, BS_NAME, BS_COUNT };

bool boxFilterMatch(const PartyMon &m, uint8_t filter);
// The box as the screen should show it, as box indices. The PLAIN view (no
// filter, slot order) lists every slot, empty ones included, because that is
// where a creature can be put; any other view lists only the matching
// creatures, best first. Returns how many.
uint16_t boxView(const Party &p, uint8_t filter, uint8_t sort, uint16_t *out);
inline bool boxViewPlain(uint8_t filter, uint8_t sort) { return filter == BF_ALL && sort == BS_SLOT; }
