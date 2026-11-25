#include "held_items.h"
#include "pet.h"
#include "inventory.h"
#include "battle.h"

static PartyMon *banked(Party &pt, uint16_t who) {
  if (who >= HOLDER_PARTY(0) && who < HOLDER_BOX(0))
    return &pt.slots[who - HOLDER_PARTY(0)];
  if (who >= HOLDER_BOX(0) && who < HOLDER_COUNT)
    return &pt.box[who - HOLDER_BOX(0)];
  return nullptr;
}
static const PartyMon *banked(const Party &pt, uint16_t who) {
  if (who >= HOLDER_PARTY(0) && who < HOLDER_BOX(0))
    return &pt.slots[who - HOLDER_PARTY(0)];
  if (who >= HOLDER_BOX(0) && who < HOLDER_COUNT)
    return &pt.box[who - HOLDER_BOX(0)];
  return nullptr;
}

bool holderExists(const Pet &p, const Party &pt, uint16_t who) {
  if (who == HOLDER_PET) return !p.isEgg();
  const PartyMon *m = banked(pt, who);
  return m && !m->empty();
}
ItemKey heldBy(const Pet &p, const Party &pt, uint16_t who) {
  if (!holderExists(p, pt, who)) return ITEM_NONE;
  if (who == HOLDER_PET) return itemIsHeld(p.held) ? p.held : ITEM_NONE;
  ItemKey item = banked(pt, who)->held;
  return itemIsHeld(item) ? item : ITEM_NONE;
}

static ItemKey *slot(Pet &p, Party &pt, uint16_t who) {
  if (!holderExists(p, pt, who)) return nullptr;
  if (who == HOLDER_PET) return &p.held;
  PartyMon *m = banked(pt, who);
  return m ? &m->held : nullptr;
}
static bool saveHolder(Pet &p, Party &pt, uint16_t who) {
  if (who == HOLDER_PET) return p.saveNow();
  if (who < HOLDER_BOX(0)) return pt.save();
  return pt.boxSave();
}

bool heldGive(Pet &p, Party &pt, uint16_t who, ItemKey item) {
  ItemKey *h = slot(p, pt, who);
  if (!h || !itemIsHeld(item) || !bag.count(item)) return false;
  if (*h == item) return true;
  ItemKey old = *h;
  if (!bag.consume(item, 1)) return false;
  if (old && !bag.addChecked(old, 1)) {
    bag.addChecked(item, 1);
    return false;
  }
  *h = item;
  if (saveHolder(p, pt, who)) return true;
  *h = old;
  if (old) bag.consume(old, 1);
  bag.addChecked(item, 1);
  saveHolder(p, pt, who);
  return false;
}

bool heldTake(Pet &p, Party &pt, uint16_t who) {
  ItemKey *h = slot(p, pt, who);
  if (!h || !itemIsHeld(*h)) return false;
  ItemKey old = *h;
  if (!bag.addChecked(old, 1)) return false;
  *h = ITEM_NONE;
  if (saveHolder(p, pt, who)) return true;
  *h = old;
  bag.consume(old, 1);
  saveHolder(p, pt, who);
  return false;
}

void heldReturn(ItemKey &held) {
  if (itemIsHeld(held)) bag.addChecked(held, 1);
  held = ITEM_NONE;
}

// (v3.38) There is no write-back from a battle any more. A berry, a FOCUS SASH
// and the rest work ONCE PER BATTLE -- Combatant::itemUsed, a battle-only copy --
// and the creature still holds them when the fight ends. Nothing in battle.cpp
// touches the stored holder; held_items_test proves the item is back.

int shinyCharmOdds(int base) {
  if (bag.has(IT_SHINY_CHARM)) {
    base /= 2;
    if (base < 4) base = 4;
  }
  return base < 1 ? 1 : base;
}
