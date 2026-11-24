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

bool itemConsumable(ItemKey item);
bool holderExists(const Pet &p, const Party &pt, uint8_t who);
ItemKey heldBy(const Pet &p, const Party &pt, uint8_t who);
bool heldGive(Pet &p, Party &pt, uint8_t who, ItemKey item);
bool heldTake(Pet &p, Party &pt, uint8_t who);
void heldReturn(ItemKey &held);
void heldBattleSync(Pet &p, Party &pt, const Combatant &c);
