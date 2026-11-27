#pragma once
#include "items.h"
#include "party.h"

class Pet;
struct Combatant;

#define HOLDER_PET 0
#define HOLDER_PARTY(i) (1 + (i))
#define HOLDER_BOX(i) (1 + PARTY_SLOTS + (i))
#define HOLDER_COUNT (1 + PARTY_SLOTS + BOX_SLOTS)
#define HOLDER_NONE (-1)

bool holderExists(const Pet &p, const Party &pt, uint16_t who);
ItemKey heldBy(const Pet &p, const Party &pt, uint16_t who);
bool heldGive(Pet &p, Party &pt, uint16_t who, ItemKey item);
bool heldTake(Pet &p, Party &pt, uint16_t who);
void heldReturn(ItemKey &held);
int shinyCharmOdds(int base);
// One line saying what an item does, for the bag (v3.57). GENERATED from the
// item's own effect code and numbers in ITEM_TBL -- what battle.cpp reads --
// so it cannot drift from the game. English and ASCII, like the item names;
// at most ITEM_DESC_MAX characters, which fits under a bag row's name.
#define ITEM_DESC_MAX 46
void itemDescribe(ItemKey k, char *out, size_t n);
